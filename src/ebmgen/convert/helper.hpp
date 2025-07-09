/*license*/
#pragma once

#define MAYBE(out, expr)                                     \
    auto out##____ = expr;                                   \
    if (!out##____) {                                        \
        return unexpect_error(std::move(out##____.error())); \
    }                                                        \
    auto out = std::move(*out##____)

#define EBM_NEW_OBJECT(ref_name, typ)                             \
    ebm::ExpressionRef ref_name;                                  \
    {                                                             \
        ebm::ExpressionBody body___;                              \
        body___.op = ebm::ExpressionOp::NEW_OBJECT;               \
        body___.type = typ;                                       \
        MAYBE(new_obj_ref________, add_expr(std::move(body___))); \
        ref_name = new_obj_ref________;                           \
    }

#define EBM_DEFINE_ANONYMOUS_VARIABLE(ref_name, typ, initial_ref)      \
    ebm::ExpressionRef ref_name;                                       \
    ebm::StatementRef ref_name##_def;                                  \
    {                                                                  \
        ebm::StatementBody body___;                                    \
        body___.statement_kind = ebm::StatementOp::VARIABLE_DECL;      \
        ebm::VariableDecl var_decl___;                                 \
        auto temporary_name = identifier_repo.new_id(ident_source);    \
        var_decl___.name = *temporary_name;                            \
        var_decl___.var_type = typ;                                    \
        var_decl___.initial_value = initial_ref;                       \
        var_decl___.is_constant(false);                                \
        body___.var_decl(std::move(var_decl___));                      \
        MAYBE(new_var_ref________, add_statement(std::move(body___))); \
        ebm::ExpressionBody expr___;                                   \
        expr___.type = typ;                                            \
        expr___.op = ebm::ExpressionOp::IDENTIFIER;                    \
        expr___.id(new_var_ref________);                               \
        MAYBE(new_expr_ref____, add_expr(std::move(expr___)));         \
        ref_name = new_expr_ref____;                                   \
        ref_name##_def = new_var_ref________;                          \
    }

#define EBM_CAST(ref_name, to_typ, from_typ, expr)                 \
    ebm::ExpressionRef ref_name;                                   \
    {                                                              \
        ebm::ExpressionBody body___;                               \
        body___.op = ebm::ExpressionOp::TYPE_CAST;                 \
        body___.type = to_typ;                                     \
        body___.from_type(from_typ);                               \
        body___.source_expr(expr);                                 \
        MAYBE(cast_kind________, get_cast_type(to_typ, from_typ)); \
        body___.cast_kind(cast_kind________);                      \
        MAYBE(cast_ref________, add_expr(std::move(body___)));     \
        ref_name = cast_ref________;                               \
    }

#define EBM_BINARY_OP(ref_name, bop__, typ, left__, right__)    \
    ebm::ExpressionRef ref_name;                                \
    {                                                           \
        ebm::ExpressionBody body___;                            \
        body___.op = ebm::ExpressionOp::BINARY_OP;              \
        body___.type = typ;                                     \
        body___.bop(bop__);                                     \
        body___.left(left__);                                   \
        body___.right(right__);                                 \
        MAYBE(binary_op_ref____, add_expr(std::move(body___))); \
        ref_name = binary_op_ref____;                           \
    }

#define EBM_UNARY_OP(ref_name, uop__, operand__)               \
    ebm::ExpressionRef ref_name;                               \
    {                                                          \
        ebm::ExpressionBody body___;                           \
        body___.op = ebm::ExpressionOp::UNARY_OP;              \
        body___.uop(uop__);                                    \
        body___.operand(operand__);                            \
        MAYBE(unary_op_ref____, add_expr(std::move(body___))); \
        ref_name = unary_op_ref____;                           \
    }

#define EBM_ASSIGNMENT(ref_name, target__, value__)               \
    ebm::StatementRef ref_name;                                   \
    {                                                             \
        ebm::StatementBody body___;                               \
        body___.statement_kind = ebm::StatementOp::ASSIGNMENT;    \
        body___.target(target__);                                 \
        body___.value(value__);                                   \
        MAYBE(assign_ref____, add_statement(std::move(body___))); \
        ref_name = assign_ref____;                                \
    }

#define EBM_INDEX(ref_name, base__, index__)                \
    ebm::ExpressionRef ref_name;                            \
    {                                                       \
        ebm::ExpressionBody body___;                        \
        body___.op = ebm::ExpressionOp::INDEX_ACCESS;       \
        body___.base(base__);                               \
        body___.index(index__);                             \
        MAYBE(index_ref____, add_expr(std::move(body___))); \
        ref_name = index_ref____;                           \
    }

// internally, represent as i = i + 1 because no INCREMENT operator in EBM
#define EBM_INCREMENT(ref_name, target__)                             \
    ebm::StatementRef ref_name;                                       \
    {                                                                 \
        ebm::StatementBody body___;                                   \
        body___.statement_kind = ebm::StatementOp::ASSIGNMENT;        \
        ebm::ExpressionRef incremented;                               \
        {                                                             \
            ebm::ExpressionBody body___;                              \
            body___.op = ebm::ExpressionOp::BINARY_OP;                \
            body___.bop(ebm::BinaryOp::add);                          \
            body___.left(target__);                                   \
            MAYBE(one, get_int_literal(1));                           \
            body___.right(one);                                       \
            MAYBE(incremented_ref____, add_expr(std::move(body___))); \
            incremented = incremented_ref____;                        \
        }                                                             \
        body___.target(target__);                                     \
        body___.value(incremented);                                   \
        MAYBE(increment_ref____, add_statement(std::move(body___)));  \
        ref_name = increment_ref____;                                 \
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
