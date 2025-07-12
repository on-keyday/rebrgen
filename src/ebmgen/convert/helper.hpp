/*license*/
#pragma once

#include "ebm/extended_binary_module.hpp"
namespace ebmgen {
#define MAYBE_VOID(out, expr)                                \
    auto out##____ = expr;                                   \
    if (!out##____) {                                        \
        return unexpect_error(std::move(out##____.error())); \
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
    EBM_AST_CONSTRUCTOR(ref_name, StatementRef, add_statement, make_func, __VA_ARGS__)

#define EBM_AST_EXPRESSION(ref_name, make_func, ...) \
    EBM_AST_CONSTRUCTOR(ref_name, ExpressionRef, add_expr, make_func, __VA_ARGS__)

    ebm::ExpressionBody make_new_object_body(ebm::TypeRef type);

#define EBM_NEW_OBJECT(ref_name, typ) \
    EBM_AST_EXPRESSION(ref_name, make_new_object_body, typ)

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref);

    ebm::ExpressionBody make_identifier_expr(ebm::StatementRef id, ebm::TypeRef type);

#define EBM_IDENTIFIER(ref_name, id, typ) \
    EBM_AST_EXPRESSION(ref_name, make_identifier_expr, id, typ)

#define EBM_DEFINE_ANONYMOUS_VARIABLE(ref_name, typ, initial_ref)                                 \
    ebm::ExpressionRef ref_name;                                                                  \
    ebm::StatementRef ref_name##_def;                                                             \
    {                                                                                             \
        MAYBE(temporary_name, identifier_repo.new_id(ident_source));                              \
        MAYBE(new_var_ref_, add_statement(make_variable_decl(temporary_name, typ, initial_ref))); \
        EBM_IDENTIFIER(new_expr_ref_, new_var_ref_, typ);                                         \
        ref_name = new_expr_ref_;                                                                 \
        ref_name##_def = new_var_ref_;                                                            \
    }

    ebm::ExpressionBody make_cast(ebm::TypeRef to_typ, ebm::TypeRef from_typ, ebm::ExpressionRef expr, ebm::CastType cast_kind);

#define EBM_CAST(ref_name, to_typ, from_typ, expr)                                                   \
    ebm::ExpressionRef ref_name;                                                                     \
    {                                                                                                \
        if (from_typ.id.value() != to_typ.id.value()) {                                              \
            MAYBE(cast_kind________, get_cast_type(to_typ, from_typ));                               \
            MAYBE(cast_ref________, add_expr(make_cast(to_typ, from_typ, expr, cast_kind________))); \
            ref_name = cast_ref________;                                                             \
        }                                                                                            \
        else {                                                                                       \
            ref_name = expr;                                                                         \
        }                                                                                            \
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
        MAYBE(one, get_int_literal(1));                                              \
        EBM_BINARY_OP(incremented, ebm::BinaryOp::add, target_type_, target__, one); \
        EBM_ASSIGNMENT(increment_ref____, target__, incremented);                    \
        ref_name = increment_ref____;                                                \
    }

    ebm::StatementBody make_block(ebm::Block&& block);

#define EBM_BLOCK(ref_name, block_body__) \
    EBM_AST_STATEMENT(ref_name, make_block, block_body__)

    ebm::StatementBody make_while_loop(ebm::ExpressionRef condition, ebm::StatementRef body);

#define EBM_WHILE_LOOP(ref_name, condition__, body__) \
    EBM_AST_STATEMENT(ref_name, make_while_loop, condition__, body__)

#define EBM_COUNTER_LOOP_START(counter_name)                          \
    ebm::ExpressionRef counter_name;                                  \
    ebm::StatementRef counter_name##_def;                             \
    {                                                                 \
        MAYBE(zero, get_int_literal(0));                              \
        MAYBE(counter_type, get_counter_type());                      \
        EBM_DEFINE_ANONYMOUS_VARIABLE(counter__, counter_type, zero); \
        counter_name = counter__;                                     \
        counter_name##_def = counter___def;                           \
    }

#define EBM_COUNTER_LOOP_END(loop_stmt, counter_name, limit_expr__, body__)             \
    ebm::StatementRef loop_stmt;                                                        \
    {                                                                                   \
        MAYBE(bool_type, get_bool_type());                                              \
        EBM_BINARY_OP(cmp, ebm::BinaryOp::less, bool_type, counter_name, limit_expr__); \
        ebm::Block loop_body;                                                           \
        loop_body.container.reserve(2);                                                 \
        append(loop_body, body__);                                                      \
        MAYBE(counter_type, get_counter_type());                                        \
        EBM_INCREMENT(inc, counter_name, counter_type);                                 \
        append(loop_body, inc);                                                         \
        EBM_BLOCK(loop_block, std::move(loop_body));                                    \
        EBM_WHILE_LOOP(loop_stmt__, cmp, loop_block);                                   \
        loop_stmt = loop_stmt__;                                                        \
    }

    ebm::ExpressionBody make_array_size(ebm::TypeRef type, ebm::ExpressionRef array_expr);

#define EBM_ARRAY_SIZE(ref_name, array_expr__)                                            \
    ebm::ExpressionRef ref_name;                                                          \
    {                                                                                     \
        MAYBE(counter_type, get_counter_type());                                          \
        MAYBE(array_size_ref____, add_expr(make_array_size(counter_type, array_expr__))); \
        ref_name = array_size_ref____;                                                    \
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

    ebm::ExpressionBody make_member_access(ebm::TypeRef type, ebm::ExpressionRef base, ebm::StatementRef member);
#define EBM_MEMBER_ACCESS(ref_name, type, base__, member__) \
    EBM_AST_EXPRESSION(ref_name, make_member_access, type, base__, member__)

    ebm::ExpressionBody make_call(ebm::CallDesc&& call_desc);
#define EBM_CALL(ref_name, call_desc__) \
    EBM_AST_EXPRESSION(ref_name, make_call, call_desc__)

}  // namespace ebmgen
