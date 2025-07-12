/*license*/
#include "helper.hpp"
#include "ebm/extended_binary_module.hpp"

namespace ebmgen {
    ebm::ExpressionBody make_new_object_body(ebm::TypeRef type) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::NEW_OBJECT;
        body.type = type;
        return body;
    }

    ebm::StatementBody make_anonymous_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
        ebm::VariableDecl var_decl;
        var_decl.name = name;
        var_decl.var_type = type;
        var_decl.initial_value = initial_ref;
        var_decl.is_constant(false);
        body.var_decl(std::move(var_decl));
        return body;
    }

    ebm::ExpressionBody make_identifier_expr(ebm::StatementRef id, ebm::TypeRef type) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::IDENTIFIER;
        body.type = type;
        body.id(id);
        return body;
    }

    ebm::ExpressionBody make_cast(ebm::TypeRef to_typ, ebm::TypeRef from_typ, ebm::ExpressionRef expr, ebm::CastType cast_kind) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::TYPE_CAST;
        body.type = to_typ;
        body.from_type(from_typ);
        body.source_expr(expr);
        body.cast_kind(cast_kind);
        return body;
    }

    ebm::ExpressionBody make_binary_op(ebm::BinaryOp bop, ebm::TypeRef type, ebm::ExpressionRef left, ebm::ExpressionRef right) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::BINARY_OP;
        body.type = type;
        body.bop(bop);
        body.left(left);
        body.right(right);
        return body;
    }

    ebm::ExpressionBody make_unary_op(ebm::UnaryOp uop, ebm::TypeRef type, ebm::ExpressionRef operand) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::UNARY_OP;
        body.type = type;
        body.uop(uop);
        body.operand(operand);
        return body;
    }

    ebm::StatementBody make_assignment(ebm::ExpressionRef target, ebm::ExpressionRef value, ebm::StatementRef previous_assignment) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::ASSIGNMENT;
        body.target(target);
        body.value(value);
        body.previous_assignment(previous_assignment);
        return body;
    }

    ebm::ExpressionBody make_index(ebm::ExpressionRef base, ebm::ExpressionRef index) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::INDEX_ACCESS;
        body.base(base);
        body.index(index);
        return body;
    }

    ebm::StatementBody make_write_data(ebm::IOData io_data) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::WRITE_DATA;
        body.write_data(io_data);
        return body;
    }
    ebm::StatementBody make_read_data(ebm::IOData io_data) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::READ_DATA;
        body.read_data(io_data);
        return body;
    }
}  // namespace ebmgen
