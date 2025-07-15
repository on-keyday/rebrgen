#include "../converter.hpp"
#include <core/ast/tool/ident.h>
#include "helper.hpp"
#include <fnet/util/base64.h>

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

    expected<ebm::CastType> Converter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = type_repo.get(dest_ref);
        auto src = type_repo.get(src_ref);

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

        if (auto literal = ast::as<ast::IntLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_INT;
            auto value = literal->parse_as<std::uint64_t>();
            if (!value) {
                return unexpect_error("cannot parse int literal");
            }
            body.int_value(*value);
        }
        else if (auto literal = ast::as<ast::BoolLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_BOOL;
            body.bool_value(literal->value);
        }
        else if (auto literal = ast::as<ast::StrLiteral>(node)) {
            MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(node)));
            body.op = ebm::ExpressionOp::LITERAL_STRING;
            EBMA_ADD_STRING(str_ref, candidate);
            body.string_value(str_ref);
        }
        else if (auto literal = ast::as<ast::TypeLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_TYPE;
            EBMA_CONVERT_TYPE(type_ref, literal->type_literal)
            body.type_ref(type_ref);
        }
        else if (auto ident = ast::as<ast::Ident>(node)) {
            body.op = ebm::ExpressionOp::IDENTIFIER;
            auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (!base) {
                return unexpect_error("Identifier {} not found", ident->ident);
            }
            EBMA_CONVERT_STATEMENT(id_ref, base->first->base.lock());
            body.id(id_ref);
        }
        else if (auto binary = ast::as<ast::Binary>(node)) {
            if (binary->op == ast::BinaryOp::define_assign || binary->op == ast::BinaryOp::const_assign) {
                return unexpect_error("define_assign/const_assign should be handled as a statement, not an expression");
            }
            body.op = ebm::ExpressionOp::BINARY_OP;
            auto left_ref = convert_expr(binary->left);
            if (!left_ref) {
                return unexpect_error(std::move(left_ref.error()));
            }
            auto right_ref = convert_expr(binary->right);
            if (!right_ref) {
                return unexpect_error(std::move(right_ref.error()));
            }
            body.left(*left_ref);
            body.right(*right_ref);
            auto bop = convert_binary_op(binary->op);
            if (!bop) {
                return unexpect_error(std::move(bop.error()));
            }
            body.bop(*bop);
        }
        else if (auto unary = ast::as<ast::Unary>(node)) {
            body.op = ebm::ExpressionOp::UNARY_OP;
            auto operand_ref = convert_expr(unary->expr);
            if (!operand_ref) {
                return unexpect_error(std::move(operand_ref.error()));
            }
            body.operand(*operand_ref);
            auto uop = convert_unary_op(unary->op);
            if (!uop) {
                return unexpect_error(std::move(uop.error()));
            }
            body.uop(*uop);
        }
        else if (auto index_expr = ast::as<ast::Index>(node)) {
            body.op = ebm::ExpressionOp::INDEX_ACCESS;
            auto base_ref = convert_expr(index_expr->expr);
            if (!base_ref) {
                return unexpect_error(std::move(base_ref.error()));
            }
            auto index_ref = convert_expr(index_expr->index);
            if (!index_ref) {
                return unexpect_error(std::move(index_ref.error()));
            }
            body.base(*base_ref);
            body.index(*index_ref);
        }
        else if (auto member_access = ast::as<ast::MemberAccess>(node)) {
            body.op = ebm::ExpressionOp::MEMBER_ACCESS;
            MAYBE(base_ref, convert_expr(member_access->target));
            MAYBE(member_ref, convert_expr(member_access->member));
            body.base(base_ref);
            body.member(member_ref);
        }
        else if (auto cast_expr = ast::as<ast::Cast>(node)) {
            body.op = ebm::ExpressionOp::TYPE_CAST;
            MAYBE(source_expr_ref, convert_expr(cast_expr->arguments[0]));
            MAYBE(source_expr_type_ref, convert_type(cast_expr->arguments[0]->expr_type));
            body.source_expr(source_expr_ref);
            body.from_type(source_expr_type_ref);
            auto cast_kind = get_cast_type(body.type, source_expr_type_ref);
            if (!cast_kind) {
                return unexpect_error(std::move(cast_kind.error()));
            }
            body.cast_kind(*cast_kind);
        }
        else if (auto range = ast::as<ast::Range>(node)) {
            body.op = ebm::ExpressionOp::RANGE;
            ebm::ExpressionRef start, end;
            if (range->start) {
                auto start_ref = convert_expr(range->start);
                if (!start_ref) {
                    return unexpect_error(std::move(start_ref.error()));
                }
                start = *start_ref;
            }
            if (range->end) {
                auto end_ref = convert_expr(range->end);
                if (!end_ref) {
                    return unexpect_error(std::move(end_ref.error()));
                }
                end = *end_ref;
            }
            body.start(start);
            body.end(end);
        }
        else if (auto io_op_stmt = ast::as<ast::IOOperation>(node)) {
            switch (io_op_stmt->method) {
                case ast::IOMethod::input_get: {
                    body.op = ebm::ExpressionOp::READ_DATA;
                    auto target_var_ref = convert_expr(io_op_stmt->arguments[0]);
                    if (!target_var_ref) {
                        return unexpect_error(std::move(target_var_ref.error()));
                    }
                    body.target_var(*target_var_ref);
                    auto data_type_ref = convert_type(io_op_stmt->arguments[0]->expr_type);
                    if (!data_type_ref) {
                        return unexpect_error(std::move(data_type_ref.error()));
                    }
                    body.data_type(*data_type_ref);
                    // TODO: Handle endian, bit_size, and fallback_stmt
                    break;
                }
                case ast::IOMethod::output_put: {
                    body.statement_kind = ebm::StatementOp::WRITE_DATA;
                    auto source_expr_ref = convert_expr(io_op_stmt->arguments[0]);
                    if (!source_expr_ref) {
                        return unexpect_error(std::move(source_expr_ref.error()));
                    }
                    body.source_expr(*source_expr_ref);
                    auto data_type_ref = convert_type(io_op_stmt->arguments[0]->expr_type);
                    if (!data_type_ref) {
                        return unexpect_error(std::move(data_type_ref.error()));
                    }
                    body.data_type(*data_type_ref);
                    // TODO: Handle endian, bit_size, and fallback_stmt
                    break;
                }
                case ast::IOMethod::input_offset:
                case ast::IOMethod::input_bit_offset: {
                    body.op = ebm::ExpressionOp::GET_STREAM_OFFSET;
                    body.stream_type(ebm::StreamType::INPUT);
                    body.unit(io_op_stmt->method == ast::IOMethod::input_bit_offset ? ebm::SizeUnit::BIT_FIXED : ebm::SizeUnit::BYTE_FIXED);
                    break;
                }
                case ast::IOMethod::input_remain: {
                    body.op = ebm::ExpressionOp::GET_REMAINING_BYTES;
                    body.stream_type(ebm::StreamType::INPUT);
                    break;
                }
                case ast::IOMethod::input_subrange: {  // Assuming input_subrange maps to SEEK_STREAM
                    body.statement_kind = ebm::StatementOp::SEEK_STREAM;
                    auto offset_ref = convert_expr(io_op_stmt->arguments[0]);
                    if (!offset_ref) {
                        return unexpect_error(std::move(offset_ref.error()));
                    }
                    body.offset(*offset_ref);
                    body.stream_type(ebm::StreamType::INPUT);
                    break;
                }
                case ast::IOMethod::input_peek: {  // Assuming input_peek maps to CAN_READ_STREAM
                    body.statement_kind = ebm::StatementOp::CAN_READ_STREAM;
                    auto target_var_ref = convert_expr(io_op_stmt->arguments[0]);
                    if (!target_var_ref) {
                        return unexpect_error(std::move(target_var_ref.error()));
                    }
                    body.target_var(*target_var_ref);
                    auto num_bytes_ref = convert_expr(io_op_stmt->arguments[1]);
                    if (!num_bytes_ref) {
                        return unexpect_error(std::move(num_bytes_ref.error()));
                    }
                    body.num_bytes(*num_bytes_ref);
                    body.stream_type(ebm::StreamType::INPUT);
                    break;
                }
                default: {
                    return unexpect_error("Unhandled IOMethod: {}", ast::to_string(io_op_stmt->method));
                }
            }
        }
        else {
            return unexpect_error("not implemented yet: {}", node_type_to_string(node->node_type));
        }
        return add_expr(std::move(body));
    }

    expected<ebm::ExpressionRef> ExpressionConverter::get_alignment_requirement(std::uint64_t alignment_bytes, ebm::StreamType type) {
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
}  // namespace ebmgen
