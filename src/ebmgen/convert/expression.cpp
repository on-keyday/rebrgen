#include "../converter.hpp"
#include <core/ast/tool/ident.h>
#include "helper.hpp"
#include <fnet/util/base64.h>
#include <core/ast/traverse.h>
#include <type_traits>

namespace ebmgen {
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
                return ebm::BinaryOp::grater;
            case ast::BinaryOp::grater_or_eq:
                return ebm::BinaryOp::grater_or_eq;
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
                return unexpect_error("Unsupported binary operator: {}", to_string(op));
        }
    }

    expected<ebm::UnaryOp> convert_unary_op(ast::UnaryOp op) {
        switch (op) {
            case ast::UnaryOp::not_:
                return ebm::UnaryOp::logical_not;
            case ast::UnaryOp::minus_sign:
                return ebm::UnaryOp::minus_sign;
            default:
                return unexpect_error("Unsupported unary operator");
        }
    }

    expected<ebm::CastType> TypeConverter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = ctx.get_type(dest_ref);
        auto src = ctx.get_type(src_ref);

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
                auto dest_size = *dest->body.size();
                auto src_size = *src->body.size();
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

    expected<ebm::ExpressionRef> ExpressionConverter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        ebm::ExpressionBody body;
        EBMA_CONVERT_TYPE(type_ref, node->expr_type);
        body.type = type_ref;

        expected<void> result = unexpect_error("Unsupported expression type: {}", node_type_to_string(node->node_type));

        brgen::ast::visit(node, [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (std::is_base_of_v<ast::Expr, T>) {
                result = convert_expr_impl(n, body);
            }
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        return ctx.add_expr(std::move(body));
    }

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type) {
        if (alignment_bytes == 0) {
            return unexpect_error("0 is not valid alignment");
        }
        if (alignment_bytes == 1) {
            return ctx.get_int_literal(0);
        }
        ebm::ExpressionBody body;
        EBMU_COUNTER_TYPE(counter_type);
        body.type = counter_type;
        body.op = ebm::ExpressionOp::GET_STREAM_OFFSET;
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
        body.op = ebm::ExpressionOp::LITERAL_INT;
        auto value = node->parse_as<std::uint64_t>();
        if (!value) {
            return unexpect_error("cannot parse int literal");
        }
        body.int_value(*value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::BoolLiteral>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::LITERAL_BOOL;
        body.bool_value(node->value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::StrLiteral>& node, ebm::ExpressionBody& body) {
        MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(node)));
        body.op = ebm::ExpressionOp::LITERAL_STRING;
        EBMA_ADD_STRING(str_ref, candidate);
        body.string_value(str_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::TypeLiteral>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::LITERAL_TYPE;
        EBMA_CONVERT_TYPE(type_ref_inner, node->type_literal)
        body.type_ref(type_ref_inner);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Ident>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::IDENTIFIER;
        auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
        if (!base) {
            return unexpect_error("Identifier {} not found", node->ident);
        }
        EBMA_CONVERT_STATEMENT(id_ref, base->first->base.lock());
        body.id(id_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Binary>& node, ebm::ExpressionBody& body) {
        if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            return unexpect_error("define_assign/const_assign should be handled as a statement, not an expression");
        }
        MAYBE(left_ref, convert_expr(node->left));
        MAYBE(right_ref, convert_expr(node->right));
        MAYBE(bop, convert_binary_op(node->op));
        body = make_binary_op(bop, body.type, left_ref, right_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Unary>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::UNARY_OP;
        MAYBE(operand_ref, convert_expr(node->expr));
        body.operand(operand_ref);
        MAYBE(uop, convert_unary_op(node->op));
        body.uop(uop);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Index>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::INDEX_ACCESS;
        MAYBE(base_ref, convert_expr(node->expr));
        MAYBE(index_ref, convert_expr(node->index));
        body.base(base_ref);
        body.index(index_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::MemberAccess>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::MEMBER_ACCESS;
        MAYBE(base_ref, convert_expr(node->target));
        MAYBE(member_ref, convert_expr(node->member));
        body.base(base_ref);
        body.member(member_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Cast>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::TYPE_CAST;
        MAYBE(source_expr_ref, convert_expr(node->arguments[0]));
        EBMA_CONVERT_TYPE(source_expr_type_ref, node->arguments[0]->expr_type);
        body.source_expr(source_expr_ref);
        body.from_type(source_expr_type_ref);
        MAYBE(cast_kind, ctx.get_type_converter().get_cast_type(body.type, source_expr_type_ref));
        body.cast_kind(cast_kind);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Range>& node, ebm::ExpressionBody& body) {
        body.op = ebm::ExpressionOp::RANGE;
        ebm::ExpressionRef start, end;
        if (node->start) {
            MAYBE(start_ref, convert_expr(node->start));
            start = start_ref;
        }
        if (node->end) {
            MAYBE(end_ref, convert_expr(node->end));
            end = end_ref;
        }
        body.start(start);
        body.end(end);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::IOOperation>& node, ebm::ExpressionBody& body) {
        switch (node->method) {
            case ast::IOMethod::input_get: {
                body.op = ebm::ExpressionOp::READ_DATA;
                MAYBE(target_var_ref, convert_expr(node->arguments[0]));
                body.target_var(target_var_ref);
                MAYBE(data_type_ref, convert_type(node->arguments[0]->expr_type));
                body.data_type(data_type_ref);
                // TODO: Handle endian, bit_size, and fallback_stmt
                break;
            }
            case ast::IOMethod::output_put: {
                body.op = ebm::ExpressionOp::WRITE_DATA;
                MAYBE(source_expr_ref, convert_expr(node->arguments[0]));
                body.source_expr(source_expr_ref);
                MAYBE(data_type_ref, convert_type(node->arguments[0]->expr_type));
                body.data_type(data_type_ref);
                // TODO: Handle endian, bit_size, and fallback_stmt
                break;
            }
            case ast::IOMethod::input_offset:
            case ast::IOMethod::input_bit_offset: {
                body.op = ebm::ExpressionOp::GET_STREAM_OFFSET;
                body.stream_type(ebm::StreamType::INPUT);
                body.unit(node->method == ast::IOMethod::input_bit_offset ? ebm::SizeUnit::BIT_FIXED : ebm::SizeUnit::BYTE_FIXED);
                break;
            }
            case ast::IOMethod::input_remain: {
                body.op = ebm::ExpressionOp::GET_REMAINING_BYTES;
                body.stream_type(ebm::StreamType::INPUT);
                break;
            }
            case ast::IOMethod::input_subrange: {
                body.op = ebm::ExpressionOp::SEEK_STREAM;
                MAYBE(offset_ref, convert_expr(node->arguments[0]));
                body.offset(offset_ref);
                body.stream_type(ebm::StreamType::INPUT);
                break;
            }
            case ast::IOMethod::input_peek: {
                body.op = ebm::ExpressionOp::CAN_READ_STREAM;
                MAYBE(target_var_ref, convert_expr(node->arguments[0]));
                body.target_var(target_var_ref);
                MAYBE(num_bytes_ref, convert_expr(node->arguments[1]));
                body.num_bytes(num_bytes_ref);
                body.stream_type(ebm::StreamType::INPUT);
                break;
            }
            default: {
                return unexpect_error("Unhandled IOMethod: {}", ast::to_string(node->method));
            }
        }
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body) {
        return unexpect_error("not implemented yet: {}", node_type_to_string(node->node_type));
    }

}  // namespace ebmgen
