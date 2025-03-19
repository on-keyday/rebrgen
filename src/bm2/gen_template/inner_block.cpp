/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {
    void write_inner_block(bm2::TmpCodeWriter& inner_block,
                           AbstractOp op, Flags& flags) {
        flags.set_func_name(bm2::FuncName::inner_block);
        inner_block.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = inner_block.indent_scope();
        auto block_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_op, op, stage)) {
                inner();
            }
        };
        block_hook([&] {}, bm2::HookFileSub::before);
        std::string desc = to_string(op);
        if (desc.starts_with("DECLARE_")) {
            desc.erase(0, 8);  // remove "DECLARE_"
        }
        if (desc.starts_with("DEFINE_")) {
            desc.erase(0, 7);  // remove "DEFINE_"
        }
        if (op == AbstractOp::DECLARE_FORMAT || op == AbstractOp::DECLARE_ENUM ||
            op == AbstractOp::DECLARE_STATE || op == AbstractOp::DECLARE_PROPERTY ||
            op == AbstractOp::DECLARE_FUNCTION) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), desc);
            define_range(inner_block, flags, op, "inner_range", code_ref(flags, "ref"), desc);
            if (op == AbstractOp::DECLARE_FUNCTION) {
                block_hook([&] {});  // do nothing
            }
            else {
                block_hook([&] {
                    inner_block.writeln("inner_block(ctx, w, inner_range);");
                });
            }
        }
        else if (op == AbstractOp::DEFINE_PROPERTY ||
                 op == AbstractOp::END_PROPERTY) {
            block_hook([&] {});  // do nothing
        }
        else if (op == AbstractOp::DECLARE_BIT_FIELD) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), "bit field");
            define_ident(inner_block, flags, op, "ident", "ref", "bit field", true);
            define_range(inner_block, flags, op, "inner_range", "ref", "bit field");
            define_ref(inner_block, flags, op, "type_ref", "ctx.ref(ref).type().value()", "bit field type");
            define_type(inner_block, flags, op, "type", "type_ref", "bit field type", true);
            block_hook([&] {
                inner_block.writeln("inner_block(ctx,w,inner_range);");
            });
        }
        else if (op == AbstractOp::DECLARE_UNION || op == AbstractOp::DECLARE_UNION_MEMBER) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), desc);
            define_range(inner_block, flags, op, "inner_range", "ref", desc);
            block_hook([&] {
                inner_block.writeln("TmpCodeWriter inner_w;");
                inner_block.writeln("inner_block(ctx, inner_w, inner_range);");
                inner_block.writeln("ctx.cw.write_unformatted(inner_w.out());");
            });
        }
        else if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE || op == AbstractOp::DEFINE_UNION_MEMBER) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "format");
            block_hook([&] {
                inner_block.writeln("w.writeln(\"", flags.struct_keyword, " \", ident, \" ", flags.block_begin, "\");");
                inner_block.writeln("defer.push_back(w.indent_scope_ex());");
            });
        }
        else if (op == AbstractOp::DEFINE_UNION) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "union");
            block_hook([&] {
                if (flags.variant_mode == "union") {
                    inner_block.writeln("w.writeln(\"", flags.union_keyword, " \",ident, \" ", flags.block_begin, "\");");
                    inner_block.writeln("defer.push_back(w.indent_scope_ex());");
                }
            });
        }
        else if (op == AbstractOp::END_UNION) {
            block_hook([&] {
                if (flags.variant_mode == "union") {
                    inner_block.writeln("defer.pop_back();");
                    inner_block.writeln("w.writeln(\"", flags.block_end_type, "\");");
                }
            });
        }
        else if (op == AbstractOp::DEFINE_ENUM) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "enum");
            define_type_ref(inner_block, flags, op, "base_type_ref", code_ref(flags, "type"), "enum base type");
            do_typed_variable_definition(inner_block, flags, op, "base_type", "std::nullopt", "std::optional<std::string>", "enum base type");
            inner_block.writeln("if(base_type_ref.ref.value() != 0) {");
            auto scope = inner_block.indent_scope();
            inner_block.writeln("base_type = type_to_string(ctx,base_type_ref);");
            scope.execute();
            inner_block.writeln("}");
            if (flags.default_enum_base.size()) {
                inner_block.writeln("else {");
                inner_block.indent_writeln("base_type = \"", flags.default_enum_base, "\";");
                inner_block.writeln("}");
            }
            block_hook([&] {
                inner_block.writeln("w.write(\"", flags.enum_keyword, " \", ident, \" ", flags.block_begin, "\");");
                inner_block.writeln("if(base_type) {");
                inner_block.indent_writeln("w.write(\" ", flags.enum_base_separator, " \", *base_type);");
                inner_block.writeln("}");
                inner_block.writeln("defer.push_back(w.indent_scope_ex());");
            });
        }
        else if (op == AbstractOp::DEFINE_ENUM_MEMBER) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "enum member");
            define_eval(inner_block, flags, op, "evaluated", code_ref(flags, "left_ref"), "enum member value");
            define_ident(inner_block, flags, op, "enum_ident", code_ref(flags, "belong"), "enum");
            block_hook([&] {
                inner_block.writeln("w.writeln(ident, \" = \", evaluated.result, \"", flags.enum_member_end, "\");");
            });
        }
        else if (op == AbstractOp::DEFINE_FIELD) {
            inner_block.writeln("if (ctx.ref(code.belong().value()).op == rebgn::AbstractOp::DEFINE_PROGRAM) {");
            auto scope = inner_block.indent_scope();
            inner_block.writeln("break;");
            scope.execute();
            inner_block.writeln("}");
            define_type(inner_block, flags, op, "type", code_ref(flags, "type"), "field type", true);
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "field");
            block_hook([&] {
                if (flags.prior_ident) {
                    inner_block.writeln("w.writeln(ident, \" ", flags.field_type_separator, "\", type, \"", flags.field_end, "\");");
                }
                else {
                    inner_block.writeln("w.writeln(type, \" ", flags.field_type_separator, "\", ident, \"", flags.field_end, "\");");
                }
            });
        }
        else if (op == AbstractOp::END_FORMAT || op == AbstractOp::END_ENUM || op == AbstractOp::END_STATE ||
                 op == AbstractOp::END_UNION_MEMBER) {
            block_hook([&] {
                inner_block.writeln("defer.pop_back();");
                inner_block.writeln("w.writeln(\"", flags.block_end_type, "\");");
            });
        }
        else if (op == AbstractOp::DEFINE_PROPERTY_GETTER || op == AbstractOp::DEFINE_PROPERTY_SETTER) {
            define_ref(inner_block, flags, op, "func", code_ref(flags, "right_ref"), "function");
            define_range(inner_block, flags, op, "inner_range", "func", "function");
            block_hook([&] {});
        }
        else {
            block_hook([&] {
                inner_block.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
            });
        }
        block_hook([&] {}, bm2::HookFileSub::after);
        inner_block.writeln("break;");
        scope.execute();
        inner_block.writeln("}");
    }

    void write_inner_block_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_block, Flags& flags) {
        w.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        inner_block.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_block = inner_block.indent_scope();
        inner_block.writeln("std::vector<futils::helper::DynDefer> defer;");
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_start, false);
        inner_block.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_block = inner_block.indent_scope();
        inner_block.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_each_code, false);
        inner_block.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }
            if (is_struct_define_related(op)) {
                write_inner_block(inner_block, op, flags);
            }
        }

        inner_block.writeln("default: {");
        auto if_block = inner_block.indent_scope();
        inner_block.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {");
        inner_block.indent_writeln("w.writeln(", unimplemented_comment(flags, "code.op"), ");");
        inner_block.writeln("}");
        inner_block.writeln("break;");
        if_block.execute();
        inner_block.writeln("}");  // close default
        inner_block.writeln("}");  // close switch
        scope_nest_block.execute();
        inner_block.writeln("}");  // close for
        scope_block.execute();
        inner_block.writeln("}");  // close function
    }

}  // namespace rebgn
