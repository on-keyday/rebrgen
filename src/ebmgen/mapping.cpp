/*license*/
#include "mapping.hpp"
#include "common.hpp"
#include "ebm/extended_binary_module.hpp"

namespace ebmgen {
    bool verbose_error;
    // Builds maps from vector data for faster access
    void MappingTable::build_maps() {
        auto map_to = [&](auto& map, const auto& vec, ebm::AliasHint hint) {
            for (const auto& item : vec) {
                map[get_id(item.id)] = &item;
                item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                    if constexpr (AnyRef<decltype(val)>) {
                        if (!is_nil(val)) {
                            inverse_refs_[get_id(val)].push_back(InverseRef{
                                .name = name,
                                .index = index,
                                .ref = to_any_ref(item.id),
                                .hint = hint,
                            });
                        }
                    }
                    else if constexpr (is_container<decltype(val)>) {
                        for (size_t i = 0; i < val.container.size(); ++i) {
                            visitor(visitor, name, val.container[i], i);
                        }
                    }
                    else
                        VISITOR_RECURSE(visitor, name, val)
                });
            }
        };
        map_to(identifier_map_, module_.identifiers, ebm::AliasHint::IDENTIFIER);
        map_to(string_literal_map_, module_.strings, ebm::AliasHint::STRING);
        map_to(type_map_, module_.types, ebm::AliasHint::TYPE);
        map_to(statement_map_, module_.statements, ebm::AliasHint::STATEMENT);
        map_to(expression_map_, module_.expressions, ebm::AliasHint::EXPRESSION);

        auto map_alias = [&](auto& map, const auto& alias) {
            map[get_id(alias.from)] = map[get_id(alias.to)];
            inverse_refs_[get_id(alias.to)].push_back(InverseRef{
                .name = to_string(alias.hint),
                .ref = to_any_ref(alias.from),
                .hint = ebm::AliasHint::ALIAS,
            });
        };

        for (const auto& alias : module_.aliases) {
            switch (alias.hint) {
                case ebm::AliasHint::IDENTIFIER:
                    map_alias(identifier_map_, alias);
                    break;
                case ebm::AliasHint::STRING:
                    map_alias(string_literal_map_, alias);
                    break;
                case ebm::AliasHint::TYPE:
                    map_alias(type_map_, alias);
                    break;
                case ebm::AliasHint::EXPRESSION:
                    map_alias(expression_map_, alias);
                    break;
                case ebm::AliasHint::STATEMENT:
                    map_alias(statement_map_, alias);
                    break;
                case ebm::AliasHint::ALIAS:
                    // ALIAS hint is not used for mapping, it's just a marker
                    break;
            }
        }
        for (const auto& debug_loc : module_.debug_info.locs) {
            debug_loc_map_[get_id(debug_loc.ident)] = &debug_loc;
        }
    }

    // --- Helper functions to get objects from references ---
    const ebm::Identifier* MappingTable::get_identifier(const ebm::IdentifierRef& ref) const {
        auto it = identifier_map_.find(get_id(ref));
        return (it != identifier_map_.end()) ? it->second : nullptr;
    }

    const ebm::StringLiteral* MappingTable::get_string_literal(const ebm::StringRef& ref) const {
        auto it = string_literal_map_.find(get_id(ref));
        return (it != string_literal_map_.end()) ? it->second : nullptr;
    }

    const ebm::Type* MappingTable::get_type(const ebm::TypeRef& ref) const {
        auto it = type_map_.find(get_id(ref));
        return (it != type_map_.end()) ? it->second : nullptr;
    }

    const ebm::Statement* MappingTable::get_statement(const ebm::StatementRef& ref) const {
        auto it = statement_map_.find(get_id(ref));
        return (it != statement_map_.end()) ? it->second : nullptr;
    }

    const ebm::Expression* MappingTable::get_expression(const ebm::ExpressionRef& ref) const {
        auto it = expression_map_.find(get_id(ref));
        return (it != expression_map_.end()) ? it->second : nullptr;
    }

    std::variant<std::monostate, const ebm::Identifier*, const ebm::StringLiteral*, const ebm::Type*, const ebm::Statement*, const ebm::Expression*> MappingTable::get_object(const ebm::AnyRef& ref) const {
        if (auto i = get_identifier(ebm::IdentifierRef{ref.id})) {
            return i;
        }
        if (auto s = get_string_literal(ebm::StringRef{ref.id})) {
            return s;
        }
        if (auto t = get_type(ebm::TypeRef{ref.id})) {
            return t;
        }
        if (auto s = get_statement(ebm::StatementRef{ref.id})) {
            return s;
        }
        if (auto e = get_expression(ebm::ExpressionRef{ref.id})) {
            return e;
        }
        return std::monostate{};
    }

    const std::vector<InverseRef>* MappingTable::get_inverse_ref(const ebm::AnyRef& ref) const {
        auto it = inverse_refs_.find(get_id(ref));
        if (it != inverse_refs_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const ebm::Statement* MappingTable::get_entry_point() const {
        return get_statement(ebm::StatementRef{module_.max_id.id});
    }

    const ebm::Identifier* MappingTable::get_identifier(const ebm::StatementRef& ref) const {
        auto stmt = get_statement(ref);
        if (!stmt) {
            return nullptr;
        }
        ebm::IdentifierRef ident;
        stmt->body.visit([&](auto&& visitor, std::string_view name, auto&& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ebm::IdentifierRef>) {
                // if (name == "name") {
                ident = value;
                //}
            }
            else
                VISITOR_RECURSE(visitor, name, value)
        });
        return get_identifier(ident);
    }

    std::string_view MappingTable::get_default_prefix(ebm::StatementRef ref) const {
        if (auto stmt = get_statement(ref)) {
            return get_default_prefix(stmt->body.kind);
        }
        return "tmp";
    }

    std::string MappingTable::get_identifier_or(const ebm::StatementRef& ref, std::string_view prefix) const {
        if (auto it = statement_identifier_direct_map_.find(get_id(ref)); it != statement_identifier_direct_map_.end()) {
            return it->second;
        }
        if (const ebm::Identifier* id = get_identifier(ref)) {
            return id->body.data;
        }
        if (prefix.empty()) {
            prefix = get_default_prefix(ref);
        }
        return std::format("{}{}", prefix, get_id(ref));
    }

    expected<std::string> MappingTable::get_identifier_or(const ebm::ExpressionRef& ref, std::string_view prefix) const {
        auto expr = get_expression(ref);
        if (!expr) {
            return unexpect_error("Invalid expression reference: {}", get_id(ref));
        }
        if (auto id = expr->body.id()) {
            return get_identifier_or(*id, prefix);
        }
        return unexpect_error("Expression does not have an associated identifier: {}", get_id(ref));
    }

    expected<std::string> MappingTable::get_identifier_or(const ebm::TypeRef& ref, std::string_view prefix) const {
        auto type = get_type(ref);
        if (!type) {
            return unexpect_error("Invalid type reference: {}", get_id(ref));
        }
        if (auto id = type->body.id()) {
            return get_identifier_or(*id, prefix);
        }
        return unexpect_error("Type does not have an associated identifier: {}", get_id(ref));
    }

    const ebm::Identifier* MappingTable::get_identifier(const ebm::ExpressionRef& ref) const {
        if (auto expr = get_expression(ref); expr && expr->body.id()) {
            return get_identifier(*expr->body.id());
        }
        return nullptr;
    }

    const ebm::Identifier* MappingTable::get_identifier(const ebm::TypeRef& ref) const {
        if (auto type = get_type(ref); type && type->body.id()) {
            return get_identifier(*type->body.id());
        }
        return nullptr;
    }

    const ebm::Identifier* MappingTable::get_identifier(const ebm::AnyRef& ref) const {
        auto obj = get_object(ref);
        return std::visit(
            [&](auto&& obj) -> const ebm::Identifier* {
                using T = std::decay_t<decltype(obj)>;
                if constexpr (std::is_same_v<T, const ebm::Identifier*>) {
                    return obj;
                }
                else if constexpr (std::is_same_v<T, const ebm::Type*>) {
                    if (obj->body.id()) {
                        return get_identifier(*obj->body.id());
                    }
                }
                else if constexpr (std::is_same_v<T, const ebm::Statement*>) {
                    return get_identifier(ebm::StatementRef{obj->id.id});
                }
                else if constexpr (std::is_same_v<T, const ebm::Expression*>) {
                    if (obj->body.id()) {
                        return get_identifier(*obj->body.id());
                    }
                }
                return nullptr;
            },
            obj);
    }

    bool MappingTable::valid() const {
        auto original_id_count = module_.identifiers.size() +
                                 module_.strings.size() +
                                 module_.types.size() +
                                 module_.statements.size() +
                                 module_.expressions.size() +
                                 module_.aliases.size();
        auto mapped_id_count = identifier_map_.size() +
                               string_literal_map_.size() +
                               type_map_.size() +
                               statement_map_.size() +
                               expression_map_.size();
        return original_id_count == mapped_id_count;
    }

    const ebm::Loc* MappingTable::get_debug_loc(const ebm::AnyRef& ref) const {
        auto it = debug_loc_map_.find(get_id(ref));
        if (it != debug_loc_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void MappingTable::directly_map_statement_identifier(ebm::StatementRef ref, std::string&& name) {
        statement_identifier_direct_map_[get_id(ref)] = std::move(name);
    }

}  // namespace ebmgen
