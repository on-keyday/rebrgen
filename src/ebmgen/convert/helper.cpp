/*license*/
#include "helper.hpp"
#include "ebm/extended_binary_module.hpp"

namespace ebmgen {
    ebm::ExpressionBody make_default_value(ebm::TypeRef type) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::DEFAULT_VALUE;
        body.type = type;
        return body;
    }

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref, bool is_const, bool is_reference) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
        ebm::VariableDecl var_decl;
        var_decl.name = name;
        var_decl.var_type = type;
        var_decl.initial_value = initial_ref;
        var_decl.is_constant(is_const);
        var_decl.is_reference(is_reference);
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

    ebm::ExpressionBody make_index(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef index) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::INDEX_ACCESS;
        body.type = type;
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

    ebm::StatementBody make_block(ebm::Block&& block) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::BLOCK;
        body.block(std::move(block));
        return body;
    }

    ebm::StatementBody make_while_loop(ebm::ExpressionRef condition, ebm::StatementRef body) {
        ebm::StatementBody body_;
        body_.statement_kind = ebm::StatementOp::LOOP_STATEMENT;
        ebm::LoopStatement loop_stmt;
        loop_stmt.loop_type = ebm::LoopType::WHILE;
        loop_stmt.condition(condition);
        loop_stmt.body = body;
        body_.loop(std::move(loop_stmt));
        return body_;
    }

    ebm::ExpressionBody make_array_size(ebm::TypeRef type, ebm::ExpressionRef array_expr) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::ARRAY_SIZE;
        body.type = type;
        body.array_expr(array_expr);
        return body;
    }

    ebm::LoweredStatement make_lowered_statement(ebm::LoweringType lowering_type, ebm::StatementRef body) {
        ebm::LoweredStatement lowered;
        lowered.lowering_type = lowering_type;
        lowered.block = body;
        return lowered;
    }

    ebm::StatementBody make_error_report(ebm::StringRef message, ebm::Expressions&& arguments) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::ERROR_REPORT;
        ebm::ErrorReport error_report;
        error_report.message = message;
        error_report.arguments = std::move(arguments);
        body.error_report(std::move(error_report));
        return body;
    }

#define ASSERT_ID(id_) assert(id_.id.value() != 0 && "Invalid ID for expression or statement")

    ebm::StatementBody make_if_statement(ebm::ExpressionRef condition, ebm::StatementRef then_block, ebm::StatementRef else_block) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::IF_STATEMENT;
        ebm::IfStatement if_stmt;
        ASSERT_ID(condition);
        ASSERT_ID(then_block);
        if_stmt.condition = condition;
        if_stmt.then_block = then_block;
        if_stmt.else_block = else_block;
        body.if_statement(std::move(if_stmt));
        return body;
    }

    ebm::StatementBody make_lowered_statements(ebm::LoweredStatements&& lowered_stmts) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::LOWERED_STATEMENTS;
        body.lowered_statements(std::move(lowered_stmts));
        return body;
    }

    ebm::StatementBody make_assert_statement(ebm::ExpressionRef condition, ebm::StatementRef lowered_statement) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::ASSERT;
        ebm::AssertDesc assert_desc;
        ASSERT_ID(condition);
        assert_desc.condition = condition;
        assert_desc.lowered_statement = lowered_statement;
        body.assert_desc(std::move(assert_desc));
        return body;
    }

    ebm::ExpressionBody make_member_access(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef member) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::MEMBER_ACCESS;
        body.type = type;
        body.base(base);
        body.member(member);
        return body;
    }

    ebm::ExpressionBody make_call(ebm::CallDesc&& call_desc) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::CALL;
        body.call_desc(std::move(call_desc));
        return body;
    }

    ebm::StatementBody make_expression_statement(ebm::ExpressionRef expr) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::EXPRESSION;
        body.expression(expr);
        return body;
    }

    ebm::ExpressionBody make_error_check(ebm::TypeRef type, ebm::ExpressionRef target_expr) {
        ebm::ExpressionBody body;
        body.type = type;
        body.op = ebm::ExpressionOp::IS_ERROR;
        body.target_expr(target_expr);
        return body;
    }

    ebm::StatementBody make_error_return(ebm::ExpressionRef value) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::ERROR_RETURN;
        body.value(value);
        return body;
    }

    ebm::ExpressionBody make_max_value(ebm::TypeRef type, ebm::ExpressionRef lowered_expr) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::MAX_VALUE;
        body.type = type;
        body.lowered_expr(lowered_expr);
        return body;
    }

    ebm::StatementBody make_loop(ebm::LoopStatement&& loop_stmt) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::LOOP_STATEMENT;
        body.loop(std::move(loop_stmt));
        return body;
    }

    ebm::IOData make_io_data(ebm::ExpressionRef target, ebm::TypeRef data_type, ebm::EndianExpr endian, ebm::Size size) {
        return ebm::IOData{
            .target = target,
            .data_type = data_type,
            .endian = endian,
            .size = size,
            .lowered_stmt = ebm::StatementRef{},
        };
    }

    expected<ebm::Size> make_fixed_size(size_t n, ebm::SizeUnit unit) {
        ebm::Size size;
        size.unit = unit;
        MAYBE(size_val, varint(n));
        if (!size.size(size_val)) {
            return unexpect_error("Unit {} is not fixed", to_string(unit));
        }
        return size;
    }

    expected<ebm::Size> make_dynamic_size(ebm::ExpressionRef ref, ebm::SizeUnit unit) {
        ebm::Size size;
        size.unit = unit;
        if (!size.ref(ref)) {
            return unexpect_error("Unit {} is not dynamic", to_string(unit));
        }
        return size;
    }

    ebm::ExpressionBody make_can_read_stream(ebm::TypeRef type, ebm::StreamType stream_type, ebm::Size num_bytes) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::CAN_READ_STREAM;
        body.type = type;
        body.stream_type(stream_type);
        body.num_bytes(num_bytes);
        return body;
    }

    ebm::StatementBody make_append(ebm::ExpressionRef target, ebm::ExpressionRef value) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::APPEND;
        body.target(target);
        body.value(value);
        return body;
    }

    ebm::ExpressionBody make_get_remaining_bytes(ebm::TypeRef type, ebm::StreamType stream_type) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::GET_REMAINING_BYTES;
        body.type = type;
        body.stream_type(stream_type);
        return body;
    }

    ebm::StatementBody make_break(ebm::StatementRef loop_id) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::BREAK;
        body.break_(ebm::LoopFlowControl{.related_statement = loop_id});
        return body;
    }
    ebm::StatementBody make_continue(ebm::StatementRef loop_id) {
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::CONTINUE;
        body.continue_(ebm::LoopFlowControl{.related_statement = loop_id});
        return body;
    }

}  // namespace ebmgen
