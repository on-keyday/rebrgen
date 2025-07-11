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

    expected<ebm::ExpressionRef> Converter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        ebm::ExpressionBody body;
        auto type_ref = convert_type(node->expr_type);
        if (!type_ref) {
            return unexpect_error(std::move(type_ref.error()));
        }
        body.type = *type_ref;

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
            std::string candidate;
            if (!futils::base64::decode(literal->binary_value, candidate)) {
                return unexpect_error("Invalid base64 string: {}", literal->binary_value);
            }
            body.op = ebm::ExpressionOp::LITERAL_STRING;
            MAYBE(str_ref, add_string(literal->value));
            body.string_value(str_ref);
        }
        else if (auto literal = ast::as<ast::TypeLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_TYPE;
            MAYBE(type_ref, convert_type(literal->type_literal));
            body.type_ref(type_ref);
        }
        else if (auto ident = ast::as<ast::Ident>(node)) {
            body.op = ebm::ExpressionOp::IDENTIFIER;
            auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (!base) {
                return unexpect_error("Identifier {} not found", ident->ident);
            }
            MAYBE(id_ref, convert_statement(base->first->base.lock()));
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
            auto base_ref = convert_expr(member_access->target);
            if (!base_ref) {
                return unexpect_error(std::move(base_ref.error()));
            }
            auto member_ref = add_identifier(member_access->member->ident);
            if (!member_ref) {
                return unexpect_error(std::move(member_ref.error()));
            }
            body.base(*base_ref);
            body.member(*member_ref);
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
        else {
            return unexpect_error("not implemented yet: {}", node_type_to_string(node->node_type));
        }
        return add_expr(std::move(body));
    }
}  // namespace ebmgen
