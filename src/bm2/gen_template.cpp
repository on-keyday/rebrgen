/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <bm2/context.hpp>
#include <bm/helper.hpp>
#include <wrap/cout.h>
struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view lang_name;
    bool is_header = false;
    std::string_view comment_prefix = "/*";
    std::string_view comment_suffix = "*/";
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&lang_name, "lang", "language name", "LANG", futils::cmdline::option::CustomFlag::required);
        ctx.VarBool(&is_header, "header", "generate header file");
        ctx.VarString<true>(&comment_prefix, "comment-prefix", "comment prefix", "PREFIX");
        ctx.VarString<true>(&comment_suffix, "comment-suffix", "comment suffix", "SUFFIX");
    }

    std::string wrap_comment(const std::string& comment) {
        std::string result;
        result.reserve(comment.size() + comment_prefix.size() + comment_suffix.size());
        result.append(comment_prefix);
        result.append(comment);
        result.append(comment_suffix);
        return result;
    }
};
namespace rebgn {
    void code_template(bm2::TmpCodeWriter& w, Flags& flags) {
        bm2::TmpCodeWriter type_to_string;

        type_to_string.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0) {");
        auto scope_type_to_string = type_to_string.indent_scope();
        type_to_string.writeln("if (s.storages.size() <= index) {");
        auto if_block_type = type_to_string.indent_scope();
        type_to_string.writeln("return ", flags.wrap_comment("type index overflow"), ";");
        if_block_type.execute();
        type_to_string.writeln("}");
        type_to_string.writeln("auto& storage = s.storages[index];");
        type_to_string.writeln("switch (storage.type) {");
        auto switch_scope = type_to_string.indent_scope();
        for (size_t i = 0; to_string(StorageType(i))[0] != 0; i++) {
            auto type = StorageType(i);
            type_to_string.writeln(std::format("case rebgn::StorageType::{}: {{", to_string(type)));
            auto scope_type = type_to_string.indent_scope();
            type_to_string.writeln(std::format("return \"{}\";", flags.wrap_comment("Unimplemented " + std::string(to_string(type)))));
            scope_type.execute();
            type_to_string.writeln("}");
        }
        type_to_string.writeln("default: {");
        auto if_block_type_default = type_to_string.indent_scope();
        type_to_string.writeln("return std::format(\"", flags.wrap_comment("Unimplemented {}"), "\", to_string(storage.type));");
        if_block_type_default.execute();
        type_to_string.writeln("}");
        switch_scope.execute();
        type_to_string.writeln("}");
        scope_type_to_string.execute();
        type_to_string.writeln("}");

        type_to_string.writeln("std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref) {");
        auto scope_type_to_string_ref = type_to_string.indent_scope();
        type_to_string.writeln("auto& storage = ctx.storage_table[ref.ref.value()];");
        type_to_string.writeln("return type_to_string_impl(ctx, storage);");
        scope_type_to_string_ref.execute();
        type_to_string.writeln("}");

        bm2::TmpCodeWriter inner_block;
        bm2::TmpCodeWriter inner_function;
        bm2::TmpCodeWriter eval;
        inner_block.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_block = inner_block.indent_scope();
        inner_block.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_block = inner_block.indent_scope();
        inner_block.writeln("auto& code = ctx.bm.code[i];");
        inner_block.writeln("switch(code.op) {");
        inner_function.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_function = inner_function.indent_scope();
        inner_function.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_function = inner_function.indent_scope();
        inner_function.writeln("auto& code = ctx.bm.code[i];");
        inner_function.writeln("switch(code.op) {");
        eval.writeln("std::vector<std::string> eval(const rebgn::Code& code, Context& ctx) {");
        auto scope_eval = eval.indent_scope();
        eval.writeln("std::vector<std::string> result;");
        eval.writeln("switch(code.op) {");
        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }
            if (is_struct_define_related(op)) {
                inner_block.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                inner_block.indent_writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                inner_block.indent_writeln("break;");
                inner_block.writeln("}");
            }
            else {
                if (is_expr(op)) {
                    eval.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    // some are may be common in several languages, so it is better to implement in the inner_function
                    if (op == AbstractOp::BINARY) {
                        eval.indent_writeln("auto op = code.bop().value();");
                        eval.indent_writeln("auto left_ref = code.left_ref().value();");
                        eval.indent_writeln("auto right_ref = code.right_ref().value();");
                        eval.indent_writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                        eval.indent_writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                        eval.indent_writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                        eval.indent_writeln("result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);");
                        eval.indent_writeln("auto opstr = to_string(op);");
                        eval.indent_writeln("result.push_back(std::format(\"({} {} {})\", left_eval.back(), opstr, right_eval.back()));");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::UNARY) {
                        eval.indent_writeln("auto op = code.uop().value();");
                        eval.indent_writeln("auto ref = code.ref().value();");
                        eval.indent_writeln("auto target = eval(ctx.ref(ref), ctx);");
                        eval.indent_writeln("auto opstr = to_string(op);");
                        eval.indent_writeln("result.push_back(std::format(\"({}{})\", opstr, target.back()));");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::IMMEDIATE_INT) {
                        eval.indent_writeln("result.push_back(std::format(\"{}\", code.int_value()->value()));");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::IMMEDIATE_TRUE) {
                        eval.indent_writeln("result.push_back(\"true\");");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::IMMEDIATE_FALSE) {
                        eval.indent_writeln("result.push_back(\"false\");");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::IMMEDIATE_INT64) {
                        eval.indent_writeln("result.push_back(std::format(\"{}\", *code.int_value64()));");
                        eval.indent_writeln("break;");
                    }
                    else if (op == AbstractOp::PHI) {
                        eval.indent_writeln("auto ref=code.ref().value();");
                        eval.indent_writeln("return eval(ctx.ref(ref), ctx);");
                    }
                    else if (op == AbstractOp::FIELD_AVAILABLE) {
                        eval.indent_writeln("auto left_ref = code.left_ref().value();");
                        eval.indent_writeln("if(left_ref.value() == 0) {");
                        auto if_block = eval.indent_scope();
                        eval.indent_writeln("auto right_ref = code.right_ref().value();");
                        eval.indent_writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                        eval.indent_writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                        if_block.execute();
                        eval.indent_writeln("}");
                        eval.indent_writeln("else {");
                        auto else_block = eval.indent_scope();
                        eval.indent_writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                        eval.indent_writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                        eval.indent_writeln("ctx.this_as.push_back(left_eval.back());");
                        eval.indent_writeln("auto right_ref = code.right_ref().value();");
                        eval.indent_writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                        eval.indent_writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                        eval.indent_writeln("ctx.this_as.pop_back();");
                        else_block.execute();
                        eval.indent_writeln("}");
                        eval.indent_writeln("break;");
                    }
                    else {
                        eval.indent_writeln("result.push_back(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                        eval.indent_writeln("break;");
                    }
                    eval.writeln("}");
                }
                if (!is_expr(op) || is_both_expr_and_def(op)) {
                    inner_function.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    inner_function.indent_writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    inner_function.indent_writeln("break;");
                    inner_function.writeln("}");
                }
            }
        }
        inner_block.writeln("default: {");
        auto if_block = inner_block.indent_scope();
        inner_block.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op))");
        inner_block.indent_writeln("w.writeln(std::format(\"/* Unimplemented {} */\", to_string(code.op)));");
        inner_block.writeln("}");
        inner_block.writeln("break;");
        if_block.execute();
        inner_block.writeln("}");  // close switch
        scope_nest_block.execute();
        inner_block.writeln("}");  // close for
        scope_block.execute();
        inner_block.writeln("}");  // close function

        inner_function.writeln("default: {");
        auto if_function = inner_function.indent_scope();
        inner_function.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_struct_define_related(code.op)&&!rebgn::is_expr(code.op)) {");
        inner_function.indent_writeln("w.writeln(std::format(\"", flags.wrap_comment("Unimplemented {}"), "\", to_string(code.op)));");
        inner_function.writeln("}");
        inner_function.writeln("break;");
        if_function.execute();
        inner_function.writeln("}");
        inner_function.writeln("}");  // close switch
        scope_nest_function.execute();
        inner_function.writeln("}");  // close for
        scope_function.execute();
        inner_function.writeln("}");  // close function

        eval.writeln("default: {");
        eval.indent_writeln("result.push_back(std::format(\"", flags.wrap_comment("Unimplemented {}"), "\", to_string(code.op)));");
        eval.indent_writeln("break;");
        eval.writeln("}");
        eval.write("}");  // close switch
        eval.writeln("return result;");
        scope_eval.execute();
        eval.writeln("}");  // close function

        w.writeln("#include <bm2/context.hpp>");
        w.writeln("#include <bm2/entry.hpp>");
        w.writeln("#include <bm/helper.hpp>");
        w.writeln("namespace bm2", flags.lang_name, " {");
        auto scope = w.indent_scope();
        w.writeln("using TmpCodeWriter = bm2::TmpCodeWriter;");
        w.writeln("struct Flags {};");
        w.writeln("struct Context : bm2::Context {");
        auto scope_context = w.indent_scope();
        w.writeln("Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,\"r\",\"w\",\"(*this)\", std::move(escape_ident)} {}");
        scope_context.execute();
        w.writeln("};");

        w.write_unformatted(type_to_string.out());
        w.write_unformatted(inner_block.out());
        w.write_unformatted(inner_function.out());
        w.write_unformatted(eval.out());

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {");
        auto scope_to_xxx = w.indent_scope();
        w.writeln("Context ctx{w, bm, [&](Context& ctx, std::uint64_t id, auto&& str) {");
        auto scope_escape_ident = w.indent_scope();
        w.writeln("return str;");
        scope_escape_ident.execute();
        w.writeln("}};");
        w.writeln("// search metadata");
        w.writeln("for (size_t j = 0; j < bm.programs.ranges.size(); j++) {");
        auto nest_scope = w.indent_scope();
        w.writeln("for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {");
        auto nest_scope2 = w.indent_scope();
        w.writeln("auto& code = bm.code[i];");
        w.writeln("switch (code.op) {");
        auto nest_scope3 = w.indent_scope();
        w.writeln("case rebgn::AbstractOp::METADATA: {");
        auto nest_scope4 = w.indent_scope();
        w.writeln("auto meta = code.metadata();");
        w.writeln("auto str = ctx.metadata_table[meta->name.value()];");
        w.writeln("// handle metadata...");
        w.writeln("break;");
        nest_scope4.execute();
        w.writeln("}");
        nest_scope3.execute();
        w.writeln("}");
        nest_scope2.execute();
        w.writeln("}");
        nest_scope.execute();
        w.writeln("}");

        w.writeln("for (size_t j = 0; j < bm.programs.ranges.size(); j++) {");
        w.indent_writeln("/* exclude DEFINE_PROGRAM and END_PROGRAM */");
        w.indent_writeln("TmpCodeWriter w;");
        w.indent_writeln("inner_block(ctx, w, rebgn::Range{.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});");
        w.indent_writeln("w.write_unformatted(w.out());");
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
        w.writeln("inner_function(ctx, w, rebgn::Range{.start = range.range.start.value() , .end = range.range.end.value()});");
        w.writeln("w.write_unformatted(w.out());");
        _scope.execute();
        w.writeln("}");

        scope_to_xxx.execute();
        w.writeln("}");

        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);

        w.writeln("struct Flags : bm2::Flags {");
        w.indent_writeln("bm2", flags.lang_name, "::Flags bm2", flags.lang_name, "_flags;");
        w.indent_writeln("void bind(futils::cmdline::option::Context& ctx) {");
        auto scope_bind = w.indent_scope();
        w.indent_writeln("bind_help(ctx);");
        scope_bind.execute();
        w.indent_writeln("}");
        w.writeln("};");

        w.writeln("DEFINE_ENTRY(Flags) {");
        auto scope_entry = w.indent_scope();
        w.writeln("bm2", flags.lang_name, "::to_", flags.lang_name, "(w, bm,flags.bm2", flags.lang_name, "_flags);");
        w.writeln("return 0;");
        scope_entry.execute();
        w.writeln("}");
    }
}  // namespace rebgn

auto& cout = futils::wrap::cout_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    bm2::TmpCodeWriter w;
    rebgn::code_template(w, flags);
    cout << w.out();
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
