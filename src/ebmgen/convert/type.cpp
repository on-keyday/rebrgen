#include "../converter.hpp"

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
        else if (auto ident_type = ast::as<ast::IdentType>(type)) {
            if (auto locked = ident_type->base.lock()) {
                return convert_type(locked);
            }
            return unexpect_error("IdentType has no base type");
        }
        else if (auto array_type = ast::as<ast::ArrayType>(type)) {
            body.kind = ebm::TypeKind::ARRAY;
            auto element_type_ref = convert_type(array_type->element_type);
            if (!element_type_ref) {
                return unexpect_error(std::move(element_type_ref.error()));
            }
            body.element_type(*element_type_ref);
            if (array_type->length) {
                auto length_expr_ref = convert_expr(array_type->length);
                if (!length_expr_ref) {
                    return unexpect_error(std::move(length_expr_ref.error()));
                }
                // TODO: Convert ExpressionRef to Varint for length
                // For now, just setting a dummy Varint
                body.length(ebm::Varint{0});
            }
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
            auto element_type_ref = convert_type(std::make_shared<ast::IntType>(str_literal_type->loc, 8, ast::Endian::unspec, false));
            if (!element_type_ref) {
                return unexpect_error(std::move(element_type_ref.error()));
            }
            body.element_type(*element_type_ref);
            if (str_literal_type->bit_size) {
                auto length = varint(*str_literal_type->bit_size / 8);
                if (!length) {
                    return unexpect_error(std::move(length.error()));
                }
                body.length(*length);
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
            // StructUnionType does not have a direct common_type field in AST, but its structs might imply one.
            // For now, we'll just convert the types of its constituent structs.
            ebm::Types members;
            for (auto& struct_member : struct_union_type->structs) {
                auto member_type_ref = convert_type(std::static_pointer_cast<ast::Type>(struct_member));
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
        else {
            return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
        }
        return add_type(std::move(body));
    }
}  // namespace ebmgen
