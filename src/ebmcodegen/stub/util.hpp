/*license*/
#pragma once
#include <string>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/common.hpp>
#include <string_view>
#include "ebmgen/mapping.hpp"

namespace ebmcodegen::util {
    // remove top level brace
    inline std::string tidy_condition_brace(std::string&& brace) {
        if (brace.starts_with("(") && brace.ends_with(")")) {
            // check it is actually a brace
            // for example, we may encounter (1 + 1) + (2 + 2)
            // so ensure that brace is actually (<expr>)
            size_t level = 0;
            for (size_t i = 0; i < brace.size(); ++i) {
                if (brace[i] == '(') {
                    ++level;
                }
                else if (brace[i] == ')') {
                    --level;
                }
                if (level == 0) {
                    if (i == brace.size() - 1) {
                        // the last char is the matching ')'
                        brace = brace.substr(1, brace.size() - 2);
                        return std::move(brace);
                    }
                    break;  // not a top-level brace
                }
            }
        }
        return std::move(brace);
    }

    ebmgen::expected<std::string> get_size_str(auto&& visitor, const ebm::Size& s) {
        if (auto size = s.size()) {
            return std::format("{}", size->value());
        }
        else if (auto ref = s.ref()) {
            MAYBE(expr, visit_Expression(visitor, *ref));
            return expr.to_string();
        }
        return ebmgen::unexpect_error("unsupported size: {}", to_string(s.unit));
    }

    auto as_IDENTIFIER(auto&& visitor, ebm::StatementRef stmt) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::IDENTIFIER;
        expr.body.id(stmt);
        return visit_Expression(visitor, expr);
    }

    auto as_DEFAULT_VALUE(auto&& visitor, ebm::TypeRef typ) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::DEFAULT_VALUE;
        expr.body.type = typ;
        return visit_Expression(visitor, expr);
    }

    struct DefaultValueOption {
        std::string_view object_init = "{}";
        std::string_view vector_init = "[]";
        std::string_view bytes_init;
        std::string_view pointer_init = "nullptr";
        std::string_view optional_init = "std::nullopt";
    };

    ebmgen::expected<std::string> get_default_value(auto&& visitor, ebm::TypeRef ref, const DefaultValueOption& option = {}) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(type, module_.get_type(ref));
        switch (type.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT: {
                ebm::Expression int_literal;
                int_literal.body.kind = ebm::ExpressionKind::LITERAL_INT;
                int_literal.body.int_value(*ebmgen::varint(0));
                MAYBE(val, visit_Expression(visitor, int_literal));
                return val.to_string();
            }
            case ebm::TypeKind::BOOL: {
                ebm::Expression bool_literal;
                bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
                bool_literal.body.bool_value(0);
                MAYBE(val, visit_Expression(visitor, bool_literal));
                return val.to_string();
            }
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::STRUCT:
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                MAYBE(ident, visit_Type(visitor, ref));
                return std::format("{}{}", ident.to_string(), option.object_init);
            }
            case ebm::TypeKind::ARRAY:
            case ebm::TypeKind::VECTOR: {
                MAYBE(elem_type_ref, type.body.element_type());
                MAYBE(elem_type, module_.get_type(elem_type_ref));
                if (elem_type.body.kind == ebm::TypeKind::UINT && elem_type.body.size() && elem_type.body.size()->value() == 8) {
                    if (option.bytes_init.size()) {
                        return std::format("{}", option.bytes_init);
                    }
                    // use default vector init
                }
                return std::format("{}", option.vector_init);
            }
            case ebm::TypeKind::PTR: {
                return std::string(option.pointer_init);
            }
            case ebm::TypeKind::OPTIONAL: {
                return std::string(option.optional_init);
            }
            default: {
                return ebmgen::unexpect_error("unsupported default: {}", to_string(type.body.kind));
            }
        }
    }

    ebmgen::expected<std::string> get_bool_literal(auto&& visitor, bool v) {
        ebm::Expression bool_literal;
        bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
        bool_literal.body.bool_value(v ? 1 : 0);
        MAYBE(expr, visit_Expression(visitor, bool_literal));
        return expr.to_string();
    }

    enum class LayerState {
        root,
        as_type,
        as_expr,
    };

    ebmgen::expected<std::vector<std::pair<ebm::StatementKind, std::string>>> get_identifier_layer(auto&& visitor, ebm::StatementRef stmt, LayerState state = LayerState::root) {
        MAYBE(statement, visitor.module_.get_statement(stmt));
        if (state == LayerState::root) {
            if (statement.body.kind == ebm::StatementKind::STRUCT_DECL ||
                statement.body.kind == ebm::StatementKind::ENUM_DECL) {
                state = LayerState::as_type;
            }
            else {
                state = LayerState::as_expr;
            }
        }
        auto ident = visitor.module_.get_identifier_or(stmt);
        std::vector<std::pair<ebm::StatementKind, std::string>> layers;
        if (const ebm::StructDecl* decl = statement.body.struct_decl()) {
            if (auto related_varint = decl->related_variant()) {
                MAYBE(type, visitor.module_.get_type(*related_varint));
                MAYBE(upper_field, type.body.related_field());
                MAYBE(upper_layers, get_identifier_layer(visitor, upper_field, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
                if (state == LayerState::as_expr) {
                    return layers;
                }
            }
        }
        if (const ebm::FieldDecl* field = statement.body.field_decl()) {
            if (!ebmgen::is_nil(field->parent_struct)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, field->parent_struct, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        if (const ebm::PropertyDecl* prop = statement.body.property_decl()) {
            if (!ebmgen::is_nil(prop->parent_format)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, prop->parent_format, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        layers.emplace_back(statement.body.kind, ident);
        return layers;
    }

    ebmgen::expected<std::vector<std::string>> struct_union_members(auto&& visitor, ebm::TypeRef variant) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<std::string> result;
        if (type.body.kind == ebm::TypeKind::VARIANT) {
            if (!ebmgen::is_nil(*type.body.related_field())) {
                auto& members = *type.body.members();
                for (auto& mem : members.container) {
                    MAYBE(member_type, module_.get_type(mem));
                    MAYBE(stmt_id, member_type.body.id());
                    MAYBE(stmt_str, visit_Statement(visitor, stmt_id));
                    result.push_back(stmt_str.to_string());
                }
            }
        }
        return result;
    }

    std::string join(auto&& joint, auto&& vec) {
        std::string result;
        bool first = true;
        for (auto&& v : vec) {
            if (!first) {
                result += joint;
            }
            result += v;
            first = false;
        }
        return result;
    }
}  // namespace ebmcodegen::util
