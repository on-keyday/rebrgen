#include "../converter.hpp"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {
    expected<ebm::TypeRef> TypeConverter::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
        ebm::TypeBody body;
        expected<void> result = {};  // To capture errors from within the lambda

        brgen::ast::visit(type, [&](auto&& n) -> expected<void> {
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
                    MAYBE(converted_type, convert_type(locked, field));
                    body = ctx.get_type(converted_type)->body;  // Copy the body from the converted type
                }
                else {
                    return unexpect_error("IdentType has no base type");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::ArrayType>>) {
                MAYBE(element_type, convert_type(n->element_type));
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
                    auto val = locked_literal->parse_as<std::uint64_t>();
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
                MAYBE(element_type, ctx.get_unsigned_n_int(8));
                body.element_type(element_type);
                if (n->bit_size) {
                    MAYBE(length, varint(*n->bit_size / 8));
                    body.length(length);
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::EnumType>>) {
                body.kind = ebm::TypeKind::ENUM;
                if (auto locked_enum = n->base.lock()) {
                    MAYBE(stmt_ref, ctx.convert_statement(locked_enum));  // Convert the enum declaration
                    body.id(stmt_ref);                                    // Use the ID of the enum declaration
                    if (locked_enum->base_type) {
                        MAYBE(base_type_ref, convert_type(locked_enum->base_type));
                        body.base_type(base_type_ref);
                    }
                    else {
                        body.base_type(ebm::TypeRef{});
                    }
                }
                else {
                    return unexpect_error("EnumType has no base enum");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StructType>>) {
                body.kind = n->recursive ? ebm::TypeKind::RECURSIVE_STRUCT : ebm::TypeKind::STRUCT;
                if (auto locked_base = n->base.lock()) {
                    MAYBE(name_ref, ctx.convert_statement(locked_base));  // Convert the struct declaration
                    body.id(name_ref);                                    // Use the ID of the struct declaration
                }
                else {
                    return unexpect_error("StructType has no base");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StructUnionType>>) {
                body.kind = ebm::TypeKind::VARIANT;
                ebm::Types members;
                for (auto& struct_member : n->structs) {
                    MAYBE(member_type_ref, convert_type(struct_member));
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
                    MAYBE(base_type_ref, convert_type(n->base_type));
                    body.base_type(base_type_ref);
                }
                else {
                    body.base_type(ebm::TypeRef{});
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::FunctionType>>) {
                body.kind = ebm::TypeKind::FUNCTION;
                if (n->return_type) {
                    MAYBE(return_type, convert_type(n->return_type));
                    body.return_type(return_type);
                }
                else {
                    MAYBE(void_type, ctx.get_void_type());
                    body.return_type(void_type);
                }

                ebm::Types params;
                for (const auto& param : n->parameters) {
                    MAYBE(param_type, convert_type(param));
                    append(params, param_type);
                }
                body.params(std::move(params));
            }
            else {
                return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
            }
            return {};  // Success
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        return ctx.add_type(std::move(body));
    }

    expected<ebm::TypeRef> ConverterContext::get_unsigned_n_int(size_t n) {
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::UINT;
        typ.size(n);
        auto utyp = type_repo.add(ident_source, std::move(typ));
        if (!utyp) {
            return unexpect_error(std::move(utyp.error()));
        }
        return utyp;
    }

    expected<ebm::TypeRef> ConverterContext::get_counter_type() {
        return get_unsigned_n_int(64);
    }

    expected<ebm::ExpressionRef> ConverterContext::get_int_literal(std::uint64_t value) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::LITERAL_INT;
        body.int_value(value);
        auto int_literal = add_expr(std::move(body));
        if (!int_literal) {
            return unexpect_error(std::move(int_literal.error()));
        }
        return *int_literal;
    }

    expected<ebm::TypeRef> get_single_type(ebm::TypeKind kind, auto& type_repo, IdentifierSource& ident_source) {
        ebm::TypeBody typ;
        typ.kind = kind;
        return type_repo.add(ident_source, std::move(typ));
    }

    expected<ebm::TypeRef> ConverterContext::get_bool_type() {
        return get_single_type(ebm::TypeKind::BOOL, type_repo, ident_source);
    }

    expected<ebm::TypeRef> ConverterContext::get_void_type() {
        return get_single_type(ebm::TypeKind::VOID, type_repo, ident_source);
    }

    expected<ebm::TypeRef> ConverterContext::get_encoder_return_type() {
        return get_single_type(ebm::TypeKind::ENCODER_RETURN, type_repo, ident_source);
    }
    expected<ebm::TypeRef> ConverterContext::get_decoder_return_type() {
        return get_single_type(ebm::TypeKind::DECODER_RETURN, type_repo, ident_source);
    }

    expected<ebm::TypeRef> ConverterContext::get_u8_n_array(size_t n) {
        auto u8typ = get_unsigned_n_int(8);
        if (!u8typ) {
            return unexpect_error(std::move(u8typ.error()));
        }
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::ARRAY;
        typ.element_type(*u8typ);
        typ.length(*varint(n));
        return type_repo.add(ident_source, std::move(typ));
    }

}  // namespace ebmgen
