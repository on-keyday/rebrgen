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

struct Flags : futils::cmdline::templ::HelpOption {
    bool is_header = false;
    bool is_main = false;
    bool is_cmake = false;
    bool is_config = false;
    std::string_view config_file = "config.json";
    std::string_view hook_file_dir = "hook";
    bool debug = false;
    bool print_hooks = false;

    // json options
    std::string lang_name;
    std::string file_suffix = "";
    std::string comment_prefix = "/*";
    std::string comment_suffix = "*/";
    std::string int_type_placeholder = "std::int{}_t";
    std::string uint_type_placeholder = "std::uint{}_t";
    std::string float_type_placeholder = "float{}_t";
    std::string array_type_placeholder = "std::array<$TYPE, $LEN>";
    std::string vector_type_placeholder = "std::vector<{}>";
    std::string optional_type_placeholder = "std::optional<{}>";
    std::string pointer_type_placeholder = "{}*";
    std::string recursive_struct_type_placeholder = "{}*";
    std::string byte_vector_type = "";
    std::string byte_array_type = "";
    std::string bool_type = "bool";
    std::string true_literal = "true";
    std::string false_literal = "false";
    std::string coder_return_type = "bool";
    std::string property_setter_return_type = "bool";
    std::string end_of_statement = ";";
    std::string block_begin = "{";
    std::string block_end = "}";
    bool otbs_on_block_end = false;  // like golang else style
    std::string block_end_type = "};";
    std::string struct_keyword = "struct";
    std::string enum_keyword = "enum";
    std::string union_keyword = "union";
    std::string define_var_keyword = "";
    std::string var_type_separator = " ";
    std::string define_var_assign = "=";
    bool omit_type_on_define_var = false;
    std::string field_type_separator = " ";
    std::string field_end = ";";
    std::string enum_member_end = ",";
    std::string func_keyword = "";
    bool trailing_return_type = false;
    std::string func_brace_ident_separator = "";
    std::string func_type_separator = " ";
    std::string func_void_return_type = "void";
    bool prior_ident = false;
    std::string if_keyword = "if";
    std::string elif_keyword = "else if";
    std::string else_keyword = "else";
    std::string infinity_loop = "for(;;)";
    std::string conditional_loop = "while";
    std::string match_keyword = "switch";
    std::string match_case_keyword = "case";
    std::string match_case_separator = ":";
    std::string match_default_keyword = "default";
    std::string self_ident = "(*this)";
    std::string param_type_separator = " ";
    bool condition_has_parentheses = true;
    std::string self_param = "";
    std::string encoder_param = "Encoder& w";
    std::string decoder_param = "Decoder& w";
    bool func_style_cast = false;
    std::string empty_pointer = "nullptr";
    std::string empty_optional = "std::nullopt";
    std::string size_method = "size";
    bool surrounded_size_method = false;  // if true, <size_method>(<expr>) will be used
    std::string append_method = "push_back";
    bool surrounded_append_method = false;  // if true, <base> = <append_method>(<base>,<expr>) will be used
    std::string variant_mode = "union";     // union or algebraic
    std::string algebraic_variant_separator = "|";
    std::string algebraic_variant_placeholder = "{}";
    std::string check_union_condition = "!std::holds_alternative<$MEMBER_INDEX>($FIELD_IDENT)";
    std::string check_union_fail_return_value = "false";
    std::string switch_union = "$FIELD_IDENT = $MEMBER_IDENT()";
    std::string address_of_placeholder = "&{}";
    std::string optional_of_placeholder = "{}";
    std::string decode_bytes_op = "$VALUE = $DECODER.decode_bytes($LEN)";
    std::string encode_bytes_op = "$ENCODER.encode_bytes($VALUE)";
    std::string decode_bytes_until_eof_op = "$VALUE = $DECODER.decode_bytes_until_eof()";
    std::string peek_bytes_op = "$VALUE = $DECODER.peek_bytes($LEN)";
    std::string encode_offset = "$ENCODER.offset()";
    std::string decode_offset = "$DECODER.offset()";
    std::string encode_backward = "$ENCODER.backward($OFFSET)";
    std::string decode_backward = "$DECODER.backward($OFFSET)";
    std::string is_little_endian = "std::endian::native == std::endian::little";
    std::string default_enum_base = "";  // if empty, omit the base type
    std::string enum_base_separator = " : ";

    std::string eval_result_text = "$RESULT = make_eval_result($TEXT);";
    std::string eval_result_passthrough = "$RESULT = $TEXT;";

#define MAP_TO_MACRO(MACRO_NAME)                                               \
    MACRO_NAME(lang_name, "lang")                                              \
    MACRO_NAME(file_suffix, "suffix")                                          \
    MACRO_NAME(comment_prefix, "comment_prefix")                               \
    MACRO_NAME(comment_suffix, "comment_suffix")                               \
    MACRO_NAME(int_type_placeholder, "int_type")                               \
    MACRO_NAME(uint_type_placeholder, "uint_type")                             \
    MACRO_NAME(float_type_placeholder, "float_type")                           \
    MACRO_NAME(array_type_placeholder, "array_type")                           \
    MACRO_NAME(vector_type_placeholder, "vector_type")                         \
    MACRO_NAME(byte_vector_type, "byte_vector_type")                           \
    MACRO_NAME(byte_array_type, "byte_array_type")                             \
    MACRO_NAME(optional_type_placeholder, "optional_type")                     \
    MACRO_NAME(pointer_type_placeholder, "pointer_type")                       \
    MACRO_NAME(recursive_struct_type_placeholder, "recursive_struct_type")     \
    MACRO_NAME(bool_type, "bool_type")                                         \
    MACRO_NAME(true_literal, "true_literal")                                   \
    MACRO_NAME(false_literal, "false_literal")                                 \
    MACRO_NAME(coder_return_type, "coder_return_type")                         \
    MACRO_NAME(property_setter_return_type, "property_setter_return_type")     \
    MACRO_NAME(end_of_statement, "end_of_statement")                           \
    MACRO_NAME(block_begin, "block_begin")                                     \
    MACRO_NAME(block_end, "block_end")                                         \
    MACRO_NAME(otbs_on_block_end, "otbs_on_block_end")                         \
    MACRO_NAME(block_end_type, "block_end_type")                               \
    MACRO_NAME(prior_ident, "prior_ident")                                     \
    MACRO_NAME(struct_keyword, "struct_keyword")                               \
    MACRO_NAME(enum_keyword, "enum_keyword")                                   \
    MACRO_NAME(define_var_keyword, "define_var_keyword")                       \
    MACRO_NAME(var_type_separator, "var_type_separator")                       \
    MACRO_NAME(define_var_assign, "define_var_assign")                         \
    MACRO_NAME(omit_type_on_define_var, "omit_type_on_define_var")             \
    MACRO_NAME(field_type_separator, "field_type_separator")                   \
    MACRO_NAME(field_end, "field_end")                                         \
    MACRO_NAME(enum_member_end, "enum_member_end")                             \
    MACRO_NAME(func_keyword, "func_keyword")                                   \
    MACRO_NAME(trailing_return_type, "trailing_return_type")                   \
    MACRO_NAME(func_brace_ident_separator, "func_brace_ident_separator")       \
    MACRO_NAME(func_type_separator, "func_type_separator")                     \
    MACRO_NAME(func_void_return_type, "func_void_return_type")                 \
    MACRO_NAME(if_keyword, "if_keyword")                                       \
    MACRO_NAME(elif_keyword, "elif_keyword")                                   \
    MACRO_NAME(else_keyword, "else_keyword")                                   \
    MACRO_NAME(infinity_loop, "infinity_loop")                                 \
    MACRO_NAME(conditional_loop, "conditional_loop")                           \
    MACRO_NAME(match_keyword, "match_keyword")                                 \
    MACRO_NAME(match_case_keyword, "match_case_keyword")                       \
    MACRO_NAME(match_case_separator, "match_case_separator")                   \
    MACRO_NAME(match_default_keyword, "match_default_keyword")                 \
    MACRO_NAME(condition_has_parentheses, "condition_has_parentheses")         \
    MACRO_NAME(self_ident, "self_ident")                                       \
    MACRO_NAME(param_type_separator, "param_type_separator")                   \
    MACRO_NAME(self_param, "self_param")                                       \
    MACRO_NAME(encoder_param, "encoder_param")                                 \
    MACRO_NAME(decoder_param, "decoder_param")                                 \
    MACRO_NAME(func_style_cast, "func_style_cast")                             \
    MACRO_NAME(empty_pointer, "empty_pointer")                                 \
    MACRO_NAME(empty_optional, "empty_optional")                               \
    MACRO_NAME(size_method, "size_method")                                     \
    MACRO_NAME(surrounded_size_method, "surrounded_size_method")               \
    MACRO_NAME(append_method, "append_method")                                 \
    MACRO_NAME(surrounded_append_method, "surrounded_append_method")           \
    MACRO_NAME(variant_mode, "variant_mode")                                   \
    MACRO_NAME(algebraic_variant_separator, "algebraic_variant_separator")     \
    MACRO_NAME(algebraic_variant_placeholder, "algebraic_variant_type")        \
    MACRO_NAME(check_union_condition, "check_union_condition")                 \
    MACRO_NAME(check_union_fail_return_value, "check_union_fail_return_value") \
    MACRO_NAME(switch_union, "switch_union")                                   \
    MACRO_NAME(address_of_placeholder, "address_of_placeholder")               \
    MACRO_NAME(optional_of_placeholder, "optional_of_placeholder")             \
    MACRO_NAME(decode_bytes_op, "decode_bytes_op")                             \
    MACRO_NAME(encode_bytes_op, "encode_bytes_op")                             \
    MACRO_NAME(decode_bytes_until_eof_op, "decode_bytes_until_eof_op")         \
    MACRO_NAME(peek_bytes_op, "peek_bytes_op")                                 \
    MACRO_NAME(encode_offset, "encode_offset")                                 \
    MACRO_NAME(decode_offset, "decode_offset")                                 \
    MACRO_NAME(encode_backward, "encode_backward")                             \
    MACRO_NAME(decode_backward, "decode_backward")                             \
    MACRO_NAME(is_little_endian, "is_little_endian_expr")                      \
    MACRO_NAME(default_enum_base, "default_enum_base")                         \
    MACRO_NAME(enum_base_separator, "enum_base_separator")                     \
    MACRO_NAME(eval_result_text, "eval_result_text")                           \
    MACRO_NAME(eval_result_passthrough, "eval_result_passthrough")

    bool from_json(const futils::json::JSON& js) {
        JSON_PARAM_BEGIN(*this, js)
        MAP_TO_MACRO(FROM_JSON_OPT)
        JSON_PARAM_END()
    }

    bool to_json(auto&& js) const {
        JSON_PARAM_BEGIN(*this, js)
        MAP_TO_MACRO(TO_JSON_PARAM)
        JSON_PARAM_END()
    }

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&lang_name, "lang", "language name", "LANG");
        ctx.VarBool(&is_header, "header", "generate header file");
        ctx.VarBool(&is_main, "main", "generate main file");
        ctx.VarBool(&is_cmake, "cmake", "generate cmake file");
        ctx.VarBool(&is_config, "config", "generate config file");
        ctx.VarBool(&debug, "debug", "debug mode (print hook call on stderr)");
        ctx.VarBool(&print_hooks, "print-hooks", "print hooks");
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

    std::string env_escape(std::string_view str, std::map<std::string, std::string>& map) {
        return futils::env::expand<std::string>(str, futils::env::expand_map<std::string>(map), true);
    }

    bool include_stack(Flags& flags, size_t& line, std::vector<std::filesystem::path>& stack, auto&& file_name, auto&& per_line) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string();
        if (flags.debug || flags.print_hooks) {
            futils::wrap::cerr_wrap() << "try hook via include: " << name << '\n';
        }
        for (auto& parents : stack) {
            std::error_code ec;
            if (std::filesystem::equivalent(parents, hook_file, ec)) {
                futils::wrap::cerr_wrap() << "circular include: " << name << '\n';
                return false;
            }
        }
        stack.push_back(hook_file);
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "hit hook via include: " << name << '\n';
        for (auto i = 0; i < lines.size(); i++) {
            if (futils::strutil::starts_with(lines[i], "!@include ")) {
                auto split = futils::strutil::split(lines[i], " ", 2);
                if (split.size() == 2) {
                    include_stack(flags, line, stack, split[1], per_line);
                }
                else {
                    futils::wrap::cerr_wrap() << "invalid include: " << lines[i] << '\n';
                }
                continue;
            }
            per_line(line, lines[i]);
            line++;
        }
        stack.pop_back();
        return true;
    }

    bool may_write_from_hook(Flags& flags, auto&& file_name, auto&& per_line) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string() + u8".txt";
        if (flags.debug) {
            futils::wrap::cerr_wrap() << "try hook: " << name << '\n';
        }
        else if (flags.print_hooks) {
            futils::wrap::cerr_wrap() << "hook: " << name << '\n';
        }
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "hit hook: " << name << '\n';
        size_t line = 0;
        for (auto i = 0; i < lines.size(); i++) {
            if (futils::strutil::starts_with(lines[i], "!@include ")) {
                auto split = futils::strutil::split(lines[i], " ", 2);
                if (split.size() == 2) {
                    std::vector<std::filesystem::path> stack;
                    stack.reserve(10);
                    stack.push_back(hook_file);
                    include_stack(flags, line, stack, split[1], per_line);
                }
                else {
                    futils::wrap::cerr_wrap() << "invalid include: " << lines[i] << '\n';
                }
                continue;
            }
            per_line(line, lines[i]);
            line++;
        }
        return true;
    }

    bool may_write_from_hook(Flags& flags, bm2::HookFile hook, auto&& per_line) {
        return may_write_from_hook(flags, to_string(hook), per_line);
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bm2::HookFileSub sub) {
        auto concat = std::format("{}{}", to_string(hook), to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub = bm2::HookFileSub::main) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub, auto sub_sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}_{}", to_string(hook), op_name, to_string(sub), sub_sub);
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, bm2::HookFileSub sub = bm2::HookFileSub::main) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line) {
            w.writeln(line);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code) {
        return may_write_from_hook(flags, hook, [&](size_t i, futils::view::rvec& line) {
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

    void write_inner_function(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags) {
        inner_function.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = inner_function.indent_scope();
        auto func_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_op, op, stage)) {
                inner();
            }
        };
        func_hook([&] {}, bm2::HookFileSub::before);
        if (op == AbstractOp::APPEND) {
            inner_function.writeln("auto vector_ref = code.left_ref().value();");
            inner_function.writeln("auto new_element_ref = code.right_ref().value();");
            inner_function.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
            inner_function.writeln("auto new_element_eval = eval(ctx.ref(new_element_ref), ctx);");
            func_hook([&] {
                if (flags.surrounded_append_method) {
                    inner_function.writeln("w.writeln(vector_eval.result ,\" = \", vector_eval.result, \".", flags.append_method, "(\", new_element_eval.result, \")", flags.end_of_statement, "\");");
                }
                else {
                    inner_function.writeln("w.writeln(vector_eval.result, \".", flags.append_method, "(\", new_element_eval.result, \")", flags.end_of_statement, "\");");
                }
            });
        }
        else if (op == AbstractOp::ASSIGN) {
            inner_function.writeln("auto left_ref = code.left_ref().value();");
            inner_function.writeln("auto right_ref = code.right_ref().value();");
            inner_function.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
            inner_function.writeln("auto right_eval = eval(ctx.ref(right_ref), ctx);");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"\", left_eval.result, \" = \", right_eval.result, \"", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::BACKWARD_INPUT || op == AbstractOp::BACKWARD_OUTPUT) {
            inner_function.writeln("auto ref = code.ref().value();");
            inner_function.writeln("auto evaluated = eval(ctx.ref(ref),ctx);");
            func_hook([&] {
                if (op == AbstractOp::BACKWARD_INPUT) {
                    std::map<std::string, std::string> map{
                        {"DECODER", "\",ctx.r(),\""},
                        {"OFFSET", "\",evaluated.result,\""},
                    };
                    auto escaped = env_escape(flags.decode_backward, map);
                    inner_function.writeln("w.writeln(\"", escaped, flags.end_of_statement, "\");");
                }
                else {
                    std::map<std::string, std::string> map{
                        {"ENCODER", "\",ctx.w(),\""},
                        {"OFFSET", "\",evaluated.result,\""},
                    };
                    auto escaped = env_escape(flags.encode_backward, map);
                    inner_function.writeln("w.writeln(\"", escaped, flags.end_of_statement, "\");");
                }
            });
        }
        else if (op == AbstractOp::LENGTH_CHECK) {
            inner_function.writeln("auto vector_ref = code.left_ref().value();");
            inner_function.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
            inner_function.writeln("auto size_ref = code.right_ref().value();");
            inner_function.writeln("auto size_eval = eval(ctx.ref(size_ref), ctx);");
            func_hook([&] {
                if (flags.surrounded_size_method) {
                    inner_function.writeln("w.writeln(\"assert(", flags.size_method, "(\",vector_eval.result,\") == \", size_eval.result, \")", flags.end_of_statement, "\");");
                }
                else {
                    inner_function.writeln("w.writeln(\"assert(\", vector_eval.result, \".", flags.size_method, "() == \", size_eval.result, \")", flags.end_of_statement, "\");");
                }
            });
        }
        else if (op == AbstractOp::DEFINE_VARIABLE || op == AbstractOp::DECLARE_VARIABLE) {
            if (op == AbstractOp::DECLARE_VARIABLE) {
                inner_function.writeln("auto ident = ctx.ident(code.ref().value());");
                inner_function.writeln("auto init_ref = ctx.ref(code.ref().value()).ref().value();");
                inner_function.writeln("auto type_ref = ctx.ref(code.ref().value()).type().value();");
            }
            else {
                inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                inner_function.writeln("auto init_ref = code.ref().value();");
                inner_function.writeln("auto type_ref = code.type().value();");
            }
            inner_function.writeln("auto type = type_to_string(ctx,type_ref);");
            inner_function.writeln("auto init = eval(ctx.ref(init_ref), ctx);");
            func_hook([&] {
                if (flags.omit_type_on_define_var) {
                    inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.define_var_assign, " {}", flags.end_of_statement, "\", ident, init.result));");
                }
                else {
                    if (flags.prior_ident) {
                        inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} ", flags.define_var_assign, " {}", flags.end_of_statement, "\", ident, type, init.result));");
                    }
                    else {
                        inner_function.writeln("w.writeln(std::format(\"", flags.define_var_keyword, "{} ", flags.var_type_separator, "{} ", flags.define_var_assign, " {}", flags.end_of_statement, "\",type, ident, init.result));");
                    }
                }
            });
        }
        else if (op == AbstractOp::BEGIN_ENCODE_PACKED_OPERATION || op == AbstractOp::BEGIN_DECODE_PACKED_OPERATION ||
                 op == AbstractOp::END_ENCODE_PACKED_OPERATION || op == AbstractOp::END_DECODE_PACKED_OPERATION ||
                 op == AbstractOp::DYNAMIC_ENDIAN) {
            inner_function.writeln("auto fallback = ctx.bm.code[i].fallback().value();");
            func_hook([&] {
                inner_function.writeln("if(fallback.value() != 0) {");
                auto scope = inner_function.indent_scope();
                inner_function.writeln("auto inner_range = ctx.range(fallback);");
                func_hook([&] {
                    inner_function.writeln("inner_function(ctx, w, inner_range);");
                },
                          bm2::HookFileSub::fallback);
                scope.execute();
                inner_function.writeln("}");
                inner_function.writeln("else {");
                auto scope2 = inner_function.indent_scope();
                func_hook([&] {
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                },
                          bm2::HookFileSub::no_fallback);
                scope2.execute();
                inner_function.writeln("}");
            });
        }
        else if (op == AbstractOp::ASSERT) {
            inner_function.writeln("auto evaluated = eval(ctx.ref(code.ref().value()), ctx);");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"assert(\", evaluated.result, \")", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::EXPLICIT_ERROR) {
            inner_function.writeln("auto param = code.param().value();");
            inner_function.writeln("auto evaluated = eval(ctx.ref(param.refs[0]), ctx);");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"throw std::runtime_error(\", evaluated.result, \")", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::IF || op == AbstractOp::ELIF ||
                 op == AbstractOp::ELSE || op == AbstractOp::LOOP_INFINITE ||
                 op == AbstractOp::LOOP_CONDITION || op == AbstractOp::DEFINE_FUNCTION ||
                 op == AbstractOp::MATCH || op == AbstractOp::EXHAUSTIVE_MATCH ||
                 op == AbstractOp::CASE || op == AbstractOp::DEFAULT_CASE) {
            if (op == AbstractOp::IF || op == AbstractOp::ELIF || op == AbstractOp::LOOP_CONDITION ||
                op == AbstractOp::MATCH || op == AbstractOp::EXHAUSTIVE_MATCH ||
                op == AbstractOp::CASE) {
                inner_function.writeln("auto ref = code.ref().value();");
                inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
            }
            if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                inner_function.writeln("defer.pop_back();");
            }

            if (op == AbstractOp::DEFINE_FUNCTION) {
                inner_function.writeln("auto ident = ctx.ident(code.ident().value());");
                // inner_function.writeln("auto range = ctx.range(code.ident().value());");
                inner_function.writeln("auto found_type_pos = find_op(ctx,range,rebgn::AbstractOp::RETURN_TYPE);");
                inner_function.writeln("std::optional<std::string> type;");
                inner_function.writeln("if(found_type_pos) {");
                {
                    auto inner_scope = inner_function.indent_scope();
                    inner_function.writeln("auto type_ref = ctx.bm.code[*found_type_pos].type().value();");
                    inner_function.writeln("type = type_to_string(ctx,type_ref);");
                }
                inner_function.writeln("}");
                inner_function.writeln("std::optional<std::string> belong_name;");
                inner_function.writeln("if(auto belong = code.belong();belong&&belong->value()!=0) {");
                {
                    auto inner_scope = inner_function.indent_scope();
                    inner_function.writeln("belong_name = ctx.ident(belong.value());");
                }
                inner_function.writeln("}");
                func_hook([&] {
                    inner_function.writeln("w.write(\"", flags.func_keyword, " \");");
                    if (!flags.trailing_return_type) {
                        inner_function.writeln("if(type) {");
                        inner_function.indent_writeln("w.write(*type);");
                        inner_function.writeln("}");
                        inner_function.writeln("else {");
                        inner_function.indent_writeln("w.write(\"", flags.func_void_return_type, "\");");
                        inner_function.writeln("}");
                    }
                    inner_function.writeln("w.write(\" \", ident, \"", flags.func_brace_ident_separator, "(\");");
                    inner_function.writeln("add_parameter(ctx, w, range);");
                    inner_function.writeln("w.write(\") \");");
                    if (flags.trailing_return_type) {
                        inner_function.writeln("if(type) {");
                        inner_function.indent_writeln("w.write(\"", flags.func_type_separator, "\", *type);");
                        inner_function.writeln("}");
                        inner_function.writeln("else {");
                        inner_function.indent_writeln("w.write(\"", flags.func_void_return_type, "\");");
                        inner_function.writeln("}");
                    }
                    inner_function.writeln("w.writeln(\"", flags.block_begin, "\");");
                });
            }
            else {
                func_hook([&] {
                    if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                        if (flags.otbs_on_block_end) {
                            inner_function.writeln("w.write(\"", flags.block_end, "\");");
                        }
                        else {
                            inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
                        }
                    }
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
                        case AbstractOp::MATCH:
                        case AbstractOp::EXHAUSTIVE_MATCH:
                            inner_function.write(flags.match_keyword, " ", condition, " ", flags.block_begin);
                            break;
                        case AbstractOp::CASE:
                            inner_function.write(flags.match_case_keyword, " ", condition, " ", flags.match_case_separator, flags.block_begin);
                            break;
                        case AbstractOp::DEFAULT_CASE:
                            inner_function.write(flags.match_default_keyword, " ", flags.block_begin);
                            break;
                    }
                    inner_function.writeln("\");");
                });
            }
            inner_function.writeln("defer.push_back(w.indent_scope_ex());");
        }
        else if (op == AbstractOp::END_IF || op == AbstractOp::END_LOOP ||
                 op == AbstractOp::END_FUNCTION || op == AbstractOp::END_MATCH ||
                 op == AbstractOp::END_CASE) {
            inner_function.writeln("defer.pop_back();");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
            });
        }
        else if (op == AbstractOp::CONTINUE) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"continue", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::BREAK) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"break", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::RET) {
            inner_function.writeln("auto ref = code.ref().value();");
            inner_function.writeln("if(ref.value() != 0) {");
            auto scope = inner_function.indent_scope();
            inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return \", evaluated.result, \"", flags.end_of_statement, "\");");
            },
                      bm2::HookFileSub::value);
            scope.execute();
            inner_function.writeln("}");
            inner_function.writeln("else {");
            auto else_scope = inner_function.indent_scope();
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return", flags.end_of_statement, "\");");
            },
                      bm2::HookFileSub::empty);
            else_scope.execute();
            inner_function.writeln("}");
        }
        else if (op == AbstractOp::RET_SUCCESS || op == AbstractOp::RET_PROPERTY_SETTER_OK) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return ", flags.true_literal, flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::RET_PROPERTY_SETTER_FAIL) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return ", flags.false_literal, flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::INC) {
            inner_function.writeln("auto ref = code.ref().value();");
            inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
            func_hook([&] {
                inner_function.writeln("w.writeln(evaluated.result, \"+= 1", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::CHECK_UNION || op == AbstractOp::SWITCH_UNION) {
            inner_function.writeln("auto union_member_ref = code.ref().value();");
            inner_function.writeln("auto union_ref = ctx.ref(union_member_ref).belong().value();");
            inner_function.writeln("auto union_field_ref = ctx.ref(union_ref).belong().value();");
            inner_function.writeln("auto union_member_index = ctx.ref(union_member_ref).int_value()->value();");
            inner_function.writeln("auto union_member_ident = ctx.ident(union_member_ref);");
            inner_function.writeln("auto union_ident = ctx.ident(union_ref);");
            inner_function.writeln("auto union_field_ident = eval(ctx.ref(union_field_ref),ctx);");
            if (op == AbstractOp::CHECK_UNION) {
                inner_function.writeln("auto check_type = code.check_at().value();");
            }
            func_hook([&] {
                std::map<std::string, std::string> map{
                    {"MEMBER_IDENT", "\",union_member_ident,\""},
                    {"MEMBER_FULL_IDENT", "\",type_accessor(ctx.ref(union_member_ref),ctx),\""},
                    {"UNION_IDENT", "\",union_ident,\""},
                    {"UNION_FULL_IDENT", "\",type_accessor(ctx.ref(union_ref),ctx),\""},
                    {"FIELD_IDENT", "\",union_field_ident.result,\""},
                    {"MEMBER_INDEX", "\",futils::number::to_string<std::string>(union_member_index),\""},
                };
                auto escaped = env_escape(flags.check_union_condition, map);
                inner_function.writeln("w.writeln(\"", flags.if_keyword, flags.condition_has_parentheses ? "(" : " ",
                                       escaped, flags.condition_has_parentheses ? ") " : " ", flags.block_begin, "\");");
                inner_function.writeln("auto scope = w.indent_scope_ex();");
                if (op == AbstractOp::CHECK_UNION) {
                    inner_function.writeln("if(check_type == rebgn::UnionCheckAt::ENCODER) {");
                    auto encoder_scope = inner_function.indent_scope();
                    auto ret = env_escape(flags.check_union_fail_return_value, map);
                    inner_function.writeln("w.writeln(\"return ", ret, flags.end_of_statement, "\");");
                    encoder_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_PTR) {");
                    auto property_getter_ptr_scope = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"return ", flags.empty_pointer, flags.end_of_statement, "\");");
                    property_getter_ptr_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_OPTIONAL) {");
                    auto property_getter_optional_scope = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"return ", flags.empty_optional, flags.end_of_statement, "\");");
                    property_getter_optional_scope.execute();
                    inner_function.writeln("}");
                }
                else {
                    auto switch_union = env_escape(flags.switch_union, map);
                    inner_function.writeln("w.writeln(\"", switch_union, flags.end_of_statement, "\");");
                }
                inner_function.writeln("scope.execute();");
                inner_function.writeln("w.writeln(\"", flags.block_end, "\");");
            });
        }
        else if (op == AbstractOp::CALL_ENCODE || op == AbstractOp::CALL_DECODE) {
            inner_function.writeln("auto func_ref = code.left_ref().value();");
            inner_function.writeln("auto func_belong = ctx.ref(func_ref).belong().value();");
            inner_function.writeln("auto func_belong_name = type_accessor(ctx.ref(func_belong), ctx);");
            inner_function.writeln("auto func_name = ctx.ident(func_ref);");
            inner_function.writeln("auto obj_ref = code.right_ref().value();");
            inner_function.writeln("auto obj_eval = eval(ctx.ref(obj_ref), ctx);");
            func_hook([&] {
                inner_function.writeln("w.write(obj_eval.result, \".\", func_name, \"(\");");
                inner_function.writeln("add_call_parameter(ctx, w,range);");
                inner_function.writeln("w.writeln(\")", flags.end_of_statement, "\");");
            });
        }
        else if (op == AbstractOp::ENCODE_INT || op == AbstractOp::DECODE_INT ||
                 op == AbstractOp::ENCODE_INT_VECTOR || op == AbstractOp::DECODE_INT_VECTOR ||
                 op == AbstractOp::ENCODE_INT_VECTOR_FIXED || op == AbstractOp::DECODE_INT_VECTOR_FIXED ||
                 op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF || op == AbstractOp::PEEK_INT_VECTOR) {
            inner_function.writeln("auto fallback = code.fallback().value();");
            func_hook([&] {
                inner_function.writeln("if(fallback.value() != 0) {");
                auto indent = inner_function.indent_scope();
                inner_function.writeln("auto inner_range = ctx.range(fallback);");
                func_hook([&] {
                    inner_function.writeln("inner_function(ctx, w, inner_range);");
                },
                          bm2::HookFileSub::fallback);
                indent.execute();
                inner_function.writeln("}");
                inner_function.writeln("else {");
                auto indent2 = inner_function.indent_scope();
                inner_function.writeln("auto bit_size = code.bit_size()->value();");
                inner_function.writeln("auto endian = code.endian().value();");
                if (op == AbstractOp::ENCODE_INT || op == AbstractOp::DECODE_INT ||
                    op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                    inner_function.writeln("auto ref = code.ref().value();");
                    inner_function.writeln("auto evaluated = eval(ctx.ref(ref), ctx);");
                }
                else {
                    inner_function.writeln("auto vector_ref = code.left_ref().value();");
                    inner_function.writeln("auto vector_value = eval(ctx.ref(vector_ref), ctx);");
                    inner_function.writeln("auto size_ref = code.right_ref().value();");
                    inner_function.writeln("auto size_value = eval(ctx.ref(size_ref), ctx);");
                }
                func_hook([&] {
                    inner_function.writeln("if(bit_size == 8) {");
                    auto indent3 = inner_function.indent_scope();
                    if (op == AbstractOp::ENCODE_INT_VECTOR || op == AbstractOp::ENCODE_INT_VECTOR_FIXED) {
                        std::map<std::string, std::string> map{
                            {"ENCODER", "\",ctx.w(),\""},
                            {"LEN", "\",size_value.result,\""},
                            {"VALUE", "\",vector_value.result,\""},
                        };
                        auto escaped = env_escape(flags.encode_bytes_op, map);
                        inner_function.writeln("w.writeln(\"", escaped, "\");");
                    }
                    else if (op == AbstractOp::DECODE_INT_VECTOR || op == AbstractOp::DECODE_INT_VECTOR_FIXED) {
                        std::map<std::string, std::string> map{
                            {"DECODER", "\",ctx.r(),\""},
                            {"LEN", "\",size_value.result,\""},
                            {"VALUE", "\",vector_value.result,\""},
                        };
                        auto escaped = env_escape(flags.decode_bytes_op, map);
                        inner_function.writeln("w.writeln(\"", escaped, "\");");
                    }
                    else if (op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                        std::map<std::string, std::string> map{
                            {"DECODER", "\",ctx.r(),\""},
                            {"VALUE", "\",evaluated.result,\""},
                        };
                        auto escaped = env_escape(flags.decode_bytes_until_eof_op, map);
                        inner_function.writeln("w.writeln(\"", escaped, "\");");
                    }
                    else if (op == AbstractOp::PEEK_INT_VECTOR) {
                        std::map<std::string, std::string> map{
                            {"DECODER", "\",ctx.r(),\""},
                            {"LEN", "\",size_value.result,\""},
                            {"VALUE", "\",vector_value.result,\""},
                        };
                        auto escaped = env_escape(flags.peek_bytes_op, map);
                        inner_function.writeln("w.writeln(\"", escaped, "\");");
                    }
                    else {
                        inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    }
                    indent3.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else {");
                    auto indent4 = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    indent4.execute();
                    inner_function.writeln("}");
                },
                          bm2::HookFileSub::no_fallback);
                indent2.execute();
                inner_function.writeln("}");
            });
        }
        else {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
            });
        }
        func_hook([&] {}, bm2::HookFileSub::after);
        inner_function.writeln("break;");
        scope.execute();
        inner_function.writeln("}");
    }

    enum class EvalResultMode {
        TEXT,
        PASSTHROUGH,
        COMMENT,  // treat as TEXT but special handling for comment
    };

    void do_make_eval_result(bm2::TmpCodeWriter& w, AbstractOp op, Flags& flags, const std::string& text, EvalResultMode mode) {
        std::map<std::string, std::string> map{
            {"RESULT", "result"},
            {"TEXT", text},
        };
        if (mode == EvalResultMode::PASSTHROUGH) {
            auto escaped = env_escape(flags.eval_result_passthrough, map);
            w.writeln(escaped);
        }
        else {
            auto escaped = env_escape(flags.eval_result_text, map);
            w.writeln(escaped);
        }
    }

    void write_field_accessor(bm2::TmpCodeWriter& field_accessor, bm2::TmpCodeWriter& type_accessor, AbstractOp op, Flags& flags) {
        auto field_accessor_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(field_accessor, flags, bm2::HookFile::field_accessor_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(field_accessor, flags, bm2::HookFile::field_accessor_op, op, stage)) {
                inner();
            }
        };

        auto add_start = [&](auto&& inner) {
            field_accessor.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = field_accessor.indent_scope();
            field_accessor_hook([&] {}, bm2::HookFileSub::before);
            inner();
            field_accessor_hook([&] {}, bm2::HookFileSub::after);
            field_accessor.writeln("break;");
            scope.execute();
            field_accessor.writeln("}");
        };

        auto type_accessor_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(type_accessor, flags, bm2::HookFile::type_accessor_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(type_accessor, flags, bm2::HookFile::type_accessor_op, op, stage)) {
                inner();
            }
        };
        auto add_type_start = [&](auto&& inner) {
            type_accessor.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = field_accessor.indent_scope();
            type_accessor_hook([&] {}, bm2::HookFileSub::before);
            inner();
            type_accessor_hook([&] {}, bm2::HookFileSub::after);
            type_accessor.writeln("break;");
            scope.execute();
            type_accessor.writeln("}");
        };
        if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE) {
            add_start([&] {
                field_accessor_hook([&] {
                    do_make_eval_result(field_accessor, op, flags, "ctx.this_()", EvalResultMode::TEXT);
                });
            });
            add_type_start([&] {
                type_accessor.writeln("auto ident = ctx.ident(code.ident().value());");
                type_accessor_hook([&] {
                    type_accessor.writeln("result = ident;");
                });
            });
        }
        else if (op == AbstractOp::DEFINE_UNION || op == AbstractOp::DEFINE_UNION_MEMBER ||
                 op == AbstractOp::DEFINE_FIELD || op == AbstractOp::DEFINE_BIT_FIELD ||
                 op == AbstractOp::DEFINE_PROPERTY) {
            add_start([&] {
                field_accessor.writeln("auto ident = ctx.ident(code.ident().value());");
                field_accessor.writeln("auto belong = code.belong().value();");
                field_accessor.writeln("auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM;");
                if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                    field_accessor.writeln("auto union_member_ref = code.ident().value();");
                    field_accessor.writeln("auto union_ref = belong;");
                    field_accessor.writeln("auto union_field_ref = ctx.ref(union_ref).belong().value();");
                    field_accessor.writeln("auto union_field_belong = ctx.ref(union_field_ref).belong().value();");
                }
                field_accessor_hook([&] {
                    if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                        do_make_eval_result(field_accessor, op, flags, "field_accessor(ctx.ref(union_field_ref),ctx)", EvalResultMode::PASSTHROUGH);
                    }
                    else if (op == AbstractOp::DEFINE_BIT_FIELD) {
                        do_make_eval_result(field_accessor, op, flags, "field_accessor(ctx.ref(belong),ctx)", EvalResultMode::PASSTHROUGH);
                    }
                    else {
                        field_accessor.writeln("if(is_member) {");
                        auto scope = field_accessor.indent_scope();
                        field_accessor.writeln("auto belong_eval = field_accessor(ctx.ref(belong), ctx);");
                        field_accessor_hook([&] {
                            do_make_eval_result(field_accessor, op, flags, "std::format(\"{}.{}\", belong_eval.result, ident)", EvalResultMode::TEXT);
                        },
                                            bm2::HookFileSub::field);
                        scope.execute();
                        field_accessor.writeln("}");
                        field_accessor.writeln("else {");
                        auto scope2 = field_accessor.indent_scope();
                        field_accessor_hook([&] {
                            do_make_eval_result(field_accessor, op, flags, "ident", EvalResultMode::TEXT);
                        },
                                            bm2::HookFileSub::self);
                        scope2.execute();
                        field_accessor.writeln("}");
                    }
                });
            });
            add_type_start([&] {
                type_accessor.writeln("auto ident = ctx.ident(code.ident().value());");
                type_accessor.writeln("auto belong = code.belong().value();");
                type_accessor.writeln("auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM;");
                if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                    type_accessor.writeln("auto union_member_ref = code.ident().value();");
                    type_accessor.writeln("auto union_ref = belong;");
                    type_accessor.writeln("auto union_field_ref = ctx.ref(union_ref).belong().value();");
                    type_accessor.writeln("auto union_field_belong = ctx.ref(union_field_ref).belong().value();");
                }
                type_accessor_hook([&] {
                    type_accessor.writeln("if(is_member) {");
                    auto scope = type_accessor.indent_scope();
                    type_accessor.writeln("auto belong_eval = type_accessor(ctx.ref(belong), ctx);");
                    type_accessor_hook([&] {
                        type_accessor.writeln("result = std::format(\"{}.{}\", belong_eval, ident);");
                    },
                                       bm2::HookFileSub::field);
                    scope.execute();
                    type_accessor.writeln("}");
                    type_accessor.writeln("else {");
                    auto scope2 = type_accessor.indent_scope();
                    type_accessor_hook([&] {
                        type_accessor.writeln("result = ident;");
                    },
                                       bm2::HookFileSub::self);
                    scope2.execute();
                    type_accessor.writeln("}");
                });
            });
        }
    }

    void write_inner_block(bm2::TmpCodeWriter& inner_block,
                           AbstractOp op, Flags& flags) {
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
        if (op == AbstractOp::DECLARE_FORMAT || op == AbstractOp::DECLARE_ENUM ||
            op == AbstractOp::DECLARE_STATE || op == AbstractOp::DECLARE_PROPERTY ||
            op == AbstractOp::DECLARE_FUNCTION) {
            inner_block.writeln("auto ref = code.ref().value();");
            inner_block.writeln("auto inner_range = ctx.range(ref);");
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
            inner_block.writeln("auto ref=code.ref().value();");
            inner_block.writeln("auto ident = ctx.ident(ref);");
            inner_block.writeln("auto inner_range = ctx.range(ref);");
            inner_block.writeln("auto type_ref = ctx.ref(ref).type().value();");
            inner_block.writeln("auto type = type_to_string(ctx, type_ref);");
            block_hook([&] {
                inner_block.writeln("inner_block(ctx,w,inner_range);");
            });
        }
        else if (op == AbstractOp::DECLARE_UNION || op == AbstractOp::DECLARE_UNION_MEMBER) {
            inner_block.writeln("auto ref = code.ref().value();");
            inner_block.writeln("auto inner_range = ctx.range(ref);");
            block_hook([&] {
                inner_block.writeln("TmpCodeWriter inner_w;");
                inner_block.writeln("inner_block(ctx, inner_w, inner_range);");
                inner_block.writeln("ctx.cw.write_unformatted(inner_w.out());");
            });
        }
        else if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE || op == AbstractOp::DEFINE_UNION_MEMBER) {
            inner_block.writeln("auto ident = ctx.ident(code.ident().value());");
            block_hook([&] {
                inner_block.writeln("w.writeln(\"", flags.struct_keyword, " \", ident, \" ", flags.block_begin, "\");");
                inner_block.writeln("defer.push_back(w.indent_scope_ex());");
            });
        }
        else if (op == AbstractOp::DEFINE_UNION) {
            inner_block.writeln("auto ident = ctx.ident(code.ident().value());");
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
            inner_block.writeln("auto ident = ctx.ident(code.ident().value());");
            inner_block.writeln("auto base_type_ref = code.type().value();");
            inner_block.writeln("std::optional<std::string> base_type;");
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
            inner_block.writeln("auto ident = ctx.ident(code.ident().value());");
            inner_block.writeln("auto evaluated = eval(ctx.ref(code.left_ref().value()), ctx);");
            inner_block.writeln("auto belong = code.belong().value();");
            inner_block.writeln("auto enum_ident = ctx.ident(belong);");
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
            inner_block.writeln("auto type = type_to_string(ctx, code.type().value());");
            inner_block.writeln("auto ident = ctx.ident(code.ident().value());");
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
            inner_block.writeln("auto func = code.right_ref().value();");
            inner_block.writeln("auto inner_range = ctx.range(func);");
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

    void write_eval(bm2::TmpCodeWriter& eval, AbstractOp op, Flags& flags) {
        eval.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = eval.indent_scope();
        auto eval_hook = [&](auto&& default_action, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(eval, flags, bm2::HookFile::eval_op, op, stage)) {
                default_action();
            }
        };
        eval_hook([&] {}, bm2::HookFileSub::before);
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
                eval_hook([&] {}, bm2::HookFileSub::op);
                for (size_t b = 0; to_string(BinaryOp(b))[0] != '\0'; b++) {
                    bm2::TmpCodeWriter inner_eval;
                    if (may_write_from_hook(inner_eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::op, BinaryOp(b))) {
                        eval.writeln("if(op == rebgn::BinaryOp::" + std::string(to_string(BinaryOp(b))) + ") {");
                        auto scope = eval.indent_scope();
                        eval.writeln(inner_eval.out());
                        scope.execute();
                        eval.writeln("}");
                    }
                }
                do_make_eval_result(eval, op, flags, "std::format(\"({} {} {})\", left_eval.result, opstr, right_eval.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::UNARY) {
            eval.writeln("auto op = code.uop().value();");
            eval.writeln("auto ref = code.ref().value();");
            eval.writeln("auto target = eval(ctx.ref(ref), ctx);");
            eval.writeln("auto opstr = to_string(op);");
            eval_hook([&] {
                eval_hook([&] {}, bm2::HookFileSub::op);
                for (size_t b = 0; to_string(UnaryOp(b))[0] != '\0'; b++) {
                    bm2::TmpCodeWriter inner_eval;
                    if (may_write_from_hook(inner_eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::op, UnaryOp(b))) {
                        eval.writeln("if(op == rebgn::UnaryOp::" + std::string(to_string(UnaryOp(b))) + ") {");
                        auto scope = eval.indent_scope();
                        eval.writeln(inner_eval.out());
                        scope.execute();
                        eval.writeln("}");
                    }
                }
                do_make_eval_result(eval, op, flags, "std::format(\"({}{})\", opstr, target.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IS_LITTLE_ENDIAN) {
            eval.writeln("auto fallback = code.fallback().value();");
            eval_hook([&] {
                eval.writeln("if(fallback.value() != 0) {");
                auto scope = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, "eval(ctx.ref(fallback), ctx)", EvalResultMode::PASSTHROUGH);
                },
                          bm2::HookFileSub::fallback);
                scope.execute();
                eval.writeln("}");
                eval.writeln("else {");
                auto scope2 = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, "\"" + flags.is_little_endian + "\"", EvalResultMode::TEXT);
                },
                          bm2::HookFileSub::no_fallback);
                scope2.execute();
                eval.writeln("}");
            });
        }
        else if (op == AbstractOp::ADDRESS_OF) {
            eval.writeln("auto ref = code.ref().value();");
            eval.writeln("auto target = eval(ctx.ref(ref), ctx);");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"" + flags.address_of_placeholder + "\", target.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::OPTIONAL_OF) {
            eval.writeln("auto ref = code.ref().value();");
            eval.writeln("auto target = eval(ctx.ref(ref), ctx);");
            eval.writeln("auto type = type_to_string(ctx, code.type().value());");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"" + flags.optional_of_placeholder + "\", target.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::INPUT_BYTE_OFFSET) {
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"DECODER", "\",ctx.r(),\""},
                };
                auto escaped = env_escape(flags.decode_offset, map);
                do_make_eval_result(eval, op, flags, "futils::strutil::concat<std::string>(\"" + escaped + "\")", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::OUTPUT_BYTE_OFFSET) {
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"ENCODER", "\",ctx.w(),\""},
                };
                auto escaped = env_escape(flags.encode_offset, map);
                do_make_eval_result(eval, op, flags, "futils::strutil::concat<std::string>(\"" + escaped + "\")", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_INT) {
            eval.writeln("auto value = code.int_value()->value();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", value)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_STRING) {
            eval.writeln("auto str = ctx.string_table[code.ident().value().value()];");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"\\\"{}\\\"\", futils::escape::escape_str<std::string>(str,futils::escape::EscapeFlag::hex,futils::escape::no_escape_set(),futils::escape::escape_all()))", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_TRUE) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.true_literal + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_FALSE) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.false_literal + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_INT64) {
            eval.writeln("auto value = *code.int_value64();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", value)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_TYPE) {
            eval.writeln("auto type = code.type().value();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "type_to_string(ctx, type)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_CHAR) {
            eval.writeln("auto char_code = code.int_value()->value();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", char_code)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::EMPTY_PTR) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.empty_pointer + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::EMPTY_OPTIONAL) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.empty_optional + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::PHI || op == AbstractOp::DECLARE_VARIABLE ||
                 op == AbstractOp::DEFINE_VARIABLE_REF ||
                 op == AbstractOp::BEGIN_COND_BLOCK) {
            eval.writeln("auto ref=code.ref().value();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(ref), ctx)", EvalResultMode::PASSTHROUGH);
            });
        }
        else if (op == AbstractOp::ASSIGN) {
            eval.writeln("auto left_ref = code.left_ref().value();");
            eval.writeln("auto ref = code.ref().value();");
            eval.writeln("auto right_ref = code.right_ref().value();");
            eval_hook([&] {
                eval.writeln("if(ref.value() != 0) {");
                auto scope = eval.indent_scope();
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(ref), ctx)", EvalResultMode::PASSTHROUGH);
                scope.execute();
                eval.writeln("}");
                eval.writeln("else {");
                auto scope2 = eval.indent_scope();
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(left_ref), ctx)", EvalResultMode::PASSTHROUGH);
                scope2.execute();
                eval.writeln("}");
            });
        }
        else if (op == AbstractOp::ACCESS) {
            eval.writeln("auto left_ref = code.left_ref().value();");
            eval.writeln("auto right_ref = code.right_ref().value();");
            eval.writeln("auto left_eval = eval(ctx.ref(left_ref), ctx);");
            // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
            eval.writeln("auto right_ident = ctx.ident(right_ref);");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}.{}\", left_eval.result, right_ident)", EvalResultMode::TEXT);
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
                do_make_eval_result(eval, op, flags, "std::format(\"{}[{}]\", left_eval.result, right_eval.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::ARRAY_SIZE) {
            eval.writeln("auto vector_ref = code.ref().value();");
            eval.writeln("auto vector_eval = eval(ctx.ref(vector_ref), ctx);");
            // eval.writeln("result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);");
            eval_hook([&] {
                if (flags.surrounded_size_method) {
                    do_make_eval_result(eval, op, flags, "std::format(\"" + flags.size_method + "({})\", vector_eval.result)", EvalResultMode::TEXT);
                }
                else {
                    do_make_eval_result(eval, op, flags, "std::format(\"{}({})\", vector_eval.result, \"" + flags.size_method + "\")", EvalResultMode::TEXT);
                }
            });
        }
        else if (op == AbstractOp::DEFINE_FIELD || op == AbstractOp::DEFINE_PROPERTY) {
            do_make_eval_result(eval, op, flags, "field_accessor(code,ctx)", EvalResultMode::PASSTHROUGH);
        }
        else if (op == AbstractOp::DEFINE_VARIABLE ||
                 op == AbstractOp::DEFINE_PARAMETER ||
                 op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
            eval.writeln("auto ident = ctx.ident(code.ident().value());");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "ident", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::NEW_OBJECT) {
            eval.writeln("auto type_ref = code.type().value();");
            eval.writeln("auto type = type_to_string(ctx, type_ref);");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}()\", type)", EvalResultMode::TEXT);
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
                    do_make_eval_result(eval, op, flags, "std::format(\"{}({})\", type_str, evaluated.result)", EvalResultMode::TEXT);
                }
                else {
                    do_make_eval_result(eval, op, flags, "std::format(\"({}){}\", type_str, evaluated.result)", EvalResultMode::TEXT);
                }
            });
        }
        else if (op == AbstractOp::FIELD_AVAILABLE) {
            eval.writeln("auto left_ref = code.left_ref().value();");
            eval.writeln("if(left_ref.value() == 0) {");
            auto scope_1 = eval.indent_scope();
            eval.writeln("auto right_ref = code.right_ref().value();");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(right_ref), ctx)", EvalResultMode::PASSTHROUGH);
            },
                      bm2::HookFileSub::self);
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
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(right_ref), ctx)", EvalResultMode::PASSTHROUGH);
            },
                      bm2::HookFileSub::field);
            // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
            eval.writeln("ctx.this_as.pop_back();");
            scope_2.execute();
            eval.writeln("}");
        }
        else {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.wrap_comment("Unimplemented " + std::string(to_string(op))) + "\"", EvalResultMode::COMMENT);
            });
        }
        eval_hook([&] {}, bm2::HookFileSub::after);
        eval.writeln("break;");
        scope.execute();
        eval.writeln("}");
    }

    void write_add_parameter(bm2::TmpCodeWriter& add_parameter,
                             bm2::TmpCodeWriter& add_call_parameter,
                             AbstractOp op, Flags& flags) {
        if (op != AbstractOp::RETURN_TYPE) {
            add_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = add_parameter.indent_scope();
            auto param_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
                if (stage == bm2::HookFileSub::main) {
                    may_write_from_hook(add_parameter, flags, bm2::HookFile::param_op, op, bm2::HookFileSub::pre_main);
                }
                if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_op, op, stage)) {
                    inner();
                }
            };
            param_hook([&] {}, bm2::HookFileSub::before);
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
            param_hook([&] {}, bm2::HookFileSub::after);
            add_parameter.writeln("break;");
            scope.execute();
            add_parameter.writeln("}");

            add_call_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope_call = add_call_parameter.indent_scope();
            auto call_param_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
                if (stage == bm2::HookFileSub::main) {
                    may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_op, op, bm2::HookFileSub::pre_main);
                }
                if (!may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_op, op, stage)) {
                    inner();
                }
            };
            call_param_hook([&] {}, bm2::HookFileSub::before);
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
            else if (op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                add_call_parameter.writeln("auto ref = code.ref().value();");
                add_call_parameter.writeln("auto ident = ctx.ident(ref);");
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(ident);");
                    add_call_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::ENCODER_PARAMETER) {
                call_param_hook([&] {
                    add_call_parameter.writeln(
                        "w.write(ctx.w());");
                    add_call_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::DECODER_PARAMETER) {
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(ctx.r());");
                    add_call_parameter.writeln("params++;");
                });
            }
            else {
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    add_call_parameter.writeln("params++;");
                });
            }
            call_param_hook([&] {}, bm2::HookFileSub::after);
            add_call_parameter.writeln("break;");
            scope_call.execute();
            add_call_parameter.writeln("}");
        }
    }

    void write_type_to_string(bm2::TmpCodeWriter& type_to_string, StorageType type, Flags& flags) {
        auto type_hook = [&](auto&& default_action, bm2::HookFileSub sub = bm2::HookFileSub::main) {
            if (sub == bm2::HookFileSub::main) {
                may_write_from_hook(type_to_string, flags, bm2::HookFile::type_op, type, bm2::HookFileSub::pre_main);
            }
            if (may_write_from_hook(type_to_string, flags, bm2::HookFile::type_op, type, sub)) {
                return;
            }
            default_action();
        };
        type_to_string.writeln(std::format("case rebgn::StorageType::{}: {{", to_string(type)));
        auto scope_type = type_to_string.indent_scope();
        type_hook([&] {}, bm2::HookFileSub::before);
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
                type_to_string.writeln("return std::format(\"", flags.recursive_struct_type_placeholder, "\", ident);");
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
            type_to_string.writeln("auto ref = storage.ref().value();");
            type_to_string.writeln("std::vector<std::string> types;");
            type_to_string.writeln("for (size_t i = index + 1; i < s.storages.size(); i++) {");
            auto scope_variant = type_to_string.indent_scope();
            type_to_string.writeln("types.push_back(type_to_string_impl(ctx, s, bit_size, i));");
            scope_variant.execute();
            type_to_string.writeln("}");
            type_to_string.writeln("auto ident = ctx.ident(ref);");
            type_hook([&] {
                if (flags.variant_mode == "union") {
                    type_to_string.writeln("return ident;");
                }
                else if (flags.variant_mode == "algebraic") {
                    type_to_string.writeln("std::string result;");
                    type_to_string.writeln("for (size_t i = 0; i < types.size(); i++) {");
                    auto scope_variant_algebraic = type_to_string.indent_scope();
                    type_to_string.writeln("if (i != 0) {");
                    auto if_block_variant_algebraic = type_to_string.indent_scope();
                    type_to_string.writeln("result += \" ", flags.algebraic_variant_separator, " \";");
                    if_block_variant_algebraic.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("result += types[i];");
                    scope_variant_algebraic.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("return std::format(\"", flags.algebraic_variant_placeholder, "\", result);");
                }
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
            type_to_string.writeln("bool is_byte_vector = index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8;");
            type_to_string.writeln("auto length = storage.size().value().value();");
            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"TYPE", "\",base_type,\""},
                    {"LENGTH", "\",futils::number::to_string<std::string>(length),\""},
                };
                auto escaped = env_escape(flags.array_type_placeholder, map);
                if (flags.byte_array_type.size()) {
                    type_to_string.writeln("if (is_byte_vector) {");
                    auto if_block_byte_vector = type_to_string.indent_scope();
                    auto escaped2 = env_escape(flags.byte_array_type, map);
                    type_to_string.writeln("return futils::strutil::concat<std::string>(\"", escaped2, "\");");
                    if_block_byte_vector.execute();
                    type_to_string.writeln("}");
                }
                type_to_string.writeln("return futils::strutil::concat<std::string>(\"", escaped, "\");");
            });
        }
        else if (type == StorageType::VECTOR) {
            type_to_string.writeln("bool is_byte_vector = index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8;");
            type_hook([&] {
                if (flags.byte_vector_type.size()) {
                    type_to_string.writeln("if (is_byte_vector) {");
                    auto if_block_byte_vector = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", flags.byte_vector_type, "\";");
                    if_block_byte_vector.execute();
                    type_to_string.writeln("}");
                }
                type_to_string.writeln("return std::format(\"", flags.vector_type_placeholder, "\", base_type);");
            });
        }
        else if (type == StorageType::OPTIONAL) {
            type_hook([&] {
                type_to_string.writeln("return std::format(\"", flags.optional_type_placeholder, "\", base_type);");
            });
        }
        type_hook([&] {}, bm2::HookFileSub::after);
        scope_type.execute();
        type_to_string.writeln("}");
    }

    void write_impl_template(bm2::TmpCodeWriter& w, Flags& flags) {
        auto unimplemented_comment = [&](const std::string& op) {
            return "std::format(\"{}{}{}\",\"" + flags.comment_prefix + "\",to_string(" + op + "),\"" + flags.comment_suffix + "\")";
        };

        bm2::TmpCodeWriter type_to_string;

        w.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0);");
        type_to_string.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size, size_t index) {");
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
            write_type_to_string(type_to_string, type, flags);
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

        w.writeln("std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref);");
        type_to_string.writeln("std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref) {");
        auto scope_type_to_string_ref = type_to_string.indent_scope();
        type_to_string.writeln("auto& storage = ctx.storage_table[ref.ref.value()];");
        type_to_string.writeln("return type_to_string_impl(ctx, storage);");
        scope_type_to_string_ref.execute();
        type_to_string.writeln("}");

        bm2::TmpCodeWriter add_parameter;
        w.writeln("void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
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
        w.write("void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        add_call_parameter.writeln("void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("size_t params = 0;");
        add_call_parameter.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("auto& code = ctx.bm.code[i];");
        add_call_parameter.writeln("switch(code.op) {");
        auto scope_switch_add_call_parameter = add_call_parameter.indent_scope();

        bm2::TmpCodeWriter inner_block;
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

        bm2::TmpCodeWriter inner_function;
        w.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        inner_function.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_function = inner_function.indent_scope();
        inner_function.writeln("std::vector<futils::helper::DynDefer> defer;");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_start, false);
        inner_function.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_function = inner_function.indent_scope();
        inner_function.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_each_code, false);
        inner_function.writeln("switch(code.op) {");

        bm2::TmpCodeWriter eval_result;
        eval_result.writeln("struct EvalResult {");
        {
            auto scope = eval_result.indent_scope();
            eval_result.writeln("std::string result;");
            may_write_from_hook(eval_result, flags, bm2::HookFile::eval_result, false);
        }
        eval_result.writeln("};");

        eval_result.writeln("EvalResult make_eval_result(std::string result) {");
        auto scope_make_eval_result = eval_result.indent_scope();
        eval_result.writeln("return EvalResult{std::move(result)};");
        scope_make_eval_result.execute();
        eval_result.writeln("}");

        w.write_unformatted(eval_result.out());

        bm2::TmpCodeWriter field_accessor;
        w.writeln("EvalResult field_accessor(const rebgn::Code& code, Context& ctx);");
        field_accessor.writeln("EvalResult field_accessor(const rebgn::Code& code, Context& ctx) {");
        auto scope_field_accessor = field_accessor.indent_scope();
        field_accessor.writeln("EvalResult result;");
        field_accessor.writeln("switch(code.op) {");

        bm2::TmpCodeWriter type_accessor;
        w.writeln("std::string type_accessor(const rebgn::Code& code, Context& ctx);");
        type_accessor.writeln("std::string type_accessor(const rebgn::Code& code, Context& ctx) {");
        auto scope_type_accessor = type_accessor.indent_scope();
        type_accessor.writeln("std::string result;");
        type_accessor.writeln("switch(code.op) {");

        bm2::TmpCodeWriter eval;
        w.writeln("EvalResult eval(const rebgn::Code& code, Context& ctx);");
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
                write_inner_block(inner_block, op, flags);
                write_field_accessor(field_accessor, type_accessor, op, flags);
            }

            if (is_expr(op)) {
                write_eval(eval, op, flags);
            }

            if (is_parameter_related(op)) {
                write_add_parameter(add_parameter, add_call_parameter, op, flags);
            }
            if ((!is_expr(op) && !is_struct_define_related(op) && !is_parameter_related(op)) || is_both_expr_and_def(op)) {
                write_inner_function(inner_function, op, flags);
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
        do_make_eval_result(field_accessor, AbstractOp::DEFINE_PROGRAM, flags, unimplemented_comment("code.op"), EvalResultMode::COMMENT);
        field_accessor.writeln("break;");
        if_block_field_accessor.execute();
        field_accessor.writeln("}");  // close default
        field_accessor.writeln("}");  // close switch
        scope_field_accessor.execute();
        field_accessor.writeln("return result;");
        field_accessor.writeln("}");  // close function

        type_accessor.writeln("default: {");
        auto if_block_type_accessor = type_accessor.indent_scope();
        type_accessor.writeln("return ", unimplemented_comment("code.op"), ";");
        if_block_type_accessor.execute();
        type_accessor.writeln("}");  // close default
        type_accessor.writeln("}");  // close switch
        scope_type_accessor.execute();
        type_accessor.writeln("return result;");
        type_accessor.writeln("}");  // close function

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

        w.write_unformatted(type_to_string.out());
        w.write_unformatted(field_accessor.out());
        w.write_unformatted(type_accessor.out());
        w.write_unformatted(eval.out());
        w.write_unformatted(add_parameter.out());
        w.write_unformatted(add_call_parameter.out());
        w.write_unformatted(inner_block.out());
        w.write_unformatted(inner_function.out());
    }

    struct Flag {
        std::string option;
        std::string bind_target;
        std::string type;
        std::string default_value;
        std::string place_holder;
        std::string description;
    };

    std::vector<Flag> get_flags(Flags& flags) {
        std::vector<Flag> parsed_flag;
        may_write_from_hook(flags, bm2::HookFile::flags, [&](size_t i, futils::view::rvec line) {
            auto split = futils::strutil::split<futils::view::rvec>(line, "|", 4);
            if (split.size() < 4) {
                futils::wrap::cerr_wrap() << "invalid flag line: " << line << " at " << i << ":split count is " << split.size() << "\n";
                return;
            }
            auto to_str = [&](auto&& str) { return std::string(str.as_char(), str.size()); };
            Flag flag;
            flag.option = to_str(split[0]);
            flag.bind_target = to_str(split[1]);
            flag.type = to_str(split[2]);
            flag.default_value = to_str(split[3]);
            if (split.size() > 4) {
                if (flag.type == "bool") {
                    flag.description = to_str(split[4]);
                }
                else {
                    auto split2 = futils::strutil::split<futils::view::rvec>(split[4], "|", 1);
                    if (split2.size() > 1) {
                        flag.place_holder = to_str(split2[0]);
                        flag.description = to_str(split2[1]);
                    }
                    else {
                        flag.place_holder = to_str(split2[0]);
                    }
                }
            }
            else if (flag.type != "bool") {
                flag.place_holder = "VALUE";
            }
            parsed_flag.push_back(std::move(flag));
        });
        return parsed_flag;
    }

    void code_main(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("#include \"bm2", flags.lang_name, ".hpp\"");
        w.writeln("#include <bm2/entry.hpp>");
        w.writeln("struct Flags : bm2::Flags {");
        auto scope_flags = w.indent_scope();
        w.writeln("bm2", flags.lang_name, "::Flags bm2", flags.lang_name, "_flags;");
        w.writeln("void bind(futils::cmdline::option::Context& ctx) {");
        auto scope_bind = w.indent_scope();
        auto parsed_flag = get_flags(flags);
        w.writeln("bm2::Flags::bind(ctx);");
        for (auto&& flag : parsed_flag) {
            if (flag.type == "bool") {
                w.writeln("ctx.VarBool(&bm2", flags.lang_name, "_flags.", flag.bind_target, ", \"", flag.option, "\", \"", flag.description, "\");");
            }
            else if (flag.type == "string") {
                w.writeln("ctx.VarString(&bm2", flags.lang_name, "_flags.", flag.bind_target, ", \"", flag.option, "\", \"", flag.description, "\", \"", flag.place_holder, "\");");
            }
        }
        scope_bind.execute();
        w.writeln("}");
        scope_flags.execute();
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
        auto parsed_flag = get_flags(flags);
        w.writeln("struct Flags {");
        auto scope_flags = w.indent_scope();
        for (auto&& flag : parsed_flag) {
            if (flag.type != "bool" && flag.type != "string") {
                continue;
            }
            w.writeln(flag.type, " ", flag.bind_target, " = ", flag.default_value, ";");
        }
        scope_flags.execute();
        w.writeln("};");

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
        w.writeln("target_link_libraries(bm2", flags.lang_name, " futils bm)");
        w.writeln("install(TARGETS bm2", flags.lang_name, " DESTINATION tool)");
        w.writeln("if (\"$ENV{BUILD_MODE}\" STREQUAL \"web\")");
        w.writeln("  install(FILES \"${CMAKE_BINARY_DIR}/tool/bm2", flags.lang_name, ".wasm\" DESTINATION tool)");
        w.writeln("endif()");
    }

    void code_config(bm2::TmpCodeWriter& w, Flags& flags) {
        auto js = futils::json::convert_to_json<futils::json::OrderedJSON>(flags);
        auto out = futils::json::to_string<std::string>(js);
        w.writeln(out);
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

    void code_template(bm2::TmpCodeWriter& w, Flags& flags) {
        if (!may_load_config(flags)) {
            return;
        }
        if (flags.is_config) {
            code_config(w, flags);
            return;
        }
        if (!flags.print_hooks) {
            if (flags.lang_name.empty()) {
                futils::wrap::cerr_wrap() << "--lang option is required\n";
                return;
            }
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
    rebgn::code_template(w, flags);
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
