/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <bm2/context.hpp>
#include <bm/helper.hpp>
#include <wrap/cout.h>
#include <strutil/splits.h>
#include <file/file_view.h>
#include <json/convert_json.h>
#include <json/json_export.h>

struct Flags : futils::cmdline::templ::HelpOption {
    std::string lang_name;
    bool is_header = false;
    bool is_main = false;
    bool is_cmake = false;
    std::string comment_prefix = "/*";
    std::string comment_suffix = "*/";
    std::string int_type_placeholder = "std::int{}_t";
    std::string uint_type_placeholder = "std::uint{}_t";
    std::string float_type_placeholder = "float{}_t";
    std::string array_type_placeholder = "std::array<{}, {}>";
    bool array_has_one_placeholder = false;
    std::string vector_type_placeholder = "std::vector<{}>";
    std::string bool_type = "bool";
    std::string coder_return_type = "bool";
    std::string property_setter_return_type = "bool";
    std::string end_of_statement = ";";
    std::string block_begin = "{";
    std::string block_end = "}";
    std::string block_end_type = "};";
    std::string keyword_file = "keyword.txt";
    std::string import_base_file = "import_base.txt";
    std::string struct_keyword = "struct";
    std::string enum_keyword = "enum";
    std::string define_var_keyword = "";
    std::string var_type_separator = " ";
    std::string field_type_separator = " ";
    std::string field_end = ";";
    std::string enum_member_end = ",";

    bool prior_ident = false;

    std::string_view config_file = "config.json";

    bool from_json(const futils::json::JSON& js) {
        JSON_PARAM_BEGIN(*this, js)
        FROM_JSON_OPT(lang_name, "lang");
        FROM_JSON_OPT(comment_prefix, "comment_prefix");
        FROM_JSON_OPT(comment_suffix, "comment_suffix");
        FROM_JSON_OPT(int_type_placeholder, "int_type");
        FROM_JSON_OPT(uint_type_placeholder, "uint_type");
        FROM_JSON_OPT(float_type_placeholder, "float_type");
        FROM_JSON_OPT(array_type_placeholder, "array_type");
        FROM_JSON_OPT(array_has_one_placeholder, "array_has_one_placeholder");
        FROM_JSON_OPT(vector_type_placeholder, "vector_type");
        FROM_JSON_OPT(bool_type, "bool_type");
        FROM_JSON_OPT(coder_return_type, "coder_return_type");
        FROM_JSON_OPT(property_setter_return_type, "property_setter_return_type");
        FROM_JSON_OPT(end_of_statement, "end_of_statement");
        FROM_JSON_OPT(block_begin, "block_begin");
        FROM_JSON_OPT(block_end, "block_end");
        FROM_JSON_OPT(block_end_type, "block_end_type");
        FROM_JSON_OPT(keyword_file, "keyword_file");
        FROM_JSON_OPT(import_base_file, "import_base_file");
        FROM_JSON_OPT(prior_ident, "prior_ident");
        FROM_JSON_OPT(struct_keyword, "struct_keyword");
        FROM_JSON_OPT(enum_keyword, "enum_keyword");
        FROM_JSON_OPT(define_var_keyword, "define_var_keyword");
        FROM_JSON_OPT(var_type_separator, "var_type_separator");
        FROM_JSON_OPT(field_type_separator, "field_type_separator");
        FROM_JSON_OPT(field_end, "field_end");
        FROM_JSON_OPT(enum_member_end, "enum_member_end");
        JSON_PARAM_END()
    }

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&lang_name, "lang", "language name", "LANG");
        ctx.VarBool(&is_header, "header", "generate header file");
        ctx.VarBool(&is_main, "main", "generate main file");
        ctx.VarBool(&is_cmake, "cmake", "generate cmake file");
        ctx.VarBool(&prior_ident, "prior-ident", "prioritize identifier than type when defining field or parameter or variable");
        ctx.VarString(&comment_prefix, "comment-prefix", "comment prefix", "PREFIX");
        ctx.VarString(&comment_suffix, "comment-suffix", "comment suffix", "SUFFIX");
        ctx.VarString(&int_type_placeholder, "int-type", "int type placeholder ({} will be replaced with bit size)", "PLACEHOLDER");
        ctx.VarString(&uint_type_placeholder, "uint-type", "uint type placeholder ({} will be replaced with bit size)", "PLACEHOLDER");
        ctx.VarString(&keyword_file, "keyword-file", "keyword file", "FILE");
        ctx.VarString(&import_base_file, "import-base-file", "import base file", "FILE");
        ctx.VarString(&end_of_statement, "end-of-statement", "end of statement", "EOS");
        ctx.VarString(&block_begin, "block-begin", "block begin", "BEGIN");
        ctx.VarString(&block_end, "block-end", "block end", "END");
        ctx.VarString(&block_end_type, "block-end-type", "block end of type definition", "END_TYPE");
        ctx.VarString(&struct_keyword, "struct-keyword", "struct keyword", "KEYWORD");
        ctx.VarString(&enum_keyword, "enum-keyword", "enum keyword", "KEYWORD");
        ctx.VarString(&float_type_placeholder, "float-type", "float type placeholder ({} will be replaced with bit size)", "PLACEHOLDER");
        ctx.VarString(&array_type_placeholder, "array-type", "array type placeholder ({} will be replaced with element type, {} will be replaced with size)", "PLACEHOLDER");
        ctx.VarBool(&array_has_one_placeholder, "array-has-one-placeholder", "array type has one placeholder");
        ctx.VarString(&vector_type_placeholder, "vector-type", "vector type placeholder ({} will be replaced with element type)", "PLACEHOLDER");
        ctx.VarString(&bool_type, "bool-type", "bool type", "TYPE");
        ctx.VarString(&coder_return_type, "coder-return-type", "coder return type", "TYPE");
        ctx.VarString(&property_setter_return_type, "property-setter-return-type", "property setter return type", "TYPE");
        ctx.VarString(&define_var_keyword, "define-var-keyword", "define variable keyword", "KEYWORD");
        ctx.VarString(&var_type_separator, "var-type-separator", "variable type separator", "SEPARATOR");
        ctx.VarString(&field_type_separator, "field-type-separator", "field type separator", "SEPARATOR");
        ctx.VarString(&field_end, "field-end", "field end", "END");
        ctx.VarString(&enum_member_end, "enum-member-end", "enum member end", "END");
        ctx.VarString<true>(&config_file, "config-file", "config file", "FILE");
    }

    bool is_valid_placeholder(std::string_view placeholder) {
        auto first_found = placeholder.find("{}");
        if (first_found == std::string_view::npos) {
            return false;
        }
        auto second_found = placeholder.find("{}", first_found + 1);
        return second_found == std::string_view::npos;
    }

    std::string wrap_int(size_t bit_size) {
        if (is_valid_placeholder(int_type_placeholder)) {
            return std::vformat(int_type_placeholder, std::make_format_args(bit_size));
        }
        else if (!int_type_placeholder.contains("{") && !int_type_placeholder.contains("}")) {
            return int_type_placeholder;
        }
        return std::format("/* invalid placeholder: {} */", int_type_placeholder);
    }

    std::string wrap_uint(size_t bit_size) {
        if (is_valid_placeholder(uint_type_placeholder)) {
            return std::vformat(uint_type_placeholder, std::make_format_args(bit_size));
        }
        else if (!uint_type_placeholder.contains("{") && !uint_type_placeholder.contains("}")) {
            return uint_type_placeholder;
        }
        return std::format("/* invalid placeholder: {} */", uint_type_placeholder);
    }

    std::string wrap_float(size_t bit_size) {
        if (is_valid_placeholder(float_type_placeholder)) {
            return std::vformat(float_type_placeholder, std::make_format_args(bit_size));
        }
        else if (!float_type_placeholder.contains("{") && !float_type_placeholder.contains("}")) {
            return float_type_placeholder;
        }
        return std::format("/* invalid placeholder: {} */", float_type_placeholder);
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
    void write_impl_template(bm2::TmpCodeWriter& w, Flags& flags) {
        auto unimplemented_comment = [&](const std::string& op) {
            return "std::format(\"{}{}{}\",\"" + flags.comment_prefix + "\",to_string(" + op + "),\"" + flags.comment_suffix + "\")";
        };

        bm2::TmpCodeWriter type_to_string;

        type_to_string.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0) {");
        auto scope_type_to_string = type_to_string.indent_scope();
        type_to_string.writeln("if (s.storages.size() <= index) {");
        auto if_block_type = type_to_string.indent_scope();
        type_to_string.writeln("return \"", flags.wrap_comment("type index overflow"), "\";");
        if_block_type.execute();
        type_to_string.writeln("}");
        type_to_string.writeln("auto& storage = s.storages[index];");
        type_to_string.writeln("switch (storage.type) {");
        auto switch_scope = type_to_string.indent_scope();
        for (size_t i = 0; to_string(StorageType(i))[0] != 0; i++) {
            auto type = StorageType(i);
            type_to_string.writeln(std::format("case rebgn::StorageType::{}: {{", to_string(type)));
            auto scope_type = type_to_string.indent_scope();
            if (type == StorageType::ARRAY || type == StorageType::VECTOR || type == StorageType::OPTIONAL || type == StorageType::PTR) {
                type_to_string.writeln("auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1);");
            }
            if (type == StorageType::UINT || type == StorageType::INT || type == StorageType::FLOAT) {
                type_to_string.writeln("auto size = storage.size().value().value();");
                type_to_string.writeln("if (bit_size) {");
                auto if_block_size = type_to_string.indent_scope();
                type_to_string.writeln("*bit_size = size;");
                if_block_size.execute();
                type_to_string.writeln("}");
                if (type == StorageType::UINT) {
                    type_to_string.writeln("if (size <= 8) {");
                    auto if_block_size_8 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_uint(8), "\";");
                    if_block_size_8.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else if (size <= 16) {");
                    auto if_block_size_16 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_uint(16), "\";");
                    if_block_size_16.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else if (size <= 32) {");
                    auto if_block_size_32 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_uint(32), "\";");
                    if_block_size_32.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else {");
                    auto if_block_size_64 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_uint(64), "\";");
                    if_block_size_64.execute();
                    type_to_string.writeln("}");
                }
                else if (type == StorageType::INT) {
                    type_to_string.writeln("if (size <= 8) {");
                    auto if_block_size_8 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_int(8), "\";");
                    if_block_size_8.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else if (size <= 16) {");
                    auto if_block_size_16 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_int(16), "\";");
                    if_block_size_16.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else if (size <= 32) {");
                    auto if_block_size_32 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_int(32), "\";");
                    if_block_size_32.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else {");
                    auto if_block_size_64 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_int(64), "\";");
                    if_block_size_64.execute();
                    type_to_string.writeln("}");
                }
                else {
                    type_to_string.writeln("if (size <= 32) {");
                    auto if_block_size_32 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_float(32), "\";");
                    if_block_size_32.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("else {");
                    auto if_block_size_64 = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.wrap_float(64), "\";");
                    if_block_size_64.execute();
                    type_to_string.writeln("}");
                }
            }

            else if (type == StorageType::STRUCT_REF) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_to_string.writeln("return ident;");
            }
            else if (type == StorageType::RECURSIVE_STRUCT_REF) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_to_string.writeln("return std::format(\"{}*\", ident);");
            }
            else if (type == StorageType::BOOL) {
                type_to_string.writeln("return \"", flags.bool_type, "\";");
            }
            else if (type == StorageType::ENUM) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_to_string.writeln("return ident;");
            }
            else if (type == StorageType::VARIANT) {
                type_to_string.writeln("return \"", flags.wrap_comment("Unimplemented VARIANT"), "\";");
            }
            else if (type == StorageType::CODER_RETURN) {
                type_to_string.writeln("return \"", flags.coder_return_type, "\";");
            }
            else if (type == StorageType::PROPERTY_SETTER_RETURN) {
                type_to_string.writeln("return \"", flags.property_setter_return_type, "\";");
            }
            else if (type == StorageType::PTR) {
                type_to_string.writeln("return std::format(\"{}*\", base_type);");
            }
            else if (type == StorageType::ARRAY) {
                if (flags.array_has_one_placeholder) {
                    type_to_string.writeln("return std::format(\"", flags.array_type_placeholder, "\", base_type);");
                }
                else {
                    type_to_string.writeln("auto length = storage.size().value().value();");
                    type_to_string.writeln("return std::format(\"", flags.array_type_placeholder, "\", base_type,length);");
                }
            }
            else if (type == StorageType::VECTOR) {
                type_to_string.writeln("return std::format(\"", flags.vector_type_placeholder, "\", base_type);");
            }
            else if (type == StorageType::OPTIONAL) {
                type_to_string.writeln("return \"", flags.wrap_comment("Unimplemented OPTIONAL"), "\";");
            }
            scope_type.execute();
            type_to_string.writeln("}");
        }
        type_to_string.writeln("default: {");
        auto if_block_type_default = type_to_string.indent_scope();
        type_to_string.writeln("return ", unimplemented_comment("storage.type"), ";");
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

        bm2::TmpCodeWriter add_parameter;
        add_parameter.writeln("void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_add_parameter = add_parameter.indent_scope();
        add_parameter.writeln("size_t params = 0;");
        add_parameter.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_add_parameter = add_parameter.indent_scope();
        add_parameter.writeln("auto& code = ctx.bm.code[i];");
        add_parameter.writeln("switch(code.op) {");
        auto scope_switch_add_parameter = add_parameter.indent_scope();

        bm2::TmpCodeWriter add_call_parameter;
        add_call_parameter.writeln("void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("size_t params = 0;");
        add_call_parameter.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("auto& code = ctx.bm.code[i];");
        add_call_parameter.writeln("switch(code.op) {");
        auto scope_switch_add_call_parameter = add_call_parameter.indent_scope();

        bm2::TmpCodeWriter inner_block;
        inner_block.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_block = inner_block.indent_scope();
        inner_block.writeln("std::vector<futils::helper::DynDefer> defer;");
        inner_block.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_block = inner_block.indent_scope();
        inner_block.writeln("auto& code = ctx.bm.code[i];");
        inner_block.writeln("switch(code.op) {");

        bm2::TmpCodeWriter inner_function;
        inner_function.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_function = inner_function.indent_scope();
        inner_function.writeln("std::vector<futils::helper::DynDefer> defer;");
        inner_function.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_function = inner_function.indent_scope();
        inner_function.writeln("auto& code = ctx.bm.code[i];");
        inner_function.writeln("switch(code.op) {");

        bm2::TmpCodeWriter field_accessor;
        field_accessor.writeln("EvalResult field_accessor(const rebgn::Code& code, Context& ctx) {");
        auto scope_field_accessor = field_accessor.indent_scope();
        field_accessor.writeln("EvalResult result;");
        field_accessor.writeln("switch(code.op) {");

        bm2::TmpCodeWriter eval_result;
        eval_result.writeln("struct EvalResult {");
        {
            auto scope = eval_result.indent_scope();
            eval_result.writeln("std::string result;");
        }
        eval_result.writeln("};");

        eval_result.writeln("EvalResult make_eval_result(std::string result) {");
        auto scope_make_eval_result = eval_result.indent_scope();
        eval_result.writeln("return EvalResult{std::move(result)};");
        scope_make_eval_result.execute();
        eval_result.writeln("}");

        bm2::TmpCodeWriter eval;

        eval.writeln("EvalResult eval(const rebgn::Code& code, Context& ctx) {");
        auto scope_eval = eval.indent_scope();
        eval.writeln("EvalResult result;");
        eval.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }
            if (is_struct_define_related(op)) {
                inner_block.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                if (op == AbstractOp::DECLARE_FORMAT || op == AbstractOp::DECLARE_ENUM ||
                    op == AbstractOp::DECLARE_STATE || op == AbstractOp::DECLARE_PROPERTY) {
                    inner_block.indent_writeln("auto ref = code.ref().value();");
                    inner_block.indent_writeln("auto range = ctx.range(ref);");
                    inner_block.indent_writeln("inner_block(ctx, w, range);");
                }
                else if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_block.indent_writeln("w.writeln(\"", flags.struct_keyword, " \", ident, \" ", flags.block_begin, "\");");
                    inner_block.indent_writeln("defer.push_back(w.indent_scope_ex());");
                }
                else if (op == AbstractOp::DEFINE_ENUM) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_block.indent_writeln("w.writeln(\"", flags.enum_keyword, " \", ident, \" ", flags.block_begin, "\");");
                    inner_block.indent_writeln("defer.push_back(w.indent_scope_ex());");
                }
                else if (op == AbstractOp::DEFINE_ENUM_MEMBER) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_block.indent_writeln("auto evaluated = eval(ctx.ref(code.left_ref().value()), ctx);");
                    inner_block.indent_writeln("w.writeln(ident, \" = \", evaluated.result, \"", flags.enum_member_end, "\");");
                }
                else if (op == AbstractOp::DEFINE_FIELD) {
                    inner_block.indent_writeln("if (ctx.ref(code.belong().value()).op == rebgn::AbstractOp::DEFINE_PROGRAM) {");
                    auto scope = inner_block.indent_scope();
                    inner_block.indent_writeln("break;");
                    scope.execute();
                    inner_block.indent_writeln("}");
                    inner_block.indent_writeln("auto type = type_to_string(ctx, code.type().value());");
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    if (flags.prior_ident) {
                        inner_block.indent_writeln("w.writeln(ident, \" ", flags.field_type_separator, "\", type, \"", flags.field_end, "\");");
                    }
                    else {
                        inner_block.indent_writeln("w.writeln(type, \" ", flags.field_type_separator, "\", ident, \"", flags.field_end, "\");");
                    }
                }
                else if (op == AbstractOp::END_FORMAT || op == AbstractOp::END_ENUM || op == AbstractOp::END_STATE) {
                    inner_block.indent_writeln("defer.pop_back();");
                    inner_block.indent_writeln("w.writeln(\"", flags.block_end_type, "\");");
                }
                else {
                    inner_block.indent_writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                }
                inner_block.indent_writeln("break;");
                inner_block.writeln("}");

                auto add_start = [&](auto&& inner) {
                    field_accessor.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    auto scope = field_accessor.indent_scope();
                    inner();
                    field_accessor.writeln("break;");
                    scope.execute();
                    field_accessor.writeln("}");
                };
                if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE) {
                    add_start([&] {
                        field_accessor.writeln("result = make_eval_result(ctx.this_());");
                    });
                }
                else if (op == AbstractOp::DEFINE_UNION || op == AbstractOp::DEFINE_UNION_MEMBER ||
                         op == AbstractOp::DEFINE_FIELD || op == AbstractOp::DEFINE_BIT_FIELD) {
                    add_start([&] {
                        field_accessor.writeln("auto belong = code.belong().value();");
                        field_accessor.writeln("if(belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM) {");
                        auto scope = field_accessor.indent_scope();
                        field_accessor.writeln("auto belong_eval = field_accessor(ctx.ref(belong), ctx);");
                        field_accessor.writeln("result = make_eval_result(std::format(\"{}.{}\", belong_eval.result, ctx.ident(code.ident().value())));");
                        scope.execute();
                        field_accessor.writeln("}");
                        field_accessor.writeln("else {");
                        auto scope2 = field_accessor.indent_scope();
                        field_accessor.writeln("result = make_eval_result(ctx.ident(code.ident().value()));");
                        scope2.execute();
                        field_accessor.writeln("}");
                    });
                }
            }

            if (is_expr(op)) {
                eval.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                auto scope = eval.indent_scope();
                if (op == AbstractOp::BINARY) {
                    eval.writeln("auto op = code.bop().value();");
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);");
                    eval.writeln("auto opstr = to_string(op);");
                    eval.writeln("result = make_eval_result(std::format(\"({} {} {})\", left_eval.result, opstr, right_eval.result));");
                }
                else if (op == AbstractOp::UNARY) {
                    eval.writeln("auto op = code.uop().value();");
                    eval.writeln("auto ref = code.ref().value();");
                    eval.writeln("auto target = eval(ctx.ref(ref), ctx);");
                    eval.writeln("auto opstr = to_string(op);");
                    eval.writeln("result = make_eval_result(std::format(\"({}{})\", opstr, target.result));");
                }
                else if (op == AbstractOp::IMMEDIATE_INT) {
                    eval.writeln("result = make_eval_result(std::format(\"{}\", code.int_value()->value()));");
                }
                else if (op == AbstractOp::IMMEDIATE_TRUE) {
                    eval.writeln("result = make_eval_result(\"true\");");
                }
                else if (op == AbstractOp::IMMEDIATE_FALSE) {
                    eval.writeln("result = make_eval_result(\"false\");");
                }
                else if (op == AbstractOp::IMMEDIATE_INT64) {
                    eval.writeln("result = make_eval_result(std::format(\"{}\", *code.int_value64()));");
                }
                else if (op == AbstractOp::IMMEDIATE_TYPE) {
                    eval.writeln("auto type = code.type().value();");
                    eval.writeln("result = make_eval_result(type_to_string(ctx, type));");
                }
                else if (op == AbstractOp::IMMEDIATE_CHAR) {
                    eval.writeln("auto char_code = code.int_value()->value();");
                    eval.writeln("result = make_eval_result(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                }
                else if (op == AbstractOp::PHI || op == AbstractOp::DECLARE_VARIABLE ||
                         op == AbstractOp::DEFINE_VARIABLE_REF) {
                    eval.writeln("auto ref=code.ref().value();");
                    eval.writeln("return eval(ctx.ref(ref), ctx);");
                }
                else if (op == AbstractOp::ACCESS) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_ident = ctx.ident(right_ref);");
                    eval.writeln("result = make_eval_result(std::format(\"{}.{}\", left_eval.result, right_ident));");
                }
                else if (op == AbstractOp::INDEX) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);");
                    eval.writeln("result = make_eval_result(std::format(\"{}[{}]\", left_eval.result, right_eval.result));");
                }
                else if (op == AbstractOp::ARRAY_SIZE) {
                    eval.writeln("auto vector_ref = code.ref().value();");
                    eval.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);");
                    eval.writeln("result = make_eval_result(std::format(\"{}.size()\", vector_eval.result));");
                }
                else if (op == AbstractOp::DEFINE_FIELD) {
                    eval.writeln("result = field_accessor(code, ctx);");
                }
                else if (op == AbstractOp::DEFINE_VARIABLE || op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
                    eval.writeln("auto ident = ctx.ident(code.ident().value());");
                    eval.writeln("result = make_eval_result(ident);");
                }
                else if (op == AbstractOp::CAST) {
                    eval.writeln("auto type = code.type().value();");
                    eval.writeln("auto ref = code.ref().value();");
                    eval.writeln("auto type_str = type_to_string(ctx, type);");
                    eval.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    // eval.writeln("result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);");
                    eval.writeln("result = make_eval_result(std::format(\"({}){}\", type_str, evaluated.result));");
                }
                else if (op == AbstractOp::FIELD_AVAILABLE) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("if(left_ref.value() == 0) {");
                    auto scope_1 = eval.indent_scope();
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("result = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                    scope_1.execute();
                    eval.writeln("}");
                    eval.writeln("else {");
                    auto scope_2 = eval.indent_scope();
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("ctx.this_as.push_back(left_eval.result);");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("result = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                    eval.writeln("ctx.this_as.pop_back();");
                    scope_2.execute();
                    eval.writeln("}");
                }
                else {
                    eval.writeln("result = make_eval_result(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                }
                eval.writeln("break;");
                scope.execute();
                eval.writeln("}");
            }
            if (is_parameter_related(op)) {
                if (op != AbstractOp::RETURN_TYPE) {
                    add_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    auto scope = add_parameter.indent_scope();
                    add_parameter.writeln("if(params > 0) {");
                    add_parameter.indent_writeln("w.write(\", \");");
                    add_parameter.writeln("}");
                    scope.execute();
                    if (op == AbstractOp::PROPERTY_INPUT_PARAMETER ||
                        op == AbstractOp::DEFINE_PARAMETER ||
                        op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                        if (op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                            add_parameter.indent_writeln("auto ref = code.ref().value();");
                            add_parameter.indent_writeln("auto type = type_to_string(ctx,ctx.ref(ref).type().value());");
                        }
                        else {
                            add_parameter.indent_writeln("auto ref = code.ident().value();");
                            add_parameter.indent_writeln("auto type = type_to_string(ctx,code.type().value());");
                        }
                        add_parameter.indent_writeln("auto ident = ctx.ident(ref);");
                        if (flags.prior_ident) {
                            add_parameter.indent_writeln("w.write(ident, \" \", type);");
                        }
                        else {
                            add_parameter.indent_writeln("w.write(type, \" \", ident);");
                        }
                    }
                    else {
                        add_parameter.indent_writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    }
                    add_parameter.indent_writeln("params++;");
                    add_parameter.indent_writeln("break;");
                    add_parameter.writeln("}");

                    add_call_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    auto scope_call = add_call_parameter.indent_scope();
                    add_call_parameter.writeln("if(params > 0) {");
                    add_call_parameter.indent_writeln("w.write(\", \");");
                    add_call_parameter.writeln("}");
                    scope_call.execute();
                    if (op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
                        add_call_parameter.indent_writeln("auto ref = code.ident().value();");
                        add_call_parameter.indent_writeln("auto ident = ctx.ident(ref);");
                        add_call_parameter.indent_writeln("w.write(ident);");
                    }
                    else {
                        add_call_parameter.indent_writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    }
                    add_call_parameter.indent_writeln("params++;");
                    add_call_parameter.indent_writeln("break;");
                    add_call_parameter.writeln("}");
                }
            }
            if ((!is_expr(op) && !is_struct_define_related(op) && !is_parameter_related(op)) || is_both_expr_and_def(op)) {
                inner_function.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                auto scope = inner_function.indent_scope();
                if (op == AbstractOp::APPEND) {
                    inner_function.writeln("auto vector_ref = code.left_ref().value();");
                    inner_function.writeln("auto new_element_ref = code.right_ref().value();");
                    inner_function.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);");
                    inner_function.writeln("auto new_element_eval = eval(ctx.ref(new_element_ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), new_element_eval.begin(), new_element_eval.end());");
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::ASSIGN) {
                    inner_function.writeln("auto left_ref = code.left_ref().value();");
                    inner_function.writeln("auto right_ref = code.right_ref().value();");
                    inner_function.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    inner_function.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    inner_function.writeln("w.writeln(\"\", left_eval.result, \" = \", right_eval.result, \"", flags.end_of_statement, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::DEFINE_VARIABLE) {
                    inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("auto type = type_to_string(ctx,code.type().value());");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);");
                    if (flags.prior_ident) {
                        inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} = {}", flags.end_of_statement, "\", ident, type, evaluated.result));");
                    }
                    else {
                        inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} = {}", flags.end_of_statement, "\",type, ident, evaluated.result));");
                    }
                    // inner_function.writeln("auto evaluated = eval(code, ctx);");
                    // inner_function.writeln("w.writeln(evaluated[evaluated.size() - 2]);");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::ASSERT) {
                    inner_function.writeln("auto evaluated = eval(ctx.ref(code.ref().value()), ctx);");
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::EXPLICIT_ERROR) {
                    inner_function.writeln("auto param = code.param().value();");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(param.refs[0]), ctx);");
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::IF || op == AbstractOp::ELIF ||
                         op == AbstractOp::ELSE || op == AbstractOp::LOOP_INFINITE ||
                         op == AbstractOp::LOOP_CONDITION || op == AbstractOp::DEFINE_FUNCTION) {
                    if (op == AbstractOp::IF || op == AbstractOp::ELIF || op == AbstractOp::LOOP_CONDITION) {
                        inner_function.writeln("auto ref = code.ref().value();");
                        inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    }
                    if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                        inner_function.writeln("defer.pop_back();");
                    }
                    auto indent = inner_function.indent_scope();
                    if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                        inner_function.writeln("w.writeln(\"}\");");
                    }
                    if (op == AbstractOp::DEFINE_FUNCTION) {
                        inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                        inner_function.writeln("auto range = ctx.range(code.ident().value());");
                        inner_function.writeln("auto found_type_pos = find_op(ctx,range,rebgn::AbstractOp::RETURN_TYPE);");
                        inner_function.writeln("if(!found_type_pos) {");
                        inner_function.writeln("w.writeln(\"void\");");
                        inner_function.writeln("}");
                        inner_function.writeln("else {");
                        inner_function.writeln("auto type = type_to_string(ctx, ctx.bm.code[*found_type_pos].type().value());");
                        inner_function.writeln("w.write(type);");
                        inner_function.writeln("}");
                        inner_function.writeln("w.write(\" \", ident, \"(\");");
                        inner_function.writeln("add_parameter(ctx, w, range);");
                        inner_function.writeln("w.writeln(\") ", flags.block_begin, "\");");
                    }
                    else {
                        inner_function.write("w.writeln(\"");
                        switch (op) {
                            case AbstractOp::IF:
                                inner_function.write("if (\",evaluated.result,\") ", flags.block_begin);
                                break;
                            case AbstractOp::ELIF:
                                inner_function.write("else if (\",evaluated.result,\") ", flags.block_begin);
                                break;
                            case AbstractOp::ELSE:
                                inner_function.write("else ", flags.block_begin);
                                break;
                            case AbstractOp::LOOP_INFINITE:
                                inner_function.write("for(;;) ", flags.block_begin);
                                break;
                            case AbstractOp::LOOP_CONDITION:
                                inner_function.write("while (\",evaluated.result,\") ", flags.block_begin);
                                break;
                        }
                        inner_function.writeln("\");");
                    }
                    indent.execute();
                    inner_function.writeln("defer.push_back(w.indent_scope_ex());");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::END_IF || op == AbstractOp::END_LOOP ||
                         op == AbstractOp::END_FUNCTION) {
                    inner_function.writeln("defer.pop_back();");
                    inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::CONTINUE) {
                    inner_function.writeln("w.writeln(\"continue", flags.end_of_statement, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::BREAK) {
                    inner_function.writeln("w.writeln(\"break", flags.end_of_statement, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET) {
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("if(ref.value() != 0) {");
                    auto scope = inner_function.indent_scope();
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    inner_function.writeln("w.writeln(\"return \", evaluated.result, \"", flags.end_of_statement, "\");");
                    scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else {");
                    auto else_scope = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"return", flags.end_of_statement, "\");");
                    else_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET_SUCCESS || op == AbstractOp::RET_PROPERTY_SETTER_OK) {
                    inner_function.writeln("w.writeln(\"return true", flags.end_of_statement, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET_PROPERTY_SETTER_FAIL) {
                    inner_function.writeln("w.writeln(\"return false", flags.end_of_statement, "\");");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::INC) {
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    inner_function.writeln("w.writeln(evaluated.result, \"+= 1", flags.end_of_statement, "\");");
                }
                else if (op == AbstractOp::ENCODE_INT || op == AbstractOp::DECODE_INT ||
                         op == AbstractOp::ENCODE_INT_VECTOR || op == AbstractOp::DECODE_INT_VECTOR ||
                         op == AbstractOp::ENCODE_INT_VECTOR_FIXED || op == AbstractOp::DECODE_INT_VECTOR_FIXED ||
                         op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                    inner_function.writeln("auto fallback = code.fallback().value();");
                    inner_function.writeln("if(fallback.value() != 0) {");
                    auto indent = inner_function.indent_scope();
                    inner_function.writeln("auto range = ctx.range(fallback);");
                    inner_function.writeln("inner_function(ctx, w, range);");
                    indent.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else {");
                    auto indent2 = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    indent2.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("break;");
                }
                else {
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    inner_function.writeln("break;");
                }
                scope.execute();
                inner_function.writeln("}");
            }
        }

        add_parameter.writeln("default: {");
        add_parameter.indent_writeln("// skip other op");
        add_parameter.indent_writeln("break;");
        add_parameter.writeln("}");
        scope_switch_add_parameter.execute();
        add_parameter.writeln("}");
        scope_nest_add_parameter.execute();
        add_parameter.writeln("}");  // close for
        scope_add_parameter.execute();
        add_parameter.writeln("}");  // close function

        add_call_parameter.writeln("default: {");
        add_call_parameter.indent_writeln("// skip other op");
        add_call_parameter.indent_writeln("break;");
        add_call_parameter.writeln("}");
        scope_switch_add_call_parameter.execute();
        add_call_parameter.writeln("}");
        scope_nest_add_call_parameter.execute();
        add_call_parameter.writeln("}");  // close for
        scope_add_call_parameter.execute();
        add_call_parameter.writeln("}");  // close function

        inner_block.writeln("default: {");
        auto if_block = inner_block.indent_scope();
        inner_block.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {");
        inner_block.indent_writeln("w.writeln(", unimplemented_comment("code.op"), ");");
        inner_block.writeln("}");
        inner_block.writeln("break;");
        if_block.execute();
        inner_block.writeln("}");  // close default
        inner_block.writeln("}");  // close switch
        scope_nest_block.execute();
        inner_block.writeln("}");  // close for
        scope_block.execute();
        inner_block.writeln("}");  // close function

        field_accessor.writeln("default: {");
        auto if_block_field_accessor = field_accessor.indent_scope();
        field_accessor.writeln("result = make_eval_result(", unimplemented_comment("code.op"), ");");
        field_accessor.writeln("break;");
        if_block_field_accessor.execute();
        field_accessor.writeln("}");  // close default
        field_accessor.writeln("}");  // close switch
        scope_field_accessor.execute();
        field_accessor.writeln("return result;");
        field_accessor.writeln("}");  // close function

        inner_function.writeln("default: {");
        auto if_function = inner_function.indent_scope();
        inner_function.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_struct_define_related(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {");
        inner_function.indent_writeln("w.writeln(", unimplemented_comment("code.op"), ");");
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
        eval.indent_writeln("result = make_eval_result(", unimplemented_comment("code.op"), ");");
        eval.indent_writeln("break;");
        eval.writeln("}");
        eval.write("}");  // close switch
        eval.writeln("return result;");
        scope_eval.execute();
        eval.writeln("}");  // close function

        w.write_unformatted(eval_result.out());
        w.write_unformatted(type_to_string.out());
        w.write_unformatted(field_accessor.out());
        w.write_unformatted(eval.out());
        w.write_unformatted(add_parameter.out());
        w.write_unformatted(add_call_parameter.out());
        w.write_unformatted(inner_block.out());
        w.write_unformatted(inner_function.out());
    }

    void code_main(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("#include \"bm2", flags.lang_name, ".hpp\"");
        w.writeln("#include <bm2/entry.hpp>");
        w.writeln("struct Flags : bm2::Flags {");
        w.indent_writeln("bm2", flags.lang_name, "::Flags bm2", flags.lang_name, "_flags;");
        w.indent_writeln("void bind(futils::cmdline::option::Context& ctx) {");
        auto scope_bind = w.indent_scope();
        w.indent_writeln("bm2::Flags::bind(ctx);");
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

    void code_header(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("#pragma once");
        w.writeln("#include <binary/writer.h>");
        w.writeln("#include <bm/binary_module.hpp>");
        w.writeln("namespace bm2", flags.lang_name, " {");
        auto scope = w.indent_scope();
        w.writeln("struct Flags {};");

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);");
        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);
    }

    void code_cmake(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("#license");
        w.writeln("cmake_minimum_required(VERSION 3.25)");
        w.writeln("project(bm2", flags.lang_name, ")");
        w.writeln("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool)");
        w.writeln("add_executable(bm2", flags.lang_name, " main.cpp bm2", flags.lang_name, ".cpp)");
        w.writeln("target_link_libraries(bm2", flags.lang_name, " futils)");
        w.writeln("install(TARGETS bm2", flags.lang_name, " DESTINATION tool)");
        w.writeln("if (\"$ENV{BUILD_MODE}\" STREQUAL \"web\")");
        w.writeln("  install(FILES \"${CMAKE_BINARY_DIR}/tool/bm2", flags.lang_name, ".wasm\" DESTINATION tool)");
        w.writeln("endif()");
    }

    void code_template(bm2::TmpCodeWriter& w, Flags& flags) {
        if (!flags.config_file.empty()) {
            futils::file::View config_file;
            if (auto res = config_file.open(flags.config_file); res) {
                auto parsed = futils::json::parse<futils::json::JSON>(config_file, true);
                if (parsed.is_undef()) {
                    futils::wrap::cerr_wrap() << "failed to parse json\n";
                    return;
                }
                if (!futils::json::convert_from_json(parsed, flags)) {
                    futils::wrap::cerr_wrap() << "failed to convert json to flags\n";
                    return;
                }
            }
        }
        if (flags.lang_name.empty()) {
            futils::wrap::cerr_wrap() << "--lang option is required\n";
            return;
        }
        if (flags.is_header) {
            code_header(w, flags);
            return;
        }
        if (flags.is_main) {
            code_main(w, flags);
            return;
        }
        if (flags.is_cmake) {
            code_cmake(w, flags);
            return;
        }

        w.writeln("/*license*/");
        w.writeln("#include <bm2/context.hpp>");
        w.writeln("#include <bm/helper.hpp>");
        w.writeln("#include \"bm2", flags.lang_name, ".hpp\"");
        w.writeln("namespace bm2", flags.lang_name, " {");
        auto scope = w.indent_scope();
        w.writeln("using TmpCodeWriter = bm2::TmpCodeWriter;");
        w.writeln("struct Context : bm2::Context {");
        auto scope_context = w.indent_scope();
        w.writeln("Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,\"r\",\"w\",\"(*this)\", std::move(escape_ident)} {}");
        scope_context.execute();
        w.writeln("};");

        write_impl_template(w, flags);

        futils::file::View keyword_file;
        if (auto res = keyword_file.open(flags.keyword_file); res && keyword_file.data()) {
            auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(keyword_file));
            w.writeln("std::string escape_", flags.lang_name, "_keyword(const std::string& str) {");
            auto scope_escape_ident = w.indent_scope();
            w.write("if (");
            for (size_t i = 0; i < lines.size(); i++) {
                if (i != 0) {
                    w.write(" || ");
                }
                w.write("str == \"", lines[i], "\"");
            }
            w.writeln(") {");
            w.indent_writeln("return str + \"_\";");
            w.writeln("}");
            w.writeln("return str;");
            scope_escape_ident.execute();
            w.writeln("}");
        }
        else {
            w.writeln("std::string escape_", flags.lang_name, "_keyword(const std::string& str) {");
            auto scope_escape_ident = w.indent_scope();
            w.writeln("if (str == \"if\" || str == \"for\" || str == \"else\" || str == \"break\" || str == \"continue\") {");
            w.indent_writeln("return str + \"_\";");
            w.writeln("}");
            w.writeln("return str;");
            scope_escape_ident.execute();
            w.writeln("}");
        }

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {");
        auto scope_to_xxx = w.indent_scope();
        w.writeln("Context ctx{w, bm, [&](bm2::Context& ctx, std::uint64_t id, auto&& str) {");
        auto scope_escape_key_ident = w.indent_scope();
        w.writeln("return escape_", flags.lang_name, "_keyword(str);");
        scope_escape_key_ident.execute();
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

        futils::file::View import_base_file;
        if (auto res = import_base_file.open(flags.import_base_file); res && import_base_file.data()) {
            auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(import_base_file));
            for (size_t i = 0; i < lines.size(); i++) {
                w.writeln("ctx.cw.writeln(\"", lines[i], "\");");
            }
        }

        w.writeln("for (size_t j = 0; j < bm.programs.ranges.size(); j++) {");
        w.indent_writeln("/* exclude DEFINE_PROGRAM and END_PROGRAM */");
        w.indent_writeln("TmpCodeWriter w;");
        w.indent_writeln("inner_block(ctx, w, rebgn::Range{.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});");
        w.indent_writeln("ctx.cw.write_unformatted(w.out());");
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
        w.writeln("ctx.cw.write_unformatted(w.out());");
        _scope.execute();
        w.writeln("}");

        scope_to_xxx.execute();
        w.writeln("}");

        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);
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
