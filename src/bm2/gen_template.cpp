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
#include "hook_list.hpp"
#include <filesystem>

struct Flags : futils::cmdline::templ::HelpOption {
    bool is_header = false;
    bool is_main = false;
    bool is_cmake = false;
    std::string_view config_file = "config.json";
    std::string_view hook_file_dir = "hook";
    bool debug = false;

    // json options
    std::string lang_name;
    std::string comment_prefix = "/*";
    std::string comment_suffix = "*/";
    std::string int_type_placeholder = "std::int{}_t";
    std::string uint_type_placeholder = "std::uint{}_t";
    std::string float_type_placeholder = "float{}_t";
    std::string array_type_placeholder = "std::array<{}, {}>";
    bool array_has_one_placeholder = false;
    std::string vector_type_placeholder = "std::vector<{}>";
    std::string optional_type_placeholder = "std::optional<{}>";
    std::string pointer_type_placeholder = "{}*";
    std::string bool_type = "bool";
    std::string true_literal = "true";
    std::string false_literal = "false";
    std::string coder_return_type = "bool";
    std::string property_setter_return_type = "bool";
    std::string end_of_statement = ";";
    std::string block_begin = "{";
    std::string block_end = "}";
    std::string block_end_type = "};";
    std::string struct_keyword = "struct";
    std::string enum_keyword = "enum";
    std::string define_var_keyword = "";
    std::string var_type_separator = " ";
    std::string field_type_separator = " ";
    std::string field_end = ";";
    std::string enum_member_end = ",";
    std::string func_keyword = "";
    std::string func_type_separator = " ";
    std::string func_void_return_type = "void";
    bool prior_ident = false;
    std::string if_keyword = "if";
    std::string elif_keyword = "else if";
    std::string else_keyword = "else";
    std::string infinity_loop = "for(;;)";
    std::string conditional_loop = "while";
    std::string self_ident = "(*this)";
    std::string param_type_separator = " ";
    bool condition_has_parentheses = true;
    std::string self_param = "";
    std::string encoder_param = "Encoder& w";
    std::string decoder_param = "Decoder& w";
    bool func_style_cast = false;

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
        FROM_JSON_OPT(optional_type_placeholder, "optional_type");
        FROM_JSON_OPT(pointer_type_placeholder, "pointer_type");
        FROM_JSON_OPT(bool_type, "bool_type");
        FROM_JSON_OPT(true_literal, "true_literal");
        FROM_JSON_OPT(false_literal, "false_literal");
        FROM_JSON_OPT(coder_return_type, "coder_return_type");
        FROM_JSON_OPT(property_setter_return_type, "property_setter_return_type");
        FROM_JSON_OPT(end_of_statement, "end_of_statement");
        FROM_JSON_OPT(block_begin, "block_begin");
        FROM_JSON_OPT(block_end, "block_end");
        FROM_JSON_OPT(block_end_type, "block_end_type");
        FROM_JSON_OPT(prior_ident, "prior_ident");
        FROM_JSON_OPT(struct_keyword, "struct_keyword");
        FROM_JSON_OPT(enum_keyword, "enum_keyword");
        FROM_JSON_OPT(define_var_keyword, "define_var_keyword");
        FROM_JSON_OPT(var_type_separator, "var_type_separator");
        FROM_JSON_OPT(field_type_separator, "field_type_separator");
        FROM_JSON_OPT(field_end, "field_end");
        FROM_JSON_OPT(enum_member_end, "enum_member_end");
        FROM_JSON_OPT(func_keyword, "func_keyword");
        FROM_JSON_OPT(func_type_separator, "func_type_separator");
        FROM_JSON_OPT(func_void_return_type, "func_void_return_type");
        FROM_JSON_OPT(if_keyword, "if_keyword");
        FROM_JSON_OPT(elif_keyword, "elif_keyword");
        FROM_JSON_OPT(else_keyword, "else_keyword");
        FROM_JSON_OPT(infinity_loop, "infinity_loop");
        FROM_JSON_OPT(conditional_loop, "conditional_loop");
        FROM_JSON_OPT(condition_has_parentheses, "condition_has_parentheses");
        FROM_JSON_OPT(self_ident, "self_ident");
        FROM_JSON_OPT(param_type_separator, "param_type_separator");
        FROM_JSON_OPT(self_param, "self_param");
        FROM_JSON_OPT(encoder_param, "encoder_param");
        FROM_JSON_OPT(decoder_param, "decoder_param");
        FROM_JSON_OPT(func_style_cast, "func_style_cast");
        JSON_PARAM_END()
    }

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&lang_name, "lang", "language name", "LANG");
        ctx.VarBool(&is_header, "header", "generate header file");
        ctx.VarBool(&is_main, "main", "generate main file");
        ctx.VarBool(&is_cmake, "cmake", "generate cmake file");
        ctx.VarBool(&debug, "debug", "debug mode (print hook call on stderr)");
        /*
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
        ctx.VarString(&func_keyword, "func-keyword", "function keyword", "KEYWORD");
        ctx.VarString(&func_type_separator, "func-type-separator", "function type separator", "SEPARATOR");
        ctx.VarString(&func_void_return_type, "func-void-return-type", "function void return type", "TYPE");
        ctx.VarString(&if_keyword, "if-keyword", "if keyword", "KEYWORD");
        ctx.VarString(&elif_keyword, "elif-keyword", "elif keyword", "KEYWORD");
        ctx.VarString(&else_keyword, "else-keyword", "else keyword", "KEYWORD");
        ctx.VarString(&infinity_loop, "infinity-loop", "infinity loop", "LOOP");
        ctx.VarString(&conditional_loop, "conditional-loop", "conditional loop", "LOOP");
        ctx.VarBool(&condition_has_parentheses, "condition-has-parentheses", "condition has parentheses");
        ctx.VarString(&self_ident, "self-name", "self name", "NAME");
        ctx.VarString(&param_type_separator, "param-type-separator", "parameter type separator", "SEPARATOR");
        ctx.VarBool(&explicit_self, "explicit-self", "explicit self");
        */
        ctx.VarString<true>(&config_file, "config-file", "config file", "FILE");
        ctx.VarString<true>(&hook_file_dir, "hook-dir", "hook file directory", "DIR");
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
    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, auto&& file_name, auto&& per_line) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        if (flags.debug) {
            futils::wrap::cerr_wrap() << "try hook: " << hook_file.generic_u8string() << '\n';
        }
        futils::file::View view;
        if (auto res = view.open(hook_file.generic_u8string()); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "hit hook: " << hook_file.generic_u8string() << '\n';
        for (auto i = 0; i < lines.size(); i++) {
            per_line(i, lines[i]);
        }
        return true;
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, auto&& per_line) {
        return may_write_from_hook(w, flags, to_string(hook), per_line);
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, const char* add = "") {
        auto op_name = to_string(op);
        auto concat = std::format("{}{}", op_name, add);
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(w, flags, std::vformat(to_string(hook), std::make_format_args(concat)), [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, const char* add = "") {
        auto op_name = to_string(op);
        auto concat = std::format("{}{}", op_name, add);
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(w, flags, std::vformat(to_string(hook), std::make_format_args(concat)), [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code) {
        return may_write_from_hook(w, flags, hook, [&](size_t i, futils::view::rvec& line) {
            if (as_code) {
                if (!futils::strutil::contains(line, "\"") && !futils::strutil::contains(line, "\\")) {
                    w.writeln("w.writeln(\"", line, "\");");
                }
                else {
                    std::string escaped_line;
                    for (auto c : line) {
                        if (c == '"' || c == '\\') {
                            escaped_line.push_back('\\');
                        }
                        escaped_line.push_back(c);
                    }
                    w.writeln("w.writeln(\"", escaped_line, "\");");
                }
            }
            else {
                w.writeln(line);
            }
        });
    }

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
            auto type_hook = [&](auto&& default_action) {
                if (may_write_from_hook(w, flags, bm2::HookFile::type_op, type)) {
                    return;
                }
                default_action();
            };
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
                type_hook([&] {
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
                });
            }
            else if (type == StorageType::STRUCT_REF) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_hook([&] {
                    type_to_string.writeln("return ident;");
                });
            }
            else if (type == StorageType::RECURSIVE_STRUCT_REF) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_hook([&] {
                    type_to_string.writeln("return std::format(\"{}*\", ident);");
                });
            }
            else if (type == StorageType::BOOL) {
                type_hook([&] {
                    type_to_string.writeln("return \"", flags.bool_type, "\";");
                });
            }
            else if (type == StorageType::ENUM) {
                type_to_string.writeln("auto ref = storage.ref().value().value();");
                type_to_string.writeln("auto& ident = ctx.ident_table[ref];");
                type_hook([&] {
                    type_to_string.writeln("return ident;");
                });
            }
            else if (type == StorageType::VARIANT) {
                type_hook([&] {
                    type_to_string.writeln("return \"", flags.wrap_comment("Unimplemented VARIANT"), "\";");
                });
            }
            else if (type == StorageType::CODER_RETURN) {
                type_hook([&] {
                    type_to_string.writeln("return \"", flags.coder_return_type, "\";");
                });
            }
            else if (type == StorageType::PROPERTY_SETTER_RETURN) {
                type_hook([&] {
                    type_to_string.writeln("return \"", flags.property_setter_return_type, "\";");
                });
            }
            else if (type == StorageType::PTR) {
                type_hook([&] {
                    type_to_string.writeln("return std::format(\"", flags.pointer_type_placeholder, "\", base_type);");
                });
            }
            else if (type == StorageType::ARRAY) {
                type_to_string.writeln("auto length = storage.size().value().value();");
                type_hook([&] {
                    if (flags.array_has_one_placeholder) {
                        type_to_string.writeln("return std::format(\"", flags.array_type_placeholder, "\", base_type);");
                    }
                    else {
                        type_to_string.writeln("return std::format(\"", flags.array_type_placeholder, "\", base_type,length);");
                    }
                });
            }
            else if (type == StorageType::VECTOR) {
                type_hook([&] {
                    type_to_string.writeln("return std::format(\"", flags.vector_type_placeholder, "\", base_type);");
                });
            }
            else if (type == StorageType::OPTIONAL) {
                type_hook([&] {
                    type_to_string.writeln("return std::format(\"", flags.optional_type_placeholder, "\", base_type);");
                });
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
        add_parameter.writeln("auto belong = ctx.bm.code[range.start].belong().value();");
        add_parameter.writeln("auto is_member = belong.value() != 0 && ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM;");
        if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_start, false)) {
            if (flags.self_param.size()) {
                add_parameter.writeln("if(is_member) {");
                auto if_block_belong = add_parameter.indent_scope();
                add_parameter.writeln("w.write(\"", flags.self_param, "\");");
                add_parameter.writeln("params++;");
                if_block_belong.execute();
                add_parameter.writeln("}");
            }
        }
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
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_start, false);
        inner_block.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_block = inner_block.indent_scope();
        inner_block.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_each_code, false);
        inner_block.writeln("switch(code.op) {");

        bm2::TmpCodeWriter inner_function;
        inner_function.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_function = inner_function.indent_scope();
        inner_function.writeln("std::vector<futils::helper::DynDefer> defer;");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_start, false);
        inner_function.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_function = inner_function.indent_scope();
        inner_function.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_each_code, false);
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
                auto block_hook = [&](auto&& inner) {
                    if (!may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_op, op)) {
                        inner();
                    }
                };
                if (op == AbstractOp::DECLARE_FORMAT || op == AbstractOp::DECLARE_ENUM ||
                    op == AbstractOp::DECLARE_STATE || op == AbstractOp::DECLARE_PROPERTY) {
                    inner_block.indent_writeln("auto ref = code.ref().value();");
                    inner_block.indent_writeln("auto range = ctx.range(ref);");
                    block_hook([&] {
                        inner_block.indent_writeln("inner_block(ctx, w, range);");
                    });
                }
                else if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    block_hook([&] {
                        inner_block.indent_writeln("w.writeln(\"", flags.struct_keyword, " \", ident, \" ", flags.block_begin, "\");");
                        inner_block.indent_writeln("defer.push_back(w.indent_scope_ex());");
                    });
                }
                else if (op == AbstractOp::DEFINE_ENUM) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    block_hook([&] {
                        inner_block.indent_writeln("w.writeln(\"", flags.enum_keyword, " \", ident, \" ", flags.block_begin, "\");");
                        inner_block.indent_writeln("defer.push_back(w.indent_scope_ex());");
                    });
                }
                else if (op == AbstractOp::DEFINE_ENUM_MEMBER) {
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_block.indent_writeln("auto evaluated = eval(ctx.ref(code.left_ref().value()), ctx);");
                    block_hook([&] {
                        inner_block.indent_writeln("w.writeln(ident, \" = \", evaluated.result, \"", flags.enum_member_end, "\");");
                    });
                }
                else if (op == AbstractOp::DEFINE_FIELD) {
                    inner_block.indent_writeln("if (ctx.ref(code.belong().value()).op == rebgn::AbstractOp::DEFINE_PROGRAM) {");
                    auto scope = inner_block.indent_scope();
                    inner_block.indent_writeln("break;");
                    scope.execute();
                    inner_block.indent_writeln("}");
                    inner_block.indent_writeln("auto type = type_to_string(ctx, code.type().value());");
                    inner_block.indent_writeln("auto ident = ctx.ident(code.ident().value());");
                    block_hook([&] {
                        if (flags.prior_ident) {
                            inner_block.indent_writeln("w.writeln(ident, \" ", flags.field_type_separator, "\", type, \"", flags.field_end, "\");");
                        }
                        else {
                            inner_block.indent_writeln("w.writeln(type, \" ", flags.field_type_separator, "\", ident, \"", flags.field_end, "\");");
                        }
                    });
                }
                else if (op == AbstractOp::END_FORMAT || op == AbstractOp::END_ENUM || op == AbstractOp::END_STATE) {
                    block_hook([&] {
                        inner_block.indent_writeln("defer.pop_back();");
                        inner_block.indent_writeln("w.writeln(\"", flags.block_end_type, "\");");
                    });
                }
                else {
                    block_hook([&] {
                        inner_block.indent_writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    });
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
                auto eval_hook = [&](auto&& default_action, const char* stage = "") {
                    if (!may_write_from_hook(eval, flags, bm2::HookFile::eval_op, op, stage)) {
                        default_action();
                    }
                };
                if (op == AbstractOp::BINARY) {
                    eval.writeln("auto op = code.bop().value();");
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);");
                    eval.writeln("auto opstr = to_string(op);");
                    eval_hook([&] {
                        eval_hook([&] {}, "_op");
                        eval.writeln("result = make_eval_result(std::format(\"({} {} {})\", left_eval.result, opstr, right_eval.result));");
                    });
                }
                else if (op == AbstractOp::UNARY) {
                    eval.writeln("auto op = code.uop().value();");
                    eval.writeln("auto ref = code.ref().value();");
                    eval.writeln("auto target = eval(ctx.ref(ref), ctx);");
                    eval.writeln("auto opstr = to_string(op);");
                    eval_hook([&] {
                        eval_hook([&] {}, "_op");
                        eval.writeln("result = make_eval_result(std::format(\"({}{})\", opstr, target.result));");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_INT) {
                    eval.writeln("auto value = code.int_value()->value();");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"{}\", value));");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_TRUE) {
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(\"", flags.true_literal, "\");");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_FALSE) {
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(\"", flags.false_literal, "\");");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_INT64) {
                    eval.writeln("auto value = *code.int_value64();");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"{}\", value));");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_TYPE) {
                    eval.writeln("auto type = code.type().value();");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(type_to_string(ctx, type));");
                    });
                }
                else if (op == AbstractOp::IMMEDIATE_CHAR) {
                    eval.writeln("auto char_code = code.int_value()->value();");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"'{}'\", char_code));");
                    });
                }
                else if (op == AbstractOp::PHI || op == AbstractOp::DECLARE_VARIABLE ||
                         op == AbstractOp::DEFINE_VARIABLE_REF) {
                    eval.writeln("auto ref=code.ref().value();");
                    eval_hook([&] {
                        eval.writeln("return eval(ctx.ref(ref), ctx);");
                    });
                }
                else if (op == AbstractOp::ACCESS) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_ident = ctx.ident(right_ref);");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"{}.{}\", left_eval.result, right_ident));");
                    });
                }
                else if (op == AbstractOp::INDEX) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"{}[{}]\", left_eval.result, right_eval.result));");
                    });
                }
                else if (op == AbstractOp::ARRAY_SIZE) {
                    eval.writeln("auto vector_ref = code.ref().value();");
                    eval.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(std::format(\"{}.size()\", vector_eval.result));");
                    });
                }
                else if (op == AbstractOp::DEFINE_FIELD) {
                    eval.writeln("result = field_accessor(code, ctx);");
                }
                else if (op == AbstractOp::DEFINE_VARIABLE || op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
                    eval.writeln("auto ident = ctx.ident(code.ident().value());");
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(ident);");
                    });
                }
                else if (op == AbstractOp::CAST) {
                    eval.writeln("auto type = code.type().value();");
                    eval.writeln("auto from_type = code.from_type().value();");
                    eval.writeln("auto ref = code.ref().value();");
                    eval.writeln("auto type_str = type_to_string(ctx, type);");
                    eval.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    // eval.writeln("result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);");
                    eval_hook([&] {
                        if (flags.func_style_cast) {
                            eval.writeln("result = make_eval_result(std::format(\"{}({})\", type_str, evaluated.result));");
                        }
                        else {
                            eval.writeln("result = make_eval_result(std::format(\"({}){}\", type_str, evaluated.result));");
                        }
                    });
                }
                else if (op == AbstractOp::FIELD_AVAILABLE) {
                    eval.writeln("auto left_ref = code.left_ref().value();");
                    eval.writeln("if(left_ref.value() == 0) {");
                    auto scope_1 = eval.indent_scope();
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval_hook([&] {
                        eval.writeln("result = eval(ctx.ref(right_ref), ctx);");
                    },
                              "_self");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                    scope_1.execute();
                    eval.writeln("}");
                    eval.writeln("else {");
                    auto scope_2 = eval.indent_scope();
                    eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
                    eval.writeln("ctx.this_as.push_back(left_eval.result);");
                    eval.writeln("auto right_ref = code.right_ref().value();");
                    eval_hook([&] {
                        eval.writeln("result = eval(ctx.ref(right_ref), ctx);");
                    },
                              "_field");
                    // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
                    eval.writeln("ctx.this_as.pop_back();");
                    scope_2.execute();
                    eval.writeln("}");
                }
                else {
                    eval_hook([&] {
                        eval.writeln("result = make_eval_result(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    });
                }
                eval.writeln("break;");
                scope.execute();
                eval.writeln("}");
            }
            if (is_parameter_related(op)) {
                if (op != AbstractOp::RETURN_TYPE) {
                    add_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    auto scope = add_parameter.indent_scope();
                    auto param_hook = [&](auto&& inner, const char* stage = "") {
                        if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_op, op, stage)) {
                            inner();
                        }
                    };
                    if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_each_code, false)) {
                        add_parameter.writeln("if(params > 0) {");
                        add_parameter.indent_writeln("w.write(\", \");");
                        add_parameter.writeln("}");
                    }
                    if (op == AbstractOp::PROPERTY_INPUT_PARAMETER ||
                        op == AbstractOp::DEFINE_PARAMETER ||
                        op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                        if (op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                            add_parameter.writeln("auto ref = code.ref().value();");
                            add_parameter.writeln("auto type = type_to_string(ctx,ctx.ref(ref).type().value());");
                        }
                        else {
                            add_parameter.writeln("auto ref = code.ident().value();");
                            add_parameter.writeln("auto type = type_to_string(ctx,code.type().value());");
                        }
                        add_parameter.writeln("auto ident = ctx.ident(ref);");
                        param_hook([&] {
                            if (flags.prior_ident) {
                                add_parameter.writeln("w.write(ident, \" ", flags.param_type_separator, "\", type);");
                            }
                            else {
                                add_parameter.writeln("w.write(type, \" ", flags.param_type_separator, "\", ident);");
                            }
                            add_parameter.writeln("params++;");
                        });
                    }
                    else if (op == AbstractOp::ENCODER_PARAMETER) {
                        param_hook([&] {
                            add_parameter.writeln("w.write(\"", flags.encoder_param, "\");");
                            add_parameter.writeln("params++;");
                        });
                    }
                    else if (op == AbstractOp::DECODER_PARAMETER) {
                        param_hook([&] {
                            add_parameter.writeln("w.write(\"", flags.decoder_param, "\");");
                            add_parameter.writeln("params++;");
                        });
                    }
                    else {
                        param_hook([&] {
                            add_parameter.writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                            add_parameter.writeln("params++;");
                        });
                    }
                    add_parameter.writeln("break;");
                    scope.execute();
                    add_parameter.writeln("}");

                    add_call_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                    auto scope_call = add_call_parameter.indent_scope();
                    auto call_param_hook = [&](auto&& inner, const char* stage = "") {
                        if (!may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_op, op, stage)) {
                            inner();
                        }
                    };
                    if (!may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_each_code, false)) {
                        add_call_parameter.writeln("if(params > 0) {");
                        add_call_parameter.indent_writeln("w.write(\", \");");
                        add_call_parameter.writeln("}");
                    }
                    if (op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
                        add_call_parameter.writeln("auto ref = code.ident().value();");
                        add_call_parameter.writeln("auto ident = ctx.ident(ref);");
                        call_param_hook([&] {
                            add_call_parameter.writeln("w.write(ident);");
                            add_call_parameter.writeln("params++;");
                        });
                    }
                    else {
                        call_param_hook([&] {
                            add_call_parameter.writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                            add_call_parameter.writeln("params++;");
                        });
                    }
                    add_call_parameter.writeln("break;");
                    scope_call.execute();
                    add_call_parameter.writeln("}");
                }
            }
            if ((!is_expr(op) && !is_struct_define_related(op) && !is_parameter_related(op)) || is_both_expr_and_def(op)) {
                inner_function.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
                auto scope = inner_function.indent_scope();
                auto func_hook = [&](auto&& inner, const char* stage = "") {
                    if (!may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_op, op, stage)) {
                        inner();
                    }
                };
                if (op == AbstractOp::APPEND) {
                    inner_function.writeln("auto vector_ref = code.left_ref().value();");
                    inner_function.writeln("auto new_element_ref = code.right_ref().value();");
                    inner_function.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);");
                    inner_function.writeln("auto new_element_eval = eval(ctx.ref(new_element_ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), new_element_eval.begin(), new_element_eval.end());");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"\", vector_eval.result, \".push_back(\", new_element_eval.result, \")", flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::ASSIGN) {
                    inner_function.writeln("auto left_ref = code.left_ref().value();");
                    inner_function.writeln("auto right_ref = code.right_ref().value();");
                    inner_function.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
                    inner_function.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"\", left_eval.result, \" = \", right_eval.result, \"", flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::DEFINE_VARIABLE) {
                    inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("auto type = type_to_string(ctx,code.type().value());");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    // inner_function.writeln("result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);");
                    func_hook([&] {
                        if (flags.prior_ident) {
                            inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} = {}", flags.end_of_statement, "\", ident, type, evaluated.result));");
                        }
                        else {
                            inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} = {}", flags.end_of_statement, "\",type, ident, evaluated.result));");
                        }
                    });
                    // inner_function.writeln("auto evaluated = eval(code, ctx);");
                    // inner_function.writeln("w.writeln(evaluated[evaluated.size() - 2]);");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::ASSERT) {
                    inner_function.writeln("auto evaluated = eval(ctx.ref(code.ref().value()), ctx);");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"assert(\", evaluated.result, \")", flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::EXPLICIT_ERROR) {
                    inner_function.writeln("auto param = code.param().value();");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(param.refs[0]), ctx);");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"throw std::runtime_error(\", evaluated.result, \")", flags.end_of_statement, "\");");
                    });
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
                    if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                        inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
                    }
                    if (op == AbstractOp::DEFINE_FUNCTION) {
                        inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                        inner_function.writeln("auto range = ctx.range(code.ident().value());");
                        inner_function.writeln("auto found_type_pos = find_op(ctx,range,rebgn::AbstractOp::RETURN_TYPE);");
                        inner_function.writeln("std::optional<std::string> type;");
                        inner_function.writeln("if(found_type_pos) {");
                        {
                            auto inner_scope = inner_function.indent_scope();
                            inner_function.writeln("auto type_ref = ctx.bm.code[*found_type_pos].type().value();");
                            inner_function.writeln("type = type_to_string(ctx,type_ref);");
                        }
                        inner_function.writeln("}");
                        func_hook([&] {
                            if (!flags.func_keyword.size()) {
                                inner_function.writeln("if(type) {");
                                inner_function.indent_writeln("w.write(*type);");
                                inner_function.writeln("}");
                                inner_function.writeln("else {");
                                inner_function.indent_writeln("w.write(\"", flags.func_void_return_type, "\");");
                                inner_function.writeln("}");
                            }
                            else {
                                inner_function.writeln("w.write(\"", flags.func_keyword, " \");");
                            }
                            inner_function.writeln("w.write(\" \", ident, \"(\");");
                            inner_function.writeln("add_parameter(ctx, w, range);");
                            inner_function.writeln("w.write(\") \");");
                            if (flags.func_keyword.size()) {
                                inner_function.writeln("if(type) {");
                                inner_function.indent_writeln("w.write(\"", flags.func_type_separator, "\", *type);");
                                inner_function.writeln("}");
                                inner_function.writeln("else {");
                                inner_function.indent_writeln("w.write(\"", flags.func_void_return_type, "\");");
                                inner_function.writeln("}");
                            }
                        });
                        inner_function.writeln("w.writeln(\"", flags.block_begin, "\");");
                    }
                    else {
                        func_hook([&] {
                            inner_function.write("w.writeln(\"");
                            std::string condition = "\",evaluated.result,\"";
                            if (flags.condition_has_parentheses) {
                                condition = "(" + condition + ")";
                            }
                            switch (op) {
                                case AbstractOp::IF:
                                    inner_function.write(flags.if_keyword, " ", condition, " ", flags.block_begin);
                                    break;
                                case AbstractOp::ELIF:
                                    inner_function.write(flags.elif_keyword, " ", condition, " ", flags.block_begin);
                                    break;
                                case AbstractOp::ELSE:
                                    inner_function.write(flags.else_keyword, " ", flags.block_begin);
                                    break;
                                case AbstractOp::LOOP_INFINITE:
                                    inner_function.write(flags.infinity_loop, " ", flags.block_begin);
                                    break;
                                case AbstractOp::LOOP_CONDITION:
                                    inner_function.write(flags.conditional_loop, " ", condition, " ", flags.block_begin);
                                    break;
                            }
                            inner_function.writeln("\");");
                        });
                    }
                    inner_function.writeln("defer.push_back(w.indent_scope_ex());");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::END_IF || op == AbstractOp::END_LOOP ||
                         op == AbstractOp::END_FUNCTION) {
                    inner_function.writeln("defer.pop_back();");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::CONTINUE) {
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"continue", flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::BREAK) {
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"break", flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET) {
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("if(ref.value() != 0) {");
                    auto scope = inner_function.indent_scope();
                    func_hook([&] {
                        inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                        inner_function.writeln("w.writeln(\"return \", evaluated.result, \"", flags.end_of_statement, "\");");
                    },
                              "_value");
                    scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else {");
                    auto else_scope = inner_function.indent_scope();
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"return", flags.end_of_statement, "\");");
                    },
                              "_empty");
                    else_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET_SUCCESS || op == AbstractOp::RET_PROPERTY_SETTER_OK) {
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"return ", flags.true_literal, flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::RET_PROPERTY_SETTER_FAIL) {
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"return ", flags.false_literal, flags.end_of_statement, "\");");
                    });
                    inner_function.writeln("break;");
                }
                else if (op == AbstractOp::INC) {
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                    func_hook([&] {
                        inner_function.writeln("w.writeln(evaluated.result, \"+= 1", flags.end_of_statement, "\");");
                    });
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
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    });
                    indent2.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("break;");
                }
                else {
                    func_hook([&] {
                        inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    });
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
        may_write_from_hook(w, flags, bm2::HookFile::bm_context, false);
        w.writeln("Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,\"r\",\"w\",\"", flags.self_ident, "\", std::move(escape_ident)} {}");
        scope_context.execute();
        w.writeln("};");

        write_impl_template(w, flags);

        w.writeln("std::string escape_", flags.lang_name, "_keyword(const std::string& str) {");
        auto scope_escape_ident = w.indent_scope();
        w.write("if (");
        if (!may_write_from_hook(w, flags, bm2::HookFile::keyword, [&](size_t i, futils::view::rvec keyword) {
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

        bm2::TmpCodeWriter tmp;
        if (may_write_from_hook(tmp, flags, bm2::HookFile::file_top, true)) {
            w.writeln("{");
            auto scope = w.indent_scope();
            w.writeln("auto& w = ctx.cw;");
            w.write_unformatted(tmp.out());
            scope.execute();
            w.writeln("}");
            tmp.out().clear();
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

        if (may_write_from_hook(w, flags, bm2::HookFile::file_bottom, true)) {
            w.writeln("{");
            auto scope = w.indent_scope();
            w.writeln("auto& w = ctx.cw;");
            w.write_unformatted(tmp.out());
            scope.execute();
            w.writeln("}");
        }

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
