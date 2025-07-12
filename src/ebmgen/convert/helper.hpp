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

    ebm::ExpressionBody make_new_object_body(ebm::TypeRef type);

#define EBM_NEW_OBJECT(ref_name, typ)                                    \
    ebm::ExpressionRef ref_name;                                         \
    {                                                                    \
        MAYBE(new_obj_ref________, add_expr(make_new_object_body(typ))); \
        ref_name = new_obj_ref________;                                  \
    }

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref);

    ebm::ExpressionBody make_identifier_expr(ebm::StatementRef id, ebm::TypeRef type);

#define EBM_DEFINE_ANONYMOUS_VARIABLE(ref_name, typ, initial_ref)                                        \
    ebm::ExpressionRef ref_name;                                                                         \
    ebm::StatementRef ref_name##_def;                                                                    \
    {                                                                                                    \
        MAYBE(temporary_name, identifier_repo.new_id(ident_source));                                     \
        MAYBE(new_var_ref________, add_statement(make_variable_decl(temporary_name, typ, initial_ref))); \
        MAYBE(new_expr_ref____, add_expr(make_identifier_expr(new_var_ref________, typ)));               \
        ref_name = new_expr_ref____;                                                                     \
        ref_name##_def = new_var_ref________;                                                            \
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

#define EBM_BINARY_OP(ref_name, bop__, typ, left__, right__)                             \
    ebm::ExpressionRef ref_name;                                                         \
    {                                                                                    \
        MAYBE(binary_op_ref____, add_expr(make_binary_op(bop__, typ, left__, right__))); \
        ref_name = binary_op_ref____;                                                    \
    }

    ebm::ExpressionBody make_unary_op(ebm::UnaryOp uop, ebm::TypeRef type, ebm::ExpressionRef operand);

#define EBM_UNARY_OP(ref_name, uop__, typ, operand__)                            \
    ebm::ExpressionRef ref_name;                                                 \
    {                                                                            \
        MAYBE(unary_op_ref____, add_expr(make_unary_op(uop__, typ, operand__))); \
        ref_name = unary_op_ref____;                                             \
    }

    ebm::StatementBody make_assignment(ebm::ExpressionRef target, ebm::ExpressionRef value, ebm::StatementRef previous_assignment);

#define EBM_ASSIGNMENT(ref_name, target__, value__)                                   \
    ebm::StatementRef ref_name;                                                       \
    { /*currently previous assign is always null*/                                    \
        MAYBE(assign_ref____, add_statement(make_assignment(target__, value__, {}))); \
        ref_name = assign_ref____;                                                    \
    }

    ebm::ExpressionBody make_index(ebm::ExpressionRef base, ebm::ExpressionRef index);

#define EBM_INDEX(ref_name, base__, index__)                         \
    ebm::ExpressionRef ref_name;                                     \
    {                                                                \
        MAYBE(index_ref____, add_expr(make_index(base__, index__))); \
        ref_name = index_ref____;                                    \
    }

    ebm::StatementBody make_write_data(ebm::IOData io_data);

#define EBM_WRITE_DATA(ref_name, io_data)                                   \
    ebm::StatementRef ref_name;                                             \
    {                                                                       \
        MAYBE(write_data_ref____, add_statement(make_write_data(io_data))); \
        ref_name = write_data_ref____;                                      \
    }

    ebm::StatementBody make_read_data(ebm::IOData io_data);

#define EBM_READ_DATA(ref_name, io_data)                                  \
    ebm::StatementRef ref_name;                                           \
    {                                                                     \
        MAYBE(read_data_ref____, add_statement(make_read_data(io_data))); \
        ref_name = read_data_ref____;                                     \
    }

// internally, represent as i = i + 1 because no INCREMENT operator in EBM
#define EBM_INCREMENT(ref_name, target__, target_type_)                              \
    ebm::StatementRef ref_name;                                                      \
    {                                                                                \
        MAYBE(one, get_int_literal(1));                                              \
        EBM_BINARY_OP(incremented, ebm::BinaryOp::add, target_type_, target__, one); \
        EBM_ASSIGNMENT(increment_ref____, target__, incremented);                    \
        ref_name = increment_ref____;                                                \
    }

#define EBM_BLOCK(ref_name, block_body__)                        \
    ebm::StatementRef ref_name;                                  \
    {                                                            \
        ebm::StatementBody body___;                              \
        body___.statement_kind = ebm::StatementOp::BLOCK;        \
        body___.block(block_body__);                             \
        MAYBE(block_ref____, add_statement(std::move(body___))); \
        ref_name = block_ref____;                                \
    }

#define EBM_WHILE_LOOP(ref_name, collection__, body__)             \
    ebm::StatementRef ref_name;                                    \
    {                                                              \
        ebm::StatementBody body___;                                \
        body___.statement_kind = ebm::StatementOp::LOOP_STATEMENT; \
        ebm::LoopStatement loop_stmt___;                           \
        loop_stmt___.loop_type = ebm::LoopType::WHILE;             \
        loop_stmt___.collection(collection__);                     \
        loop_stmt___.body = body__;                                \
        body___.loop(std::move(loop_stmt___));                     \
        MAYBE(loop_ref____, add_statement(std::move(body___)));    \
        ref_name = loop_ref____;                                   \
    }

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

#define EBM_ARRAY_SIZE(ref_name, array_expr__)                   \
    ebm::ExpressionRef ref_name;                                 \
    {                                                            \
        ebm::ExpressionBody body___;                             \
        body___.op = ebm::ExpressionOp::ARRAY_SIZE;              \
        MAYBE(counter_type, get_counter_type());                 \
        body___.type = counter_type;                             \
        body___.array_expr(array_expr__);                        \
        MAYBE(array_size_ref____, add_expr(std::move(body___))); \
        ref_name = array_size_ref____;                           \
    }
}  // namespace ebmgen