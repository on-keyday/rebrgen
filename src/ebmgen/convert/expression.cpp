#include "../converter.hpp"
#include <core/ast/tool/ident.h>
#include "core/ast/node/ast_enum.h"
#include "core/ast/node/literal.h"
#include "core/ast/node/type.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "helper.hpp"
#include <fnet/util/base64.h>
#include <core/ast/traverse.h>
#include <memory>
#include <type_traits>

namespace ebmgen {
    expected<ebm::BinaryOp> convert_assignment_binary_op(ast::BinaryOp op) {
        switch (op) {
            case ast::BinaryOp::add_assign:
                return ebm::BinaryOp::add;
            case ast::BinaryOp::sub_assign:
                return ebm::BinaryOp::sub;
            case ast::BinaryOp::mul_assign:
                return ebm::BinaryOp::mul;
            case ast::BinaryOp::div_assign:
                return ebm::BinaryOp::div;
            case ast::BinaryOp::mod_assign:
                return ebm::BinaryOp::mod;
            case ast::BinaryOp::bit_and_assign:
                return ebm::BinaryOp::bit_and;
            case ast::BinaryOp::bit_or_assign:
                return ebm::BinaryOp::bit_or;
            case ast::BinaryOp::bit_xor_assign:
                return ebm::BinaryOp::bit_xor;
            case ast::BinaryOp::left_logical_shift_assign:
                return ebm::BinaryOp::left_shift;
            case ast::BinaryOp::right_logical_shift_assign:
                return ebm::BinaryOp::right_shift;
            default:
                return unexpect_error("Unsupported binary operator: {}", to_string(op));
        }
    }

    expected<ebm::BinaryOp> convert_binary_op(ast::BinaryOp op) {
        switch (op) {
            case ast::BinaryOp::add:
                return ebm::BinaryOp::add;
            case ast::BinaryOp::sub:
                return ebm::BinaryOp::sub;
            case ast::BinaryOp::mul:
                return ebm::BinaryOp::mul;
            case ast::BinaryOp::div:
                return ebm::BinaryOp::div;
            case ast::BinaryOp::mod:
                return ebm::BinaryOp::mod;
            case ast::BinaryOp::equal:
                return ebm::BinaryOp::equal;
            case ast::BinaryOp::not_equal:
                return ebm::BinaryOp::not_equal;
            case ast::BinaryOp::less:
                return ebm::BinaryOp::less;
            case ast::BinaryOp::less_or_eq:
                return ebm::BinaryOp::less_or_eq;
            case ast::BinaryOp::grater:
                return ebm::BinaryOp::greater;
            case ast::BinaryOp::grater_or_eq:
                return ebm::BinaryOp::greater_or_eq;
            case ast::BinaryOp::logical_and:
                return ebm::BinaryOp::logical_and;
            case ast::BinaryOp::logical_or:
                return ebm::BinaryOp::logical_or;
            case ast::BinaryOp::left_logical_shift:
                return ebm::BinaryOp::left_shift;
            case ast::BinaryOp::right_logical_shift:
                return ebm::BinaryOp::right_shift;
            case ast::BinaryOp::bit_and:
                return ebm::BinaryOp::bit_and;
            case ast::BinaryOp::bit_or:
                return ebm::BinaryOp::bit_or;
            case ast::BinaryOp::bit_xor:
                return ebm::BinaryOp::bit_xor;
            default:
                return convert_assignment_binary_op(op);
        }
    }

    expected<ebm::UnaryOp> convert_unary_op(ast::UnaryOp op, const std::shared_ptr<ast::Type>& type) {
        switch (op) {
            case ast::UnaryOp::not_:
                if (ast::as<ast::BoolType>(type)) {
                    return ebm::UnaryOp::logical_not;
                }
                return ebm::UnaryOp::bit_not;
            case ast::UnaryOp::minus_sign:
                return ebm::UnaryOp::minus_sign;
            default:
                return unexpect_error("Unsupported unary operator");
        }
    }

    expected<ebm::CastType> TypeConverter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = ctx.repository().get_type(dest_ref);
        auto src = ctx.repository().get_type(src_ref);

        if (!dest || !src) {
            return unexpect_error("Invalid type reference for cast");
        }

        if (dest->body.kind == ebm::TypeKind::INT || dest->body.kind == ebm::TypeKind::UINT) {
            if (src->body.kind == ebm::TypeKind::ENUM) {
                return ebm::CastType::ENUM_TO_INT;
            }
            if (src->body.kind == ebm::TypeKind::FLOAT) {
                return ebm::CastType::FLOAT_TO_INT_BIT;
            }
            if (src->body.kind == ebm::TypeKind::BOOL) {
                return ebm::CastType::BOOL_TO_INT;
            }
            // Handle int/uint size and signedness conversions
            if ((src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) && dest->body.size() && src->body.size()) {
                auto dest_size = dest->body.size()->value();
                auto src_size = src->body.size()->value();
                if (dest_size < src_size) {
                    return ebm::CastType::LARGE_INT_TO_SMALL_INT;
                }
                if (dest_size > src_size) {
                    return ebm::CastType::SMALL_INT_TO_LARGE_INT;
                }
                // Check signedness conversion
                if (dest->body.kind == ebm::TypeKind::UINT && src->body.kind == ebm::TypeKind::INT) {
                    return ebm::CastType::SIGNED_TO_UNSIGNED;
                }
                if (dest->body.kind == ebm::TypeKind::INT && src->body.kind == ebm::TypeKind::UINT) {
                    return ebm::CastType::UNSIGNED_TO_SIGNED;
                }
            }
        }
        else if (dest->body.kind == ebm::TypeKind::ENUM) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_ENUM;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::FLOAT) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_FLOAT_BIT;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::BOOL) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_BOOL;
            }
        }
        // TODO: Add more complex type conversions (ARRAY, VECTOR, STRUCT, RECURSIVE_STRUCT)

        return ebm::CastType::OTHER;
    }

    template <class T>
    concept has_convert_expr_impl = requires(T t) {
        { convert_expr_impl(std::declval<std::shared_ptr<T>>(), std::declval<ebm::ExpressionBody&>()) } -> std::same_as<expected<void>>;
    };

    expected<ebm::ExpressionRef> ExpressionConverter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        ebm::ExpressionBody body;
        EBMA_CONVERT_TYPE(type_ref, node->expr_type);
        body.type = type_ref;

        expected<void> result = unexpect_error("Unsupported expression type: {}", node_type_to_string(node->node_type));

        brgen::ast::visit(ast::cast_to<ast::Node>(node), [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(n)>, std::shared_ptr>::template param_at<0>;
            if constexpr (std::is_base_of_v<ast::Expr, T>) {
                result = convert_expr_impl(n, body);
            }
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        EBMA_ADD_EXPR(ref, std::move(body));
        ctx.repository().add_debug_loc(node->loc, ref);
        return ref;
    }

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type) {
        if (alignment_bytes == 0) {
            return unexpect_error("0 is not valid alignment");
        }
        if (alignment_bytes == 1) {
            EBMU_INT_LITERAL(lit, 1);
            return lit;
        }
        ebm::ExpressionBody body;
        EBMU_COUNTER_TYPE(counter_type);
        body.type = counter_type;
        body.kind = ebm::ExpressionOp::GET_STREAM_OFFSET;
        body.stream_type(type);
        body.unit(ebm::SizeUnit::BYTE_FIXED);
        EBMA_ADD_EXPR(stream_offset, std::move(body));

        EBMU_INT_LITERAL(alignment, alignment_bytes);

        if (std::popcount(alignment_bytes) == 1) {
            EBMU_INT_LITERAL(alignment_bitmask, alignment_bytes - 1);
            // size(=offset) & (alignment - 1)
            EBM_BINARY_OP(mod, ebm::BinaryOp::bit_and, counter_type, stream_offset, alignment_bitmask);
            // alignment - (size & (alignment-1))
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);
            // (alignment - (size & (alignment-1))) & (alignment - 1)
            EBM_BINARY_OP(req_size, ebm::BinaryOp::bit_and, counter_type, cmp, alignment_bitmask);
            return req_size;
        }
        else {
            // size(=offset) % alignment
            EBM_BINARY_OP(mod, ebm::BinaryOp::mod, counter_type, stream_offset, alignment);

            // alignment - (size % alignment)
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);

            // (alignment - (size % alignment)) % alignment
            EBM_BINARY_OP(req_size, ebm::BinaryOp::mod, counter_type, cmp, alignment);
            return req_size;
        }
    }
    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::IntLiteral>& node, ebm::ExpressionBody& body) {
        auto value = node->parse_as<std::uint64_t>();
        if (!value) {
            return unexpect_error("cannot parse int literal");
        }
        body = get_int_literal_body(body.type, *value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::BoolLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::LITERAL_BOOL;
        body.bool_value(node->value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::StrLiteral>& node, ebm::ExpressionBody& body) {
        MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(node)));
        body.kind = ebm::ExpressionOp::LITERAL_STRING;
        EBMA_ADD_STRING(str_ref, candidate);
        body.string_value(str_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::TypeLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::LITERAL_TYPE;
        EBMA_CONVERT_TYPE(type_ref_inner, node->type_literal)
        body.type_ref(type_ref_inner);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Ident>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::IDENTIFIER;
        auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
        if (!base) {
            return unexpect_error("Identifier {} not found", node->ident);
        }
        {
            const auto normal = ctx.state().set_current_generate_type(GenerateType::Normal);
            EBMA_CONVERT_STATEMENT(id_ref, base->first->base.lock());
            body.id(id_ref);
        }
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Binary>& node, ebm::ExpressionBody& body) {
        if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            return unexpect_error("define_assign/const_assign should be handled as a statement, not an expression");
        }
        EBMA_CONVERT_EXPRESSION(left_ref, node->left);
        EBMA_CONVERT_EXPRESSION(right_ref, node->right);
        MAYBE(bop, convert_binary_op(node->op));
        body = make_binary_op(bop, body.type, left_ref, right_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Unary>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::UNARY_OP;
        EBMA_CONVERT_EXPRESSION(operand_ref, node->expr);
        body.operand(operand_ref);
        MAYBE(uop, convert_unary_op(node->op, node->expr_type));
        body.uop(uop);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Index>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::INDEX_ACCESS;
        EBMA_CONVERT_EXPRESSION(base_ref, node->expr);
        EBMA_CONVERT_EXPRESSION(index_ref, node->index);
        body.base(base_ref);
        body.index(index_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::MemberAccess>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::MEMBER_ACCESS;
        EBMA_CONVERT_EXPRESSION(base_ref, node->target);
        EBMA_CONVERT_EXPRESSION(member_ref, node->member);
        body.base(base_ref);
        body.member(member_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Cast>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::TYPE_CAST;
        EBMA_CONVERT_EXPRESSION(source_expr_ref, node->arguments[0]);
        EBMA_CONVERT_TYPE(source_expr_type_ref, node->arguments[0]->expr_type);
        body.source_expr(source_expr_ref);
        body.from_type(source_expr_type_ref);
        MAYBE(cast_kind, ctx.get_type_converter().get_cast_type(body.type, source_expr_type_ref));
        body.cast_kind(cast_kind);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Range>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::RANGE;
        ebm::ExpressionRef start, end;
        if (node->start) {
            EBMA_CONVERT_EXPRESSION(start_ref, node->start);
            start = start_ref;
        }
        if (node->end) {
            EBMA_CONVERT_EXPRESSION(end_ref, node->end);
            end = end_ref;
        }
        body.start(start);
        body.end(end);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::IOOperation>& node, ebm::ExpressionBody& body) {
        switch (node->method) {
            case ast::IOMethod::input_get:
            case ast::IOMethod::input_peek: {
                body.kind = ebm::ExpressionOp::READ_DATA;
                EBMA_CONVERT_TYPE(type_ref, node->expr_type);
                EBM_DEFAULT_VALUE(default_, type_ref);
                EBM_DEFINE_ANONYMOUS_VARIABLE(var, type_ref, default_);
                body.target_stmt(var_def);
                MAYBE(decode_info, ctx.get_decoder_converter().decode_field_type(node->expr_type, var, nullptr));
                if (node->method == ast::IOMethod::input_peek) {
                    decode_info.read_data()->attribute.is_peek(true);
                }
                EBMA_ADD_STATEMENT(io_statement_ref, std::move(decode_info));
                body.io_statement(io_statement_ref);
                break;
            }
            case ast::IOMethod::output_put: {
                body.kind = ebm::ExpressionOp::WRITE_DATA;
                EBMA_CONVERT_EXPRESSION(output, node->arguments[0]);
                body.target_expr(output);
                MAYBE(encode_info, ctx.get_encoder_converter().encode_field_type(node->arguments[0]->expr_type, output, nullptr));
                EBMA_ADD_STATEMENT(io_statement_ref, std::move(encode_info));
                body.io_statement(io_statement_ref);
                break;
            }
            case ast::IOMethod::input_offset:
            case ast::IOMethod::input_bit_offset: {
                body.kind = ebm::ExpressionOp::GET_STREAM_OFFSET;
                body.stream_type(ebm::StreamType::INPUT);
                body.unit(node->method == ast::IOMethod::input_bit_offset ? ebm::SizeUnit::BIT_FIXED : ebm::SizeUnit::BYTE_FIXED);
                break;
            }
            case ast::IOMethod::input_remain: {
                body.kind = ebm::ExpressionOp::GET_REMAINING_BYTES;
                body.stream_type(ebm::StreamType::INPUT);
                break;
            }
            case ast::IOMethod::input_subrange: {
                return unexpect_error("not implemented yet: {}", ast::to_string(node->method));
            }
            default: {
                return unexpect_error("Unhandled IOMethod: {}", ast::to_string(node->method));
            }
        }
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Paren>& node, ebm::ExpressionBody& body) {
        expected<void> result = unexpect_error("Expected expression inside parentheses, got {}", node_type_to_string(node->expr->node_type));
        brgen::ast::visit(ast::cast_to<ast::Node>(node->expr), [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(n)>, std::shared_ptr>::template param_at<0>;
            if constexpr (std::is_base_of_v<ast::Expr, T>) {
                result = convert_expr_impl(n, body);
            }
            else {
                result = unexpect_error("Expected expression inside parentheses, got {}", node_type_to_string(node->expr->node_type));
            }
        });
        return result;
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::If>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::CONDITIONAL_STATEMENT;

        EBM_DEFINE_ANONYMOUS_VARIABLE(var, body.type, {});
        body.target_stmt(var_def);

        const auto _defer = ctx.state().set_current_yield_statement(var_def);

        EBMA_CONVERT_STATEMENT(conditional_stmt_ref, node);
        body.conditional_stmt(conditional_stmt_ref);

        return {};
    }
    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Match>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::CONDITIONAL_STATEMENT;

        EBM_DEFINE_ANONYMOUS_VARIABLE(var, body.type, {});
        body.target_stmt(var_def);

        const auto _defer = ctx.state().set_current_yield_statement(var_def);

        EBMA_CONVERT_STATEMENT(conditional_stmt_ref, node);
        body.conditional_stmt(conditional_stmt_ref);

        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::CharLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::LITERAL_CHAR;
        MAYBE(value, varint(static_cast<std::uint32_t>(node->code)));
        body.char_value(value);
        return {};
    }

    expected<ebm::ExpressionBody> make_conditional(ConverterContext& ctx, ebm::TypeRef type, ebm::ExpressionRef cond, ebm::ExpressionRef then, ebm::ExpressionRef els) {
        ebm::ExpressionBody body;
        body.type = type;
        body.kind = ebm::ExpressionOp::CONDITIONAL;
        body.condition(cond);
        body.then(then);
        body.else_(els);

        // lowered
        EBM_DEFAULT_VALUE(default_, body.type);
        EBM_DEFINE_ANONYMOUS_VARIABLE(yielded_value, body.type, default_);
        EBM_IDENTIFIER(temp_var, yielded_value_def, body.type);
        ebm::StatementBody lowered_body;
        lowered_body.kind = ebm::StatementOp::YIELD;
        lowered_body.target(temp_var);
        lowered_body.value(then);
        EBMA_ADD_STATEMENT(yield_then, std::move(lowered_body));
        lowered_body.kind = ebm::StatementOp::YIELD;
        lowered_body.target(temp_var);
        lowered_body.value(els);
        EBMA_ADD_STATEMENT(yield_els, std::move(lowered_body));
        EBM_IF_STATEMENT(cond_stmt, cond, yield_then, yield_els);
        ebm::ExpressionBody lowered_expr;
        lowered_expr.type = body.type;
        lowered_expr.kind = ebm::ExpressionOp::CONDITIONAL_STATEMENT;
        lowered_expr.target_stmt(yielded_value_def);
        lowered_expr.conditional_stmt(cond_stmt);
        EBMA_ADD_EXPR(low_expr, std::move(lowered_expr));
        body.lowered_expr(ebm::LoweredExpressionRef{low_expr});
        return body;
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Cond>& node, ebm::ExpressionBody& body) {
        EBMA_CONVERT_EXPRESSION(cond, node->cond);
        EBMA_CONVERT_EXPRESSION(then, node->then);
        EBMA_CONVERT_EXPRESSION(els, node->els);
        MAYBE(body_, make_conditional(ctx, body.type, cond, then, els));
        body = std::move(body_);

        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::OrCond>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionOp::OR_COND;
        ebm::Expressions conds;
        for (auto& cond : node->conds) {
            EBMA_CONVERT_EXPRESSION(c, cond);
            append(conds, c);
        }
        body.or_cond(std::move(conds));
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body) {
        return unexpect_error("expr not implemented yet: {}", node_type_to_string(node->node_type));
    }

    expected<ebm::ExpressionRef> convert_equal_impl(ConverterContext& ctx, ebm::ExpressionRef a, ebm::ExpressionRef b, ebm::Expression& A, ebm::Expression& B) {
        EBMU_BOOL_TYPE(bool_type);
        if (A.body.kind != B.body.kind) {  // to prevent expression like 0..1 == 1..0
            if (auto start = A.body.start()) {
                auto end = A.body.end();
                if (is_nil(*start) && is_nil(*end)) {
                    ebm::ExpressionBody body;
                    body.kind = ebm::ExpressionOp::LITERAL_BOOL;
                    body.bool_value(1);
                    EBMA_ADD_EXPR(true_, std::move(body));
                    return true_;
                }
                ebm::ExpressionRef ref;
                if (!is_nil(*start)) {
                    EBM_BINARY_OP(greater, ebm::BinaryOp::less_or_eq, bool_type, *start, b);
                    ref = greater;
                }
                if (!is_nil(*end)) {
                    EBM_BINARY_OP(greater, ebm::BinaryOp::less_or_eq, bool_type, b, *end);
                    if (!is_nil(ref)) {
                        EBM_BINARY_OP(and_, ebm::BinaryOp::logical_and, bool_type, ref, greater);
                        ref = and_;
                    }
                    else {
                        ref = greater;
                    }
                }
                return ref;
            }
            else if (auto start = B.body.start()) {
                return convert_equal_impl(ctx, b, a, B, A);
            }
            else if (auto or_cond = A.body.or_cond()) {
                ebm::ExpressionRef cond;
                for (auto& or_ : or_cond->container) {
                    MAYBE(eq, ctx.get_expression_converter().convert_equal(or_, b));
                    if (is_nil(cond)) {
                        cond = eq;
                    }
                    else {
                        EBM_BINARY_OP(ored, ebm::BinaryOp::logical_or, bool_type, cond, eq);
                        cond = ored;
                    }
                }
                return cond;
            }
            else if (auto end = B.body.or_cond()) {
                return convert_equal_impl(ctx, b, a, B, A);
            }
        }
        EBM_BINARY_OP(eq, ebm::BinaryOp::equal, bool_type, a, b);
        return eq;
    }

    expected<ebm::ExpressionRef> ExpressionConverter::convert_equal(ebm::ExpressionRef a, ebm::ExpressionRef b) {
        MAYBE(A, ctx.repository().get_expression(a));
        MAYBE(B, ctx.repository().get_expression(b));
        return convert_equal_impl(ctx, a, b, A, B);
    }

}  // namespace ebmgen
