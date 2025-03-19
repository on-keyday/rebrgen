/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <bm2/context.hpp>
#include <bmgen/helper.hpp>
#include <wrap/cout.h>
#include <strutil/splits.h>
#include <file/file_view.h>
#include <json/convert_json.h>
#include <json/json_export.h>
#include "hook_list.hpp"
#include <filesystem>
#include <env/env.h>
#include "flags.hpp"
#include "define.hpp"
#include "hook_load.hpp"
#include "glue_code.hpp"
#include "generate.hpp"

namespace rebgn {
    std::string unimplemented_comment(Flags& flags, const std::string& op) {
        return "std::format(\"{}{}{}\",\"" + flags.comment_prefix + "\",to_string(" + op + "),\"" + flags.comment_suffix + "\")";
    }

    void write_impl_template(bm2::TmpCodeWriter& w, Flags& flags) {
        bm2::TmpCodeWriter add_parameter;
        bm2::TmpCodeWriter add_call_parameter;
        bm2::TmpCodeWriter inner_block;
        bm2::TmpCodeWriter inner_function;
        bm2::TmpCodeWriter field_accessor;
        bm2::TmpCodeWriter type_accessor;
        bm2::TmpCodeWriter eval_result;
        bm2::TmpCodeWriter eval;
        bm2::TmpCodeWriter type_to_string;

        write_eval_result(eval_result, flags);
        w.write_unformatted(eval_result.out());
        write_parameter_func(w, add_parameter, add_call_parameter, flags);
        write_inner_block_func(w, inner_block, flags);
        write_inner_function_func(w, inner_function, flags);
        write_field_accessor_func(w, field_accessor, type_accessor, flags);
        write_eval_func(w, eval, flags);
        write_type_to_string_func(w, type_to_string, flags);

        w.write_unformatted(type_to_string.out());
        w.write_unformatted(field_accessor.out());
        w.write_unformatted(type_accessor.out());
        w.write_unformatted(eval.out());
        w.write_unformatted(add_parameter.out());
        w.write_unformatted(add_call_parameter.out());
        w.write_unformatted(inner_block.out());
        w.write_unformatted(inner_function.out());
    }

    bool may_load_config(Flags& flags) {
        if (!flags.config_file.empty()) {
            futils::file::View config_file;
            if (auto res = config_file.open(flags.config_file); res) {
                auto parsed = futils::json::parse<futils::json::JSON>(config_file, true);
                if (parsed.is_undef()) {
                    futils::wrap::cerr_wrap() << "failed to parse json\n";
                    return false;
                }
                if (!futils::json::convert_from_json(parsed, flags)) {
                    futils::wrap::cerr_wrap() << "failed to convert json to flags\n";
                    return false;
                }
            }
        }
        return true;
    }

    void write_code_template(bm2::TmpCodeWriter& w, Flags& flags) {
        if (!may_load_config(flags)) {
            return;
        }
        if (flags.mode == bm2::GenerateMode::config_) {
            write_code_config(w, flags);
            return;
        }
        if (flags.requires_lang_option()) {
            if (flags.lang_name.empty()) {
                futils::wrap::cerr_wrap() << "--lang option is required\n";
                return;
            }
        }
        if (flags.mode == bm2::GenerateMode::header) {
            write_code_header(w, flags);
            return;
        }
        if (flags.mode == bm2::GenerateMode::main) {
            code_main(w, flags);
            return;
        }
        if (flags.mode == bm2::GenerateMode::cmake) {
            write_code_cmake(w, flags);
            return;
        }
        if (flags.mode == bm2::GenerateMode::js_worker) {
            write_code_js_glue_worker(w, flags);
            return;
        }
        if (flags.mode == bm2::GenerateMode::js_ui || flags.mode == bm2::GenerateMode::js_ui_embed) {
            write_code_js_glue_ui_and_generator_call(w, flags);
            return;
        }

        w.writeln("/*license*/");
        w.writeln("#include <bm2/context.hpp>");
        w.writeln("#include <bmgen/helper.hpp>");
        w.writeln("#include <escape/escape.h>");
        may_write_from_hook(w, flags, bm2::HookFile::generator_top, true);
        w.writeln("#include \"bm2", flags.lang_name, ".hpp\"");
        w.writeln("namespace bm2", flags.lang_name, " {");
        auto scope = w.indent_scope();
        w.writeln("using TmpCodeWriter = bm2::TmpCodeWriter;");
        w.writeln("struct Context : bm2::Context {");
        auto scope_context = w.indent_scope();
        may_write_from_hook(w, flags, bm2::HookFile::bm_context, false);
        w.writeln("Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,\"r\",\"w\",\"", flags.self_ident, "\", std::move(escape_ident)} {}");
        scope_context.execute();
        w.writeln("};");

        write_impl_template(w, flags);

        w.writeln("std::string escape_", flags.lang_name, "_keyword(const std::string& str) {");
        auto scope_escape_ident = w.indent_scope();
        w.write("if (");
        if (!may_write_from_hook(flags, bm2::HookFile::keyword, [&](size_t i, futils::view::rvec keyword) {
                if (i != 0) {
                    w.write("||");
                }
                w.write("str == \"", keyword, "\"");
            })) {
            w.writeln("str == \"if\" || str == \"for\" || str == \"else\" || str == \"break\" || str == \"continue\"");
        }
        w.writeln(") {");
        w.indent_writeln("return str + \"_\";");
        w.writeln("}");
        w.writeln("return str;");
        scope_escape_ident.execute();
        w.writeln("}");

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {");
        auto scope_to_xxx = w.indent_scope();
        w.writeln("Context ctx{w, bm, [&](bm2::Context& ctx, std::uint64_t id, auto&& str) {");
        auto scope_escape_key_ident = w.indent_scope();
        w.writeln("auto& code = ctx.ref(rebgn::Varint{id});");
        may_write_from_hook(w, flags, bm2::HookFile::escape_ident, false);
        w.writeln("return escape_", flags.lang_name, "_keyword(str);");
        scope_escape_key_ident.execute();
        w.writeln("}};");
        w.writeln("// search metadata");
        futils::helper::DynDefer first_scan;
        auto add_first_scan = [&] {
            if (first_scan) {
                return;
            }
            w.writeln("for (size_t i = 0; i < bm.code.size(); i++) {");
            auto nest_scope = w.indent_scope();
            w.writeln("auto& code = bm.code[i];");
            w.writeln("switch (code.op) {");
            auto nest_scope2 = w.indent_scope();
            first_scan = [&w, nest_scope = std::move(nest_scope), nest_scope2 = std::move(nest_scope2)]() mutable {
                w.writeln("default: {}");
                nest_scope2.execute();
                w.writeln("}");
                nest_scope.execute();
                w.writeln("}");
                w.writeln("OUT_FIRST_SCAN:;");
            };
        };
        may_write_from_hook(w, flags, bm2::HookFile::first_scan, bm2::HookFileSub::before);
        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            bm2::TmpCodeWriter tmp2;
            if (may_write_from_hook(tmp2, flags, bm2::HookFile::first_scan, op)) {
                add_first_scan();
                w.writeln("case rebgn::AbstractOp::", to_string(op), ": {");
                auto scope = w.indent_scope();
                if (op == AbstractOp::METADATA) {
                    w.writeln("auto meta = code.metadata();");
                    w.writeln("auto name = ctx.metadata_table[meta->name.value()];");
                }
                w.write_unformatted(tmp2.out());
                w.writeln("break;");
                scope.execute();
                w.writeln("}");
            }
        }
        first_scan.execute();
        may_write_from_hook(w, flags, bm2::HookFile::first_scan, bm2::HookFileSub::after);
        may_write_from_hook(w, flags, bm2::HookFile::tree_scan, bm2::HookFileSub::before);
        futils::helper::DynDefer tree_scan_start;
        auto add_tree_scan_start = [&] {
            if (tree_scan_start) {
                return;
            }
            w.writeln("for (size_t j = 0; j < bm.programs.ranges.size(); j++) {");
            auto nest_scope = w.indent_scope();
            w.writeln("rebgn::Range range{.start = bm.programs.ranges[j].start.value(), .end = bm.programs.ranges[j].end.value()};");
            w.writeln("for (size_t i = range.start + 1; i < range.end - 1; i++) {");
            auto nest_scope2 = w.indent_scope();
            w.writeln("auto& code = bm.code[i];");
            w.writeln("switch (code.op) {");
            auto nest_scope3 = w.indent_scope();
            tree_scan_start = [&w, nest_scope = std::move(nest_scope), nest_scope2 = std::move(nest_scope2), nest_scope3 = std::move(nest_scope3)]() mutable {
                w.writeln("default: {}");
                nest_scope3.execute();
                nest_scope2.execute();
                w.writeln("}");
                nest_scope.execute();
                w.writeln("}");
                w.writeln("OUT_TREE_SCAN:;");
            };
        };

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            bm2::TmpCodeWriter tmp2;
            if (may_write_from_hook(tmp2, flags, bm2::HookFile::tree_scan, op)) {
                add_tree_scan_start();
                w.writeln("case rebgn::AbstractOp::", to_string(op), ": {");
                auto scope = w.indent_scope();
                if (op == AbstractOp::METADATA) {
                    w.writeln("auto meta = code.metadata();");
                    w.writeln("auto name = ctx.metadata_table[meta->name.value()];");
                }
                w.write_unformatted(tmp2.out());
                w.writeln("break;");
                scope.execute();
                w.writeln("}");
            }
        }
        tree_scan_start.execute();
        may_write_from_hook(w, flags, bm2::HookFile::tree_scan, bm2::HookFileSub::after);

        bm2::TmpCodeWriter tmp;
        may_write_from_hook(w, flags, bm2::HookFile::file_top, bm2::HookFileSub::before);
        if (may_write_from_hook(tmp, flags, bm2::HookFile::file_top, true)) {
            w.writeln("{");
            auto scope = w.indent_scope();
            w.writeln("auto& w = ctx.cw;");
            w.write_unformatted(tmp.out());
            scope.execute();
            w.writeln("}");
            tmp.out().clear();
        }
        may_write_from_hook(w, flags, bm2::HookFile::file_top, bm2::HookFileSub::after);

        w.writeln("for (size_t j = 0; j < bm.programs.ranges.size(); j++) {");
        auto scope1 = w.indent_scope();
        w.writeln("/* exclude DEFINE_PROGRAM and END_PROGRAM */");
        w.writeln("TmpCodeWriter w;");
        may_write_from_hook(w, flags, bm2::HookFile::each_inner_block, false);
        w.writeln("inner_block(ctx, w, rebgn::Range{.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});");
        w.writeln("ctx.cw.write_unformatted(w.out());");
        scope1.execute();
        w.writeln("}");

        w.writeln("for (auto& def : ctx.on_functions) {");
        w.indent_writeln("def.execute();");
        w.writeln("}");

        w.writeln("for (size_t i = 0; i < bm.ident_ranges.ranges.size(); i++) {");
        auto _scope = w.indent_scope();
        w.writeln("auto& range = bm.ident_ranges.ranges[i];");
        w.writeln("auto& code = bm.code[range.range.start.value()];");
        w.writeln("if (code.op != rebgn::AbstractOp::DEFINE_FUNCTION) {");
        w.indent_writeln("continue;");
        w.writeln("}");
        w.writeln("TmpCodeWriter w;");
        may_write_from_hook(w, flags, bm2::HookFile::each_inner_function, false);
        w.writeln("inner_function(ctx, w, rebgn::Range{.start = range.range.start.value() , .end = range.range.end.value()});");
        w.writeln("ctx.cw.write_unformatted(w.out());");
        _scope.execute();
        w.writeln("}");

        may_write_from_hook(w, flags, bm2::HookFile::file_bottom, bm2::HookFileSub::before);
        if (may_write_from_hook(tmp, flags, bm2::HookFile::file_bottom, true)) {
            w.writeln("{");
            auto scope = w.indent_scope();
            w.writeln("auto& w = ctx.cw;");
            w.write_unformatted(tmp.out());
            scope.execute();
            w.writeln("}");
        }
        may_write_from_hook(w, flags, bm2::HookFile::file_bottom, bm2::HookFileSub::after);

        scope_to_xxx.execute();
        w.writeln("}");

        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);
    }
}  // namespace rebgn

auto& cout = futils::wrap::cout_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    bm2::TmpCodeWriter w;
    rebgn::write_code_template(w, flags);
    if (flags.mode == bm2::GenerateMode::docs_json || flags.mode == bm2::GenerateMode::docs_markdown) {
        w.out().clear();
        rebgn::write_template_document(flags, w);
        cout << w.out();
        return 0;
    }
    if (!flags.print_hooks) {
        cout << w.out();
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
