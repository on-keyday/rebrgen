#include "../converter.hpp"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {
    expected<ebm::TypeBody> TypeConverter::convert_function_type(const std::shared_ptr<ast::FunctionType>& n) {
        ebm::TypeBody body;
        body.kind = ebm::TypeKind::FUNCTION;
        if (n->return_type) {
            EBMA_CONVERT_TYPE(return_type, n->return_type);
            body.return_type(return_type);
        }
        else {
            EBMU_VOID_TYPE(void_type);
            body.return_type(void_type);
        }

        ebm::Types params;
        for (const auto& param : n->parameters) {
            EBMA_CONVERT_TYPE(param_type, param);
            append(params, param_type);
        }
        body.params(std::move(params));
        return body;
    }

    expected<ebm::TypeRef> TypeConverter::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
        ebm::TypeBody body;
        expected<void> result = {};  // To capture errors from within the lambda

        auto fn = [&](auto&& n) -> expected<void> {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<ast::IntType>>) {
                if (n->is_signed) {
                    body.kind = ebm::TypeKind::INT;
                }
                else {
                    body.kind = ebm::TypeKind::UINT;
                }
                if (!n->bit_size) {
                    return unexpect_error("IntType must have a bit_size");
                }
                body.size(*n->bit_size);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::BoolType>>) {
                body.kind = ebm::TypeKind::BOOL;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::FloatType>>) {
                body.kind = ebm::TypeKind::FLOAT;
                if (!n->bit_size) {
                    return unexpect_error("FloatType must have a bit_size");
                }
                body.size(*n->bit_size);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::IdentType>>) {
                if (auto locked = n->base.lock()) {
                    EBMA_CONVERT_TYPE(converted_type, locked, field);
                    body = ctx.repository().get_type(converted_type)->body;  // Copy the body from the converted type
                }
                else {
                    return unexpect_error("IdentType has no base type");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::ArrayType>>) {
                EBMA_CONVERT_TYPE(element_type, n->element_type);
                body.kind = ebm::TypeKind::VECTOR;
                if (n->length_value) {
                    body.kind = ebm::TypeKind::ARRAY;
                    MAYBE(expected_len, varint(*n->length_value));
                    body.length(expected_len);
                }
                else if (is_alignment_vector(field)) {
                    auto vector_len = *field->arguments->alignment_value / 8 - 1;
                    body.kind = ebm::TypeKind::ARRAY;
                    MAYBE(expected_len, varint(vector_len));
                    body.length(expected_len);
                }
                body.element_type(element_type);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::IntLiteralType>>) {
                body.kind = ebm::TypeKind::UINT;  // Assuming unsigned for now
                if (auto locked_literal = n->base.lock()) {
                    auto val = locked_literal->template parse_as<std::uint64_t>();
                    if (!val) {
                        return unexpect_error("Failed to parse IntLiteralType value");
                    }
                    // Determine bit size based on the value
                    if (*val <= 0xFF) {  // 8-bit
                        body.size(8);
                    }
                    else if (*val <= 0xFFFF) {  // 16-bit
                        body.size(16);
                    }
                    else if (*val <= 0xFFFFFFFF) {  // 32-bit
                        body.size(32);
                    }
                    else {  // 64-bit
                        body.size(64);
                    }
                }
                else {
                    return unexpect_error("IntLiteralType has no base literal");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StrLiteralType>>) {
                body.kind = ebm::TypeKind::ARRAY;
                EBMU_U8(element_type);
                body.element_type(element_type);
                if (n->bit_size) {
                    MAYBE(length, varint(*n->bit_size / 8));
                    body.length(length);
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::EnumType>>) {
                body.kind = ebm::TypeKind::ENUM;
                if (auto locked_enum = n->base.lock()) {
                    const auto _mode = ctx.state().set_current_generate_type(GenerateType::Normal);
                    EBMA_CONVERT_STATEMENT(stmt_ref, locked_enum);  // Convert the enum declaration
                    body.id(stmt_ref);                              // Use the ID of the enum declaration
                    if (locked_enum->base_type) {
                        EBMA_CONVERT_TYPE(base_type_ref, locked_enum->base_type);
                        body.base_type(base_type_ref);
                    }
                }
                else {
                    return unexpect_error("EnumType has no base enum");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StructType>>) {
                const auto _mode = ctx.state().set_current_generate_type(GenerateType::Normal);
                body.kind = n->recursive ? ebm::TypeKind::RECURSIVE_STRUCT : ebm::TypeKind::STRUCT;
                if (auto locked_base = n->base.lock()) {
                    if (ast::as<ast::Format>(locked_base) || ast::as<ast::State>(locked_base)) {
                        EBMA_CONVERT_STATEMENT(name_ref, locked_base);  // Convert the struct declaration
                        body.id(name_ref);                              // Use the ID of the struct declaration
                    }
                    else if (ast::as<ast::MatchBranch>(locked_base) || ast::as<ast::If>(locked_base)) {
                        ebm::StatementBody stmt;
                        stmt.statement_kind = ebm::StatementOp::STRUCT_DECL;
                        MAYBE(struct_decl, ctx.get_statement_converter().convert_struct_decl({}, n));
                        stmt.struct_decl(std::move(struct_decl));
                        EBMA_ADD_STATEMENT(name_ref, std::move(stmt));
                        body.id(name_ref);  // Use the ID of the struct declaration
                    }
                    else {
                        return unexpect_error("StructType base must be a Format or State :{}", node_type_to_string(locked_base->node_type));
                    }
                }
                else {
                    return unexpect_error("StructType has no base");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StructUnionType>>) {
                body.kind = ebm::TypeKind::VARIANT;
                ebm::Types members;
                for (auto& struct_member : n->structs) {
                    EBMA_CONVERT_TYPE(member_type_ref, struct_member);
                    append(members, member_type_ref);
                }
                body.members(std::move(members));
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::VoidType>>) {
                body.kind = ebm::TypeKind::VOID;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::MetaType>>) {
                body.kind = ebm::TypeKind::META;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::RangeType>>) {
                body.kind = ebm::TypeKind::RANGE;
                if (n->base_type) {
                    EBMA_CONVERT_TYPE(base_type_ref, n->base_type);
                    body.base_type(base_type_ref);
                }
                else {
                    body.base_type(ebm::TypeRef{});
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::FunctionType>>) {
                MAYBE(body_, convert_function_type(n));
                body = std::move(body_);
            }
            else {
                return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
            }
            return {};  // Success
        };

        brgen::ast::visit(ast::cast_to<ast::Node>(type), [&](auto&& n) {
            result = fn(std::forward<decltype(n)>(n));
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        EBMA_ADD_TYPE(ref, std::move(body));
        return ref;  // Return the type reference
    }

}  // namespace ebmgen
