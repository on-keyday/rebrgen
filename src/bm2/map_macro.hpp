#define MAP_TO_MACRO(MACRO_NAME)                                               \
    MACRO_NAME(lang_name, "lang")                                              \
    MACRO_NAME(file_suffix, "suffix")                                          \
    MACRO_NAME(worker_request_name, "worker_request_name")                     \
    MACRO_NAME(worker_lsp_name, "worker_lsp_name")                             \
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
