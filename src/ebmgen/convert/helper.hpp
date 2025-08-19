/*license*/
#pragma once

#include "ebm/extended_binary_module.hpp"
#include "../common.hpp"
namespace ebmgen {
#define MAYBE_VOID(out, expr)                                             \
    auto out##____ = expr;                                                \
    if (!out##____) {                                                     \
        return unexpect_error(std::move(out##____.error()), #out, #expr); \
    }

#define MAYBE(out, expr)  \
    MAYBE_VOID(out, expr) \
    auto out = std::move(*out##____)

#define EBM_AST_CONSTRUCTOR(ref_name, RefType, add_func, make_func, ...) \
    ebm::RefType ref_name;                                               \
    {                                                                    \
        MAYBE(new_ref____, add_func(make_func(__VA_ARGS__)));            \
        ref_name = new_ref____;                                          \
    }

#define EBM_AST_STATEMENT(ref_name, make_func, ...) \
    EBM_AST_CONSTRUCTOR(ref_name, StatementRef, ctx.repository().add_statement, make_func, __VA_ARGS__)

#define EBM_AST_EXPRESSION(ref_name, make_func, ...) \
    EBM_AST_CONSTRUCTOR(ref_name, ExpressionRef, ctx.repository().add_expr, make_func, __VA_ARGS__)

#define EBM_AST_EXPRESSION_REF(ref_name) ebm::ExpressionRef ref_name;
#define EBM_AST_STATEMENT_REF(ref_name) ebm::StatementRef ref_name;
#define EBM_AST_VARIABLE_REF(ref_name) EBM_AST_EXPRESSION_REF(ref_name) EBM_AST_STATEMENT_REF(ref_name##_def)
#define EBM_AST_VARIABLE_REF_SET(ref_name, expr_name, def_name) \
    ref_name = expr_name;                                       \
    ref_name##_def = def_name;

    // currently new ConverterContext

    // ebm ast related

#define EBMA_CONVERT_EXPRESSION(ref_name, expr) \
    MAYBE(ref_name, ctx.convert_expr(expr));

#define EBMA_CONVERT_TYPE(ref_name, ...) \
    MAYBE(ref_name, ctx.convert_type(__VA_ARGS__));

#define EBMA_CONVERT_STATEMENT(ref_name, ...) \
    MAYBE(ref_name, ctx.convert_statement(__VA_ARGS__));

#define EBMA_ADD_IDENTIFIER(ref_name, ident) \
    MAYBE(ref_name, ctx.repository().add_identifier(ident));

#define EBMA_ADD_STRING(ref_name, candidate) \
    MAYBE(ref_name, ctx.repository().add_string(candidate));

#define EBMA_ADD_TYPE(ref_name, type) \
    MAYBE(ref_name, ctx.repository().add_type(type));

#define EBMA_ADD_STATEMENT(ref_name, ...) \
    MAYBE(ref_name, ctx.repository().add_statement(__VA_ARGS__));

#define EBMA_ADD_EXPR(ref_name, expr) \
    MAYBE(ref_name, ctx.repository().add_expr(expr));

    // ebm utility

#define EBMU_BOOL_TYPE(ref_name) \
    MAYBE(ref_name, get_bool_type(ctx))

#define EBMU_VOID_TYPE(ref_name) \
    MAYBE(ref_name, get_void_type(ctx))

#define EBMU_COUNTER_TYPE(ref_name) \
    MAYBE(ref_name, get_counter_type(ctx))

#define EBMU_UINT_TYPE(ref_name, n) \
    MAYBE(ref_name, get_unsigned_n_int(ctx, n))

#define EBMU_U8(ref_name) \
    EBMU_UINT_TYPE(ref_name, 8)

#define EBMU_U8_N_ARRAY(ref_name, n) \
    MAYBE(ref_name, get_u8_n_array(ctx, n))

#define EBMU_INT_LITERAL(ref_name, value) \
    MAYBE(ref_name, get_int_literal(ctx, value))

    // main converter functions

    ebm::ExpressionBody make_default_value(ebm::TypeRef type);

#define EBM_DEFAULT_VALUE(ref_name, typ) \
    EBM_AST_EXPRESSION(ref_name, make_default_value, typ)

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref, bool is_const, bool is_reference);

    ebm::ExpressionBody make_identifier_expr(ebm::StatementRef id, ebm::TypeRef type);

#define EBM_IDENTIFIER(ref_name, id, typ) \
    EBM_AST_EXPRESSION(ref_name, make_identifier_expr, id, typ)

#define EBM_DEFINE_VARIABLE(ref_name, id, typ, initial_ref, is_const, is_reference)                           \
    EBM_AST_VARIABLE_REF(ref_name) {                                                                          \
        EBMA_ADD_STATEMENT(new_var_ref_, (make_variable_decl(id, typ, initial_ref, is_const, is_reference))); \
        EBM_IDENTIFIER(new_expr_ref_, new_var_ref_, typ);                                                     \
        EBM_AST_VARIABLE_REF_SET(ref_name, new_expr_ref_, new_var_ref_);                                      \
    }

#define EBM_DEFINE_ANONYMOUS_VARIABLE(ref_name, typ, initial_ref)                                             \
    ebm::ExpressionRef ref_name;                                                                              \
    ebm::StatementRef ref_name##_def;                                                                         \
    {                                                                                                         \
        MAYBE(temporary_name, ctx.repository().anonymous_identifier());                                       \
        EBMA_ADD_STATEMENT(new_var_ref_, make_variable_decl(temporary_name, typ, initial_ref, false, false)); \
        EBM_IDENTIFIER(new_expr_ref_, new_var_ref_, typ);                                                     \
        EBM_AST_VARIABLE_REF_SET(ref_name, new_expr_ref_, new_var_ref_);                                      \
    }

    ebm::ExpressionBody make_cast(ebm::TypeRef to_typ, ebm::TypeRef from_typ, ebm::ExpressionRef expr, ebm::CastType cast_kind);

#define EBM_CAST(ref_name, to_typ, from_typ, expr)                                                 \
    ebm::ExpressionRef ref_name;                                                                   \
    {                                                                                              \
        if (from_typ.id.value() != to_typ.id.value()) {                                            \
            MAYBE(cast_kind________, ctx.get_type_converter().get_cast_type(to_typ, from_typ));    \
            EBMA_ADD_EXPR(cast_ref________, make_cast(to_typ, from_typ, expr, cast_kind________)); \
            ref_name = cast_ref________;                                                           \
        }                                                                                          \
        else {                                                                                     \
            ref_name = expr;                                                                       \
        }                                                                                          \
    }

    ebm::ExpressionBody make_binary_op(ebm::BinaryOp bop, ebm::TypeRef type, ebm::ExpressionRef left, ebm::ExpressionRef right);

#define EBM_BINARY_OP(ref_name, bop__, typ, left__, right__) \
    EBM_AST_EXPRESSION(ref_name, make_binary_op, bop__, typ, left__, right__)

    ebm::ExpressionBody make_unary_op(ebm::UnaryOp uop, ebm::TypeRef type, ebm::ExpressionRef operand);

#define EBM_UNARY_OP(ref_name, uop__, typ, operand__) \
    EBM_AST_EXPRESSION(ref_name, make_unary_op, uop__, typ, operand__)

    ebm::StatementBody make_assignment(ebm::ExpressionRef target, ebm::ExpressionRef value, ebm::StatementRef previous_assignment);

#define EBM_ASSIGNMENT(ref_name, target__, value__) \
    EBM_AST_STATEMENT(ref_name, make_assignment, target__, value__, {})

    ebm::ExpressionBody make_index(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef index);

#define EBM_INDEX(ref_name, type, base__, index__) \
    EBM_AST_EXPRESSION(ref_name, make_index, type, base__, index__)

    ebm::StatementBody make_write_data(ebm::IOData io_data);

#define EBM_WRITE_DATA(ref_name, io_data) \
    EBM_AST_STATEMENT(ref_name, make_write_data, io_data)

    ebm::StatementBody make_read_data(ebm::IOData io_data);

#define EBM_READ_DATA(ref_name, io_data) \
    EBM_AST_STATEMENT(ref_name, make_read_data, io_data)

// internally, represent as i = i + 1 because no INCREMENT operator in EBM
#define EBM_INCREMENT(ref_name, target__, target_type_)                              \
    ebm::StatementRef ref_name;                                                      \
    {                                                                                \
        EBMU_INT_LITERAL(one, 1);                                                    \
        EBM_BINARY_OP(incremented, ebm::BinaryOp::add, target_type_, target__, one); \
        EBM_ASSIGNMENT(increment_ref____, target__, incremented);                    \
        ref_name = increment_ref____;                                                \
    }

    ebm::StatementBody make_block(ebm::Block&& block);

#define EBM_BLOCK(ref_name, block_body__) \
    EBM_AST_STATEMENT(ref_name, make_block, block_body__)

    ebm::StatementBody make_loop(ebm::LoopStatement&& loop_stmt);

#define EBM_LOOP(ref_name, loop_stmt__) \
    EBM_AST_STATEMENT(ref_name, make_loop, std::move(loop_stmt__))

    ebm::StatementBody make_while_loop(ebm::ExpressionRef condition, ebm::StatementRef body);

#define EBM_WHILE_LOOP(ref_name, condition__, body__) \
    EBM_AST_STATEMENT(ref_name, make_while_loop, condition__, body__)

#define EBM_COUNTER_LOOP_START_CUSTOM(counter_name, counter_type)                       \
    EBM_AST_VARIABLE_REF(counter_name);                                                 \
    {                                                                                   \
        EBMU_INT_LITERAL(zero, 0);                                                      \
        EBM_DEFINE_ANONYMOUS_VARIABLE(counter_name##__, counter_type, zero);            \
        EBM_AST_VARIABLE_REF_SET(counter_name, counter_name##__, counter_name##___def); \
    }

#define EBM_COUNTER_LOOP_START(counter_name)                                            \
    EBM_AST_VARIABLE_REF(counter_name);                                                 \
    {                                                                                   \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBM_COUNTER_LOOP_START_CUSTOM(counter_name##__, counter_type);                  \
        EBM_AST_VARIABLE_REF_SET(counter_name, counter_name##__, counter_name##___def); \
    }

#define EBM_COUNTER_LOOP_END(loop_stmt, counter_name, limit_expr__, body__)             \
    ebm::StatementRef loop_stmt;                                                        \
    {                                                                                   \
        EBMU_BOOL_TYPE(bool_type);                                                      \
        EBM_BINARY_OP(cmp, ebm::BinaryOp::less, bool_type, counter_name, limit_expr__); \
        ebm::Block loop_body;                                                           \
        loop_body.container.reserve(2);                                                 \
        append(loop_body, body__);                                                      \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBM_INCREMENT(inc, counter_name, counter_type);                                 \
        append(loop_body, inc);                                                         \
        EBM_BLOCK(loop_block, std::move(loop_body));                                    \
        EBM_WHILE_LOOP(loop_stmt__, cmp, loop_block);                                   \
        loop_stmt = loop_stmt__;                                                        \
    }

#define EBM_COUNTER_LOOP_END_BODY(loop_stmt, counter_name, limit_expr__, body__)        \
    ebm::StatementBody loop_stmt;                                                       \
    {                                                                                   \
        EBMU_BOOL_TYPE(bool_type);                                                      \
        EBM_BINARY_OP(cmp, ebm::BinaryOp::less, bool_type, counter_name, limit_expr__); \
        ebm::Block loop_body;                                                           \
        loop_body.container.reserve(2);                                                 \
        append(loop_body, body__);                                                      \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBM_INCREMENT(inc, counter_name, counter_type);                                 \
        append(loop_body, inc);                                                         \
        EBM_BLOCK(loop_block, std::move(loop_body));                                    \
        loop_stmt = make_while_loop(cmp, loop_block);                                   \
    }

    ebm::ExpressionBody make_array_size(ebm::TypeRef type, ebm::ExpressionRef array_expr);

#define EBM_ARRAY_SIZE(ref_name, array_expr__)                                          \
    ebm::ExpressionRef ref_name;                                                        \
    {                                                                                   \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBMA_ADD_EXPR(array_size_ref____, make_array_size(counter_type, array_expr__)); \
        ref_name = array_size_ref____;                                                  \
    }

    ebm::LoweredStatement make_lowered_statement(ebm::LoweringType lowering_type, ebm::StatementRef body);

    ebm::StatementBody make_error_report(ebm::StringRef message, ebm::Expressions&& arguments);

#define EBM_ERROR_REPORT(ref_name, message__, arguments__) \
    EBM_AST_STATEMENT(ref_name, make_error_report, message__, arguments__)

    ebm::StatementBody make_if_statement(ebm::ExpressionRef condition, ebm::StatementRef then_block, ebm::StatementRef else_block);
#define EBM_IF_STATEMENT(ref_name, condition__, then_block__, else_block__) \
    EBM_AST_STATEMENT(ref_name, make_if_statement, condition__, then_block__, else_block__)

    ebm::StatementBody make_lowered_statements(ebm::LoweredStatements&& lowered_stmts);

#define EBM_LOWERED_STATEMENTS(ref_name, lowered_stmts__) \
    EBM_AST_STATEMENT(ref_name, make_lowered_statements, lowered_stmts__)

    ebm::StatementBody make_assert_statement(ebm::ExpressionRef condition, ebm::StatementRef lowered_statement);
#define EBM_ASSERT(ref_name, condition__, lowered_statement__) \
    EBM_AST_STATEMENT(ref_name, make_assert_statement, condition__, lowered_statement__)

    ebm::ExpressionBody make_member_access(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef member);
#define EBM_MEMBER_ACCESS(ref_name, type, base__, member__) \
    EBM_AST_EXPRESSION(ref_name, make_member_access, type, base__, member__)

    ebm::ExpressionBody make_call(ebm::TypeRef typ, ebm::CallDesc&& call_desc);
#define EBM_CALL(ref_name, typ, call_desc__) \
    EBM_AST_EXPRESSION(ref_name, make_call, typ, call_desc__)

    ebm::StatementBody make_expression_statement(ebm::ExpressionRef expr);

#define EBM_EXPR_STATEMENT(ref_name, expr) \
    EBM_AST_STATEMENT(ref_name, make_expression_statement, expr)

    ebm::ExpressionBody make_error_check(ebm::TypeRef type, ebm::ExpressionRef target_expr);
#define EBM_IS_ERROR(ref_name, target_expr)                                       \
    ebm::ExpressionRef ref_name;                                                  \
    {                                                                             \
        EBMU_BOOL_TYPE(bool_type);                                                \
        EBMA_ADD_EXPR(error_check_ref, make_error_check(bool_type, target_expr)); \
        ref_name = error_check_ref;                                               \
    }

    ebm::StatementBody make_error_return(ebm::ExpressionRef value);
#define EBM_ERROR_RETURN(ref_name, value) \
    EBM_AST_STATEMENT(ref_name, make_error_return, value)

    ebm::ExpressionBody make_max_value(ebm::TypeRef type, ebm::ExpressionRef lowered_expr);

#define EBM_MAX_VALUE(ref_name, type, lowered_expr) \
    EBM_AST_EXPRESSION(ref_name, make_max_value, type, lowered_expr)

    ebm::ExpressionBody make_can_read_stream(ebm::TypeRef type, ebm::StreamType stream_type, ebm::Size num_bytes);

#define EBM_CAN_READ_STREAM(ref_name, stream_type, num_bytes) \
    EBMU_BOOL_TYPE(ref_name##_type_____);                     \
    EBM_AST_EXPRESSION(ref_name, make_can_read_stream, ref_name##_type_____, stream_type, num_bytes);

    ebm::StatementBody make_append(ebm::ExpressionRef target, ebm::ExpressionRef value);
#define EBM_APPEND(ref_name, target, value) \
    EBM_AST_STATEMENT(ref_name, make_append, target, value)

    ebm::ExpressionBody make_get_remaining_bytes(ebm::TypeRef type, ebm::StreamType stream_type);

#define EBM_GET_REMAINING_BYTES(ref_name, stream_type) \
    EBMU_COUNTER_TYPE(ref_name##_type_____);           \
    EBM_AST_EXPRESSION(ref_name, make_get_remaining_bytes, ref_name##_type_____, stream_type)

    ebm::StatementBody make_break(ebm::StatementRef loop_id);
    ebm::StatementBody make_continue(ebm::StatementRef loop_id);

#define EBM_BREAK(ref_name, loop_id) \
    EBM_AST_STATEMENT(ref_name, make_break, loop_id)
#define EBM_CONTINUE(ref_name, loop_id) \
    EBM_AST_STATEMENT(ref_name, make_continue, loop_id)

    ebm::IOData make_io_data(ebm::ExpressionRef target, ebm::TypeRef data_type, ebm::IOAttribute attr, ebm::Size size);

    expected<ebm::Size> make_fixed_size(size_t n, ebm::SizeUnit unit);
    expected<ebm::Size> make_dynamic_size(ebm::ExpressionRef ref, ebm::SizeUnit unit);
    ebm::Size get_size(size_t bit_size);

#define COMMON_BUFFER_SETUP(IO_MACRO, io_ref)                              \
    EBMU_U8_N_ARRAY(u8_n_array, n);                                        \
    EBM_DEFAULT_VALUE(new_obj_ref, u8_n_array);                            \
    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);        \
    EBMU_UINT_TYPE(value_type, n * 8);                                     \
    EBMU_INT_LITERAL(zero, 0);                                             \
    MAYBE(io_size, make_fixed_size(n, ebm::SizeUnit::BYTE_FIXED));         \
    IO_MACRO(io_ref, (make_io_data(buffer, u8_n_array, endian, io_size))); \
    EBMU_U8(u8_t);

}  // namespace ebmgen
