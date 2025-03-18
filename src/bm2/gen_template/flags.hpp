/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <json/convert_json.h>
#include <json/json_export.h>
#include <format>
#include <map>
#include "hook_list.hpp"

struct Content {
    std::string var_name;
    std::string type;
    std::string description;
    std::string initial_value;
};

struct Flags : futils::cmdline::templ::HelpOption {
    bm2::GenerateMode mode = bm2::GenerateMode::generator;
    std::map<bm2::FuncName, std::map<std::string, std::vector<Content>>> content;
    bm2::FuncName func_name = bm2::FuncName::eval;

    bool requires_lang_option() const {
        return !print_hooks && !(mode == bm2::GenerateMode::docs_json ||
                                 mode == bm2::GenerateMode::docs_markdown);
    }

    void set_func_name(bm2::FuncName func_name) {
        this->func_name = func_name;
    }

    std::string_view config_file = "config.json";
    std::string_view hook_file_dir = "hook";
    bool debug = false;
    bool print_hooks = false;

    const std::string& worker_name() {
        if (worker_request_name.empty()) {
            return lang_name;
        }
        return worker_request_name;
    }

    // json options
    std::string lang_name;
    std::string file_suffix = "";
    std::string worker_request_name = "";
    std::string worker_lsp_name = "";
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

#include "map_macro.hpp"

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
        // ctx.VarBool(&is_header, "header", "generate header file");
        // ctx.VarBool(&is_main, "main", "generate main file");
        // ctx.VarBool(&is_cmake, "cmake", "generate cmake file");
        // ctx.VarBool(&is_config, "config", "generate config file");
        ctx.VarBool(&debug, "debug", "debug mode (print hook call on stderr)");
        ctx.VarBool(&print_hooks, "print-hooks", "print hooks");
        ctx.VarString<true>(&config_file, "config-file", "config file", "FILE");
        ctx.VarString<true>(&hook_file_dir, "hook-dir", "hook file directory", "DIR");
        // ctx.VarMap<std::string, std::string_view, std::map>(&is_template_docs, "template-docs", "template docs (output format: json,markdown)", "FORMAT", std::map<std::string, std::string_view>{{"json", "json"}, {"markdown", "markdown"}, {"md", "markdown"}});
        // ctx.VarMap<std::string, std::string_view, std::map>(&js_glue, "js-glue", "js glue code for playground (output target: ui,worker)", "TARGET", std::map<std::string, std::string_view>{
        //                                                                                                                                                  {"ui", "ui"},
        //                                                                                                                                                  {"worker", "worker"},
        //                                                                                                                                                  {"ui-embed", "ui-embed"},
        //                                                                                                                                              });
        ctx.VarMap<std::string, bm2::GenerateMode, std::map>(
            &mode, "mode", "generate mode (generator,config,header,main,cmake,js-worker,js-ui,js-ui-embed,docs-json,docs-markdown)", "MODE",
            std::map<std::string, bm2::GenerateMode>{
                {"generator", bm2::GenerateMode::generator},
                {"header", bm2::GenerateMode::header},
                {"main", bm2::GenerateMode::main},
                {"cmake", bm2::GenerateMode::cmake},
                {"js-worker", bm2::GenerateMode::js_worker},
                {"js-ui", bm2::GenerateMode::js_ui},
                {"js-ui-embed", bm2::GenerateMode::js_ui_embed},
                {"docs-json", bm2::GenerateMode::docs_json},
                {"docs-markdown", bm2::GenerateMode::docs_markdown},
                {"config", bm2::GenerateMode::config_},
            });
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