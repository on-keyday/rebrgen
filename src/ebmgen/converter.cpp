#include "converter.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {

    expected<ebm::TypeRef> Converter::convert_type(const std::shared_ptr<ast::Type>& type) {
        ebm::TypeBody body;
        if (auto int_type = ast::as<ast::IntType>(type)) {
            if (int_type->is_signed) {
                body.kind = ebm::TypeKind::INT;
            }
            else {
                body.kind = ebm::TypeKind::UINT;
            }
            if (!int_type->bit_size) {
                return unexpect_error("IntType must have a bit_size");
            }
            body.size(*int_type->bit_size);
        }
        else if (auto bool_type = ast::as<ast::BoolType>(type)) {
            body.kind = ebm::TypeKind::BOOL;
        }
        else if (auto float_type = ast::as<ast::FloatType>(type)) {
            body.kind = ebm::TypeKind::FLOAT;
            if (!float_type->bit_size) {
                return unexpect_error("FloatType must have a bit_size");
            }
            body.size(*float_type->bit_size);
        }
        else {
            return unexpect_error("Unsupported type for conversion");
        }
        return add_type(std::move(body));
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
                return unexpect_error("Unsupported binary operator");
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

    ebm::Identifier* Converter::get_identifier(ebm::IdentifierRef ref) {
        if (ref.id.value() == 0 || identifier_map.find(ref.id.value()) == identifier_map.end()) {
            return nullptr;
        }
        return &ebm.identifiers[identifier_map[ref.id.value()]];
    }

    ebm::StringLiteral* Converter::get_string(ebm::StringRef ref) {
        if (ref.id.value() == 0 || string_map.find(ref.id.value()) == string_map.end()) {
            return nullptr;
        }
        return &ebm.strings[string_map[ref.id.value()]];
    }

    ebm::Type* Converter::get_type(ebm::TypeRef ref) {
        if (ref.id.value() == 0 || type_map.find(ref.id.value()) == type_map.end()) {
            return nullptr;
        }
        return &ebm.types[type_map[ref.id.value()]];
    }

    ebm::Expression* Converter::get_expression(ebm::ExpressionRef ref) {
        if (ref.id.value() == 0 || expression_map.find(ref.id.value()) == expression_map.end()) {
            return nullptr;
        }
        return &ebm.expressions[expression_map[ref.id.value()]];
    }

    ebm::Statement* Converter::get_statement(ebm::StatementRef ref) {
        if (ref.id.value() == 0 || statement_map.find(ref.id.value()) == statement_map.end()) {
            return nullptr;
        }
        return &ebm.statements[statement_map[ref.id.value()]];
    }

    expected<ebm::CastType> Converter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = get_type(dest_ref);
        auto src = get_type(src_ref);

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

    void Converter::convert_node(const std::shared_ptr<ast::Node>& node) {
        if (err) return;
        if (auto expr = ast::as<ast::Expr>(node)) {
            auto ref = convert_expr(ast::cast_to<ast::Expr>(node));
            if (!ref) {
                err = ref.error();
            }
        } else if (auto stmt = ast::as<ast::Stmt>(node)) {
            auto ref = convert_statement(ast::cast_to<ast::Stmt>(node));
            if (!ref) {
                err = ref.error();
            }
        }
    }

    expected<ebm::StatementRef> Converter::convert_statement(const std::shared_ptr<ast::Stmt>& node) {
        ebm::StatementBody body;
        if (auto assert_stmt = ast::as<ast::Assert>(node)) {
            body.statement_kind = ebm::StatementOp::ASSERT;
            auto cond_ref = convert_expr(assert_stmt->cond);
            if (!cond_ref) {
                return unexpect_error(std::move(cond_ref.error()));
            }
            body.condition(*cond_ref);
            // TODO: Handle assert message
            return add_statement(std::move(body));
        }
        // TODO: Implement conversion for different statement types
        return unexpect_error("Statement conversion not implemented yet");
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
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::BoolLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_BOOL;
            body.bool_value(literal->value);
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::StrLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_STRING;
            auto str_ref = add_string(literal->value);
            if (!str_ref) {
                return unexpect_error(std::move(str_ref.error()));
            }
            body.string_value(*str_ref);
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::TypeLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_TYPE;
            auto type_ref = convert_type(literal->type_literal);
            if (!type_ref) {
                return unexpect_error(std::move(type_ref.error()));
            }
            body.type_ref(*type_ref);
            return add_expr(std::move(body));
        }
        else if (auto ident = ast::as<ast::Ident>(node)) {
            body.op = ebm::ExpressionOp::IDENTIFIER;
            auto id_ref = add_identifier(ident->ident);
            if (!id_ref) {
                return unexpect_error(std::move(id_ref.error()));
            }
            body.id(*id_ref);
            return add_expr(std::move(body));
        }
        else if (auto binary = ast::as<ast::Binary>(node)) {
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
            return add_expr(std::move(body));
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
            return add_expr(std::move(body));
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
            return add_expr(std::move(body));
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
            return add_expr(std::move(body));
        }
        else if (auto cast_expr = ast::as<ast::Cast>(node)) {
            body.op = ebm::ExpressionOp::TYPE_CAST;
            auto target_type_ref = convert_type(cast_expr->expr_type);
            if (!target_type_ref) {
                return unexpect_error(std::move(target_type_ref.error()));
            }
            auto source_expr_ref = convert_expr(cast_expr->arguments[0]);
            if (!source_expr_ref) {
                return unexpect_error(std::move(source_expr_ref.error()));
            }
            body.target_type(*target_type_ref);
            auto source_expr_type_ref = convert_type(cast_expr->arguments[0]->expr_type);
            if (!source_expr_type_ref) {
                return unexpect_error(std::move(source_expr_type_ref.error()));
            }
            body.source_expr(*source_expr_ref);
            auto cast_kind = get_cast_type(*target_type_ref, *source_expr_type_ref);
            if (!cast_kind) {
                return unexpect_error(std::move(cast_kind.error()));
            }
            body.cast_kind(*cast_kind);
            return add_expr(std::move(body));
        }
        else {
            return unexpect_error("not implemented yet");
        }
    }

    Error Converter::convert(const std::shared_ptr<brgen::ast::Node>& ast_root) {
        root = ast_root;
        ast::traverse(root, [&](auto& node) {
            convert_node(node);
        });
        return err;
    }

}  // namespace ebmgen
