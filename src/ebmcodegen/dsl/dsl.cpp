/*license*/
#include "dsl.h"
#include "code/code_writer.h"
#include "escape/escape.h"
#include "helper/defer_ex.h"
#include "strutil/strutil.h"
#include <set>
#include <vector>
#include "../stub/url.hpp"
namespace ebmcodegen::dsl {

    ebmgen::expected<std::string> generate_dsl_output(std::string_view file_name, std::string_view source) {
        DSLContext ctx;
        auto seq = futils::make_ref_seq(source);
        auto res = syntax::dsl(seq, ctx, 0);
        if (res != futils::comb2::Status::match) {
            return ebmgen::unexpect_error("Failed to parse DSL source: {}", ctx.errbuf);
        }
        futils::code::CodeWriter<std::string> w;
        w.writeln("// Generated from ", file_name, " by ebmcodegen at ", repo_url, "; DO NOT EDIT");
        w.writeln("CodeWriter w;");
        bool expected_indent = false;
        size_t indent_level = 0;
        // first, insert indent nodes if only lines
        std::vector<DSLNode> new_nodes;
        for (size_t i = 0; i < ctx.nodes.size(); i++) {
            if (i == 0 && ctx.nodes[i].kind != syntax::OutputKind::Indent) {
                // insert indent at beginning
                DSLNode indent_node;
                indent_node.kind = syntax::OutputKind::Indent;
                indent_node.content = "";
                new_nodes.push_back(std::move(indent_node));
            }
            if (ctx.nodes[i].kind == syntax::OutputKind::Line) {
                expected_indent = true;
            }
            else if (expected_indent) {
                if (ctx.nodes[i].kind != syntax::OutputKind::Indent) {
                    // insert indent after line
                    DSLNode indent_node;
                    indent_node.kind = syntax::OutputKind::Indent;
                    indent_node.content = "";
                    new_nodes.push_back(std::move(indent_node));
                }
                expected_indent = false;
            }
            new_nodes.push_back(std::move(ctx.nodes[i]));
        }
        std::vector<size_t> indent_levels{0};
        std::set<size_t> indent_meaningful_levels;
        std::vector<DSLNode> finalized_nodes;
        // next, insert dedent at change point
        for (size_t i = 0; i < new_nodes.size(); i++) {
            if (new_nodes[i].kind == syntax::OutputKind::Indent) {
                if (i == new_nodes.size() - 1) {
                    continue;  // skip final indent
                }
                if (new_nodes[i + 1].kind == syntax::OutputKind::CppLiteral ||
                    new_nodes[i + 1].kind == syntax::OutputKind::CppSpecialMarker) {
                    continue;
                }
                if (new_nodes[i].content.size() > indent_levels.back()) {
                    indent_levels.push_back(new_nodes[i].content.size());  // increase indent
                    finalized_nodes.push_back(std::move(new_nodes[i]));
                }
                else if (new_nodes[i].content.size() < indent_levels.back()) {
                    // decrease indent
                    while (new_nodes[i].content.size() < indent_levels.back()) {
                        indent_levels.pop_back();
                        if (indent_levels.empty() || new_nodes[i].content.size() > indent_levels.back()) {
                            return ebmgen::unexpect_error("inconsistent indent");
                        }
                        finalized_nodes.push_back(DSLNode{.kind = syntax::OutputKind::Dedent, .content = ""});
                    }
                }
                else {
                    // same level, do nothing
                }
            }
            else {
                finalized_nodes.push_back(std::move(new_nodes[i]));
            }
        }
        while (indent_levels.size() > 1) {
            indent_levels.pop_back();
            finalized_nodes.push_back(DSLNode{.kind = syntax::OutputKind::Dedent, .content = ""});
        }
        std::vector<futils::helper::DynDefer> defers;
        bool prev_was_literal = false;
        size_t line = 1;
        auto make_dsl_line_comment = [&](size_t line) {
            return std::format("/* DSL line {} */", line);
        };
        auto trim = [](std::string_view str) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                start++;
            }
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
                end--;
            }
            return str.substr(start, end - start);
        };
        std::vector<std::string> dsl_lines;
        for (const auto& node : finalized_nodes) {
            switch (node.kind) {
                case syntax::OutputKind::CppLiteral: {
                    w.writeln(make_dsl_line_comment(line));
                    w.write_unformatted(node.content);
                    w.writeln();
                    prev_was_literal = true;
                    line += futils::strutil::count(node.content, "\n");
                    w.writeln(make_dsl_line_comment(line));
                    continue;
                }
                case syntax::OutputKind::CppSpecialMarker: {
                    auto content = trim(node.content);
                    if (content == "transfer_and_reset_writer") {
                        w.writeln("{");
                        auto scope = w.indent_scope();
                        w.writeln("MAYBE(got_writer, get_writer()); ", make_dsl_line_comment(line));
                        w.writeln("got_writer.write(std::move(w)); ", make_dsl_line_comment(line));
                        w.writeln("w.reset(); ", make_dsl_line_comment(line));
                        scope.execute();
                        w.writeln("}", make_dsl_line_comment(line));
                    }
                    else {
                        return ebmgen::unexpect_error("unknown special marker: {}", content);
                    }
                    prev_was_literal = true;
                    line += futils::strutil::count(node.content, "\n");
                    break;
                }
                case syntax::OutputKind::CppIdentifierGetter:
                case syntax::OutputKind::CppVisitedNode: {
                    auto handle_with_cached = [&](auto&& handle) {
                        w.writeln("{");
                        auto scope = w.indent_scope();
                        std::string temp_var = std::format("tmp");
                        handle(temp_var, false);
                        handle(temp_var, true);
                        scope.execute();
                        w.writeln("}", make_dsl_line_comment(line));
                    };
                    if (node.kind == syntax::OutputKind::CppIdentifierGetter) {
                        handle_with_cached([&](auto&& tmp, bool is_write) {
                            if (!is_write) {
                                w.writeln("auto ", tmp, " = get_identifier_or(", node.content, "); ", make_dsl_line_comment(line));
                            }
                            else {
                                w.writeln("w.write(", tmp, "); ", make_dsl_line_comment(line));
                            }
                        });
                    }
                    else {
                        handle_with_cached([&](auto&& tmp, bool is_write) {
                            if (!is_write) {
                                w.writeln("MAYBE(", tmp, ", visit_Object(*this,", node.content, ")); ", make_dsl_line_comment(line));
                            }
                            else {
                                w.writeln("w.write(", tmp, ".to_writer()); ", make_dsl_line_comment(line));
                            }
                        });
                    }
                    line += futils::strutil::count(node.content, "\n");
                    break;
                }
                case syntax::OutputKind::CppWrite: {
                    w.writeln("w.write(", node.content, "); ", make_dsl_line_comment(line));
                    line += futils::strutil::count(node.content, "\n");
                    break;
                }
                case syntax::OutputKind::TargetLang: {
                    auto escaped = futils::escape::escape_str<std::string>(node.content);
                    w.writeln("w.write(\"", escaped, "\"); ", make_dsl_line_comment(line));
                    break;
                }
                case syntax::OutputKind::Indent: {
                    w.writeln("{");
                    auto indent_scope = w.indent_scope_ex();
                    w.writeln("auto indent_scope = w.indent_scope(); ", make_dsl_line_comment(line));
                    defers.emplace_back([&w, scope = std::move(indent_scope)]() mutable {
                        scope.execute();
                        w.writeln("}");
                    });
                    break;
                }
                case syntax::OutputKind::Dedent: {
                    if (defers.empty()) {
                        return ebmgen::unexpect_error("inconsistent indent");
                    }
                    defers.pop_back();
                    break;
                }
                case syntax::OutputKind::Line: {
                    if (!prev_was_literal) {
                        w.writeln("w.writeln();", make_dsl_line_comment(line));
                    }
                    line++;
                    break;
                }
            }
            prev_was_literal = false;
        }
        w.writeln("return w;");
        return w.out();
    }
}  // namespace ebmcodegen::dsl
