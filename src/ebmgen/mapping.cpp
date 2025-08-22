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
                map[item.id.id.value()] = &item;
                item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                    if constexpr (AnyRef<decltype(val)>) {
                        if (val.id.value() != 0) {
                            inverse_refs_[val.id.value()].push_back(InverseRef{
                                .name = name,
                                .index = index,
                                .ref = ebm::AnyRef{item.id.id},
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
            map[alias.from.id.value()] = map[alias.to.id.value()];
            inverse_refs_[alias.to.id.value()].push_back(InverseRef{
                .name = to_string(alias.hint),
                .ref = ebm::AnyRef{alias.from.id},
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
    }

    // --- Helper functions to get objects from references ---
    const ebm::Identifier* MappingTable::get_identifier(const ebm::IdentifierRef& ref) const {
        auto it = identifier_map_.find(ref.id.value());
        return (it != identifier_map_.end()) ? it->second : nullptr;
    }

    const ebm::StringLiteral* MappingTable::get_string_literal(const ebm::StringRef& ref) const {
        auto it = string_literal_map_.find(ref.id.value());
        return (it != string_literal_map_.end()) ? it->second : nullptr;
    }

    const ebm::Type* MappingTable::get_type(const ebm::TypeRef& ref) const {
        auto it = type_map_.find(ref.id.value());
        return (it != type_map_.end()) ? it->second : nullptr;
    }

    const ebm::Statement* MappingTable::get_statement(const ebm::StatementRef& ref) const {
        auto it = statement_map_.find(ref.id.value());
        return (it != statement_map_.end()) ? it->second : nullptr;
    }

    const ebm::Expression* MappingTable::get_expression(const ebm::ExpressionRef& ref) const {
        auto it = expression_map_.find(ref.id.value());
        return (it != expression_map_.end()) ? it->second : nullptr;
    }

    const std::vector<InverseRef>* MappingTable::get_inverse_ref(const ebm::AnyRef& ref) const {
        auto it = inverse_refs_.find(ref.id.value());
        if (it != inverse_refs_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const ebm::Statement* MappingTable::get_entry_point() const {
        return get_statement(ebm::StatementRef{module_.max_id.id});
    }

    std::string MappingTable::get_identifier_or(const ebm::IdentifierRef& ref, const ebm::AnyRef& default_ref, std::string_view prefix) const {
        if (const ebm::Identifier* id = get_identifier(ref)) {
            return id->body.data;
        }
        return std::format("{}{}", prefix, default_ref.id.value());
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

    std::string MappingTable::get_identifier_or(const ebm::StatementRef& ref, std::string_view prefix) const {
        if (const ebm::Identifier* id = get_identifier(ref)) {
            return id->body.data;
        }
        return std::format("{}{}", prefix, ref.id.value());
    }
}  // namespace ebmgen
