#include "../converter.hpp"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"

namespace ebmgen {
    expected<ebm::TypeRef> Converter::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
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
        else if (auto ident_type = ast::as<ast::IdentType>(type)) {
            if (auto locked = ident_type->base.lock()) {
                return convert_type(locked, field);
            }
            return unexpect_error("IdentType has no base type");
        }
        else if (auto array_type = ast::as<ast::ArrayType>(type)) {
            MAYBE(element_type, convert_type(array_type->element_type));
            body.kind = ebm::TypeKind::VECTOR;
            if (array_type->length_value) {
                body.kind = ebm::TypeKind::ARRAY;
                MAYBE(expected_len, varint(*array_type->length_value));
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
        else if (auto int_literal_type = ast::as<ast::IntLiteralType>(type)) {
            body.kind = ebm::TypeKind::UINT;  // Assuming unsigned for now
            if (auto locked_literal = int_literal_type->base.lock()) {
                auto val = locked_literal->parse_as<std::uint64_t>();
                if (!val) {
                    return unexpect_error("Failed to parse IntLiteralType value");
                }
                // Determine bit size based on the value
                // This is a simplified approach; a more robust solution might involve analyzing the original brgen type
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
        else if (auto str_literal_type = ast::as<ast::StrLiteralType>(type)) {
            body.kind = ebm::TypeKind::ARRAY;
            MAYBE(element_type, get_unsigned_n_int(8));
            body.element_type(element_type);
            if (str_literal_type->bit_size) {
                MAYBE(length, varint(*str_literal_type->bit_size / 8));
                body.length(length);
            }
        }
        else if (auto enum_type = ast::as<ast::EnumType>(type)) {
            body.kind = ebm::TypeKind::ENUM;
            if (auto locked_enum = enum_type->base.lock()) {
                auto name_ref = add_identifier(locked_enum->ident->ident);
                if (!name_ref) {
                    return unexpect_error(std::move(name_ref.error()));
                }
                body.id(ebm::StatementRef{name_ref->id});
                if (locked_enum->base_type) {
                    auto base_type_ref = convert_type(locked_enum->base_type);
                    if (!base_type_ref) {
                        return unexpect_error(std::move(base_type_ref.error()));
                    }
                    body.base_type(*base_type_ref);
                }
                else {
                    body.base_type(ebm::TypeRef{});
                }
            }
            else {
                return unexpect_error("EnumType has no base enum");
            }
        }
        else if (auto struct_type = ast::as<ast::StructType>(type)) {
            body.kind = struct_type->recursive ? ebm::TypeKind::RECURSIVE_STRUCT : ebm::TypeKind::STRUCT;
            if (auto locked_base = struct_type->base.lock()) {
                auto name_ref = convert_statement(locked_base);
                if (!name_ref) {
                    return unexpect_error(std::move(name_ref.error()));
                }
                body.id(*name_ref);
            }
            else {
                return unexpect_error("StructType has no base");
            }
        }
        else if (auto struct_union_type = ast::as<ast::StructUnionType>(type)) {
            body.kind = ebm::TypeKind::VARIANT;
            ebm::Types members;
            for (auto& struct_member : struct_union_type->structs) {
                auto member_type_ref = convert_type(struct_member);
                if (!member_type_ref) {
                    return unexpect_error(std::move(member_type_ref.error()));
                }
                append(members, *member_type_ref);
            }
            body.members(std::move(members));
        }
        else if (auto void_type = ast::as<ast::VoidType>(type)) {
            body.kind = ebm::TypeKind::VOID;
        }
        else if (auto meta_type = ast::as<ast::MetaType>(type)) {
            body.kind = ebm::TypeKind::META;
        }
        else if (auto range_type = ast::as<ast::RangeType>(type)) {
            body.kind = ebm::TypeKind::RANGE;
            if (range_type->base_type) {
                auto base_type_ref = convert_type(range_type->base_type);
                if (!base_type_ref) {
                    return unexpect_error(std::move(base_type_ref.error()));
                }
                body.base_type(*base_type_ref);
            }
            else {
                body.base_type(ebm::TypeRef{});
            }
        }
        else if (auto function_type = ast::as<ast::FunctionType>(type)) {
            body.kind = ebm::TypeKind::FUNCTION;
            if (function_type->return_type) {
                MAYBE(return_type, convert_type(function_type->return_type));
                body.return_type(return_type);
            }
            else {
                MAYBE(void_type, get_void_type());
                body.return_type(void_type);
            }

            ebm::Types params;
            for (const auto& param : function_type->parameters) {
                MAYBE(param_type, convert_type(param));
                append(params, param_type);
            }
            body.params(std::move(params));
        }
        else {
            return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
        }
        return add_type(std::move(body));
    }
}  // namespace ebmgen
