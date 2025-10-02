/*license*/
#include "helper.hpp"
#include "ebm/extended_binary_module.hpp"

namespace ebmgen {
    ebm::ExpressionBody make_default_value(ebm::TypeRef type) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::DEFAULT_VALUE;
        body.type = type;
        return body;
    }

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref, bool is_const, bool is_reference) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::VARIABLE_DECL;
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
        body.kind = ebm::ExpressionKind::IDENTIFIER;
        body.type = type;
        body.id(id);
        return body;
    }

    ebm::ExpressionBody make_cast(ebm::TypeRef to_typ, ebm::TypeRef from_typ, ebm::ExpressionRef expr, ebm::CastType cast_kind) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::TYPE_CAST;
        body.type = to_typ;
        body.from_type(from_typ);
        body.source_expr(expr);
        body.cast_kind(cast_kind);
        return body;
    }

    ebm::ExpressionBody make_binary_op(ebm::BinaryOp bop, ebm::TypeRef type, ebm::ExpressionRef left, ebm::ExpressionRef right) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::BINARY_OP;
        body.type = type;
        body.bop(bop);
        body.left(left);
        body.right(right);
        return body;
    }

    ebm::ExpressionBody make_unary_op(ebm::UnaryOp uop, ebm::TypeRef type, ebm::ExpressionRef operand) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::UNARY_OP;
        body.type = type;
        body.uop(uop);
        body.operand(operand);
        return body;
    }

    ebm::StatementBody make_assignment(ebm::ExpressionRef target, ebm::ExpressionRef value, ebm::StatementRef previous_assignment) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::ASSIGNMENT;
        body.target(target);
        body.value(value);
        body.previous_assignment(previous_assignment);
        return body;
    }

    ebm::ExpressionBody make_index(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef index) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::INDEX_ACCESS;
        body.type = type;
        body.base(base);
        body.index(index);
        return body;
    }

    ebm::StatementBody make_write_data(ebm::IOData io_data) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::WRITE_DATA;
        body.write_data(io_data);
        return body;
    }

    ebm::StatementBody make_read_data(ebm::IOData io_data) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::READ_DATA;
        body.read_data(io_data);
        return body;
    }

    ebm::StatementBody make_block(ebm::Block&& block) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::BLOCK;
        body.block(std::move(block));
        return body;
    }

    ebm::StatementBody make_while_loop(ebm::ExpressionRef condition, ebm::StatementRef body) {
        ebm::LoopStatement loop_stmt;
        loop_stmt.loop_type = ebm::LoopType::WHILE;
        loop_stmt.condition(make_condition(condition));
        loop_stmt.body = body;
        return make_loop(std::move(loop_stmt));
    }

    ebm::StatementBody make_for_loop(ebm::StatementRef init, ebm::ExpressionRef condition, ebm::StatementRef step, ebm::StatementRef body) {
        ebm::LoopStatement loop_stmt;
        loop_stmt.loop_type = ebm::LoopType::FOR;
        loop_stmt.init(init);
        loop_stmt.condition(make_condition(condition));
        loop_stmt.increment(step);
        loop_stmt.body = body;
        return make_loop(std::move(loop_stmt));
    }

    ebm::ExpressionBody make_array_size(ebm::TypeRef type, ebm::ExpressionRef array_expr) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::ARRAY_SIZE;
        body.type = type;
        body.array_expr(array_expr);
        return body;
    }

    ebm::LoweredStatement make_lowered_statement(ebm::LoweringType lowering_type, ebm::StatementRef body) {
        ebm::LoweredStatement lowered;
        lowered.lowering_type = lowering_type;
        lowered.statement = body;
        return lowered;
    }

    ebm::StatementBody make_error_report(ebm::StringRef message, ebm::Expressions&& arguments) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::ERROR_REPORT;
        ebm::ErrorReport error_report;
        error_report.message = message;
        error_report.arguments = std::move(arguments);
        body.error_report(std::move(error_report));
        return body;
    }

#define ASSERT_ID(id_) assert(!is_nil(id_) && "Invalid ID for expression or statement")

    ebm::StatementBody make_if_statement(ebm::ExpressionRef condition, ebm::StatementRef then_block, ebm::StatementRef else_block) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::IF_STATEMENT;
        ebm::IfStatement if_stmt;
        ASSERT_ID(condition);
        ASSERT_ID(then_block);
        if_stmt.condition = make_condition(condition);
        if_stmt.then_block = then_block;
        if_stmt.else_block = else_block;
        body.if_statement(std::move(if_stmt));
        return body;
    }

    ebm::StatementBody make_lowered_statements(ebm::LoweredStatements&& lowered_stmts) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::LOWERED_STATEMENTS;
        body.lowered_statements(std::move(lowered_stmts));
        return body;
    }

    ebm::StatementBody make_assert_statement(ebm::ExpressionRef condition, ebm::StatementRef lowered_statement) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::ASSERT;
        ebm::AssertDesc assert_desc;
        ASSERT_ID(condition);
        assert_desc.condition = make_condition(condition);
        assert_desc.lowered_statement = ebm::LoweredStatementRef{lowered_statement};
        body.assert_desc(std::move(assert_desc));
        return body;
    }

    ebm::ExpressionBody make_member_access(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef member) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::MEMBER_ACCESS;
        body.type = type;
        body.base(base);
        body.member(member);
        return body;
    }

    ebm::ExpressionBody make_call(ebm::TypeRef type, ebm::CallDesc&& call_desc) {
        ebm::ExpressionBody body;
        body.type = type;
        body.kind = ebm::ExpressionKind::CALL;
        body.call_desc(std::move(call_desc));
        return body;
    }

    ebm::StatementBody make_expression_statement(ebm::ExpressionRef expr) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::EXPRESSION;
        body.expression(expr);
        return body;
    }

    ebm::ExpressionBody make_error_check(ebm::TypeRef type, ebm::ExpressionRef target_expr) {
        ebm::ExpressionBody body;
        body.type = type;
        body.kind = ebm::ExpressionKind::IS_ERROR;
        body.target_expr(target_expr);
        return body;
    }

    ebm::StatementBody make_error_return(ebm::ExpressionRef value) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::ERROR_RETURN;
        body.value(value);
        return body;
    }

    ebm::ExpressionBody make_max_value(ebm::TypeRef type, ebm::ExpressionRef lowered_expr) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::MAX_VALUE;
        body.type = type;
        body.lowered_expr(ebm::LoweredExpressionRef{lowered_expr});
        return body;
    }

    ebm::StatementBody make_loop(ebm::LoopStatement&& loop_stmt) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::LOOP_STATEMENT;
        body.loop(std::move(loop_stmt));
        return body;
    }

    ebm::IOData make_io_data(ebm::StatementRef io_ref, ebm::ExpressionRef target, ebm::TypeRef data_type, ebm::IOAttribute attr, ebm::Size size) {
        return ebm::IOData{
            .io_ref = io_ref,
            .target = target,
            .data_type = data_type,
            .attribute = attr,
            .size = size,
            .lowered_statement = ebm::LoweredStatementRef{},
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
        body.kind = ebm::ExpressionKind::CAN_READ_STREAM;
        body.type = type;
        body.stream_type(stream_type);
        body.num_bytes(num_bytes);
        return body;
    }

    ebm::StatementBody make_append(ebm::ExpressionRef target, ebm::ExpressionRef value) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::APPEND;
        body.target(target);
        body.value(value);
        return body;
    }

    ebm::ExpressionBody make_get_remaining_bytes(ebm::TypeRef type, ebm::StreamType stream_type) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::GET_REMAINING_BYTES;
        body.type = type;
        body.stream_type(stream_type);
        return body;
    }

    ebm::StatementBody make_break(ebm::StatementRef loop_id) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::BREAK;
        body.break_(ebm::LoopFlowControl{.related_statement = loop_id});
        return body;
    }
    ebm::StatementBody make_continue(ebm::StatementRef loop_id) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::CONTINUE;
        body.continue_(ebm::LoopFlowControl{.related_statement = loop_id});
        return body;
    }

    ebm::ExpressionBody make_range(ebm::TypeRef type, ebm::ExpressionRef start, ebm::ExpressionRef end) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::RANGE;
        body.type = type;
        body.start(start);
        body.end(end);
        return body;
    }

    ebm::Condition make_condition(ebm::ExpressionRef cond) {
        ebm::Condition condition;
        condition.cond = cond;
        return condition;
    }

    ebm::ExpressionBody make_sub_range_init(ebm::TypeRef type, ebm::StatementRef init) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::SUB_RANGE_INIT;
        body.type = type;
        body.sub_range(init);
        return body;
    }

    ebm::StatementBody make_property_member_decl(ebm::PropertyMemberDecl&& prop_member) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::PROPERTY_MEMBER_DECL;
        body.property_member_decl(std::move(prop_member));
        return body;
    }

    ebm::StatementBody make_property_decl(ebm::PropertyDecl&& prop_decl) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::PROPERTY_DECL;
        body.property_decl(std::move(prop_decl));
        return body;
    }

    ebm::ExpressionBody make_or_cond(ebm::TypeRef type, ebm::Expressions&& conds) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::OR_COND;
        body.type = type;
        body.or_cond(std::move(conds));
        return body;
    }

    ebm::StatementBody make_return(ebm::ExpressionRef ret) {
        ebm::StatementBody body;
        body.kind = ebm::StatementKind::RETURN;
        body.value(ret);
        return body;
    }

    ebm::ExpressionBody make_addressof(ebm::TypeRef type, ebm::ExpressionRef target) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::ADDRESS_OF;
        body.type = type;
        body.target_expr(target);
        return body;
    }

    ebm::ExpressionBody make_setter_status(ebm::TypeRef type, ebm::SetterStatus status) {
        ebm::ExpressionBody body;
        body.kind = ebm::ExpressionKind::SETTER_STATUS;
        body.type = type;
        body.setter_status(status);
        return body;
    }

}  // namespace ebmgen
