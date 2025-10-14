/*license*/
#pragma once
#include <ebm/extended_binary_module.hpp>
#include <unordered_map>
#include <cstdint>
#include <variant>
#include <vector>
#include "common.hpp"

namespace ebmgen {

    struct InverseRef {
        const char* name;
        std::optional<size_t> index;
        ebm::AnyRef ref;
        ebm::AliasHint hint;
    };

    using ObjectVariant = std::variant<std::monostate, const ebm::Identifier*, const ebm::StringLiteral*, const ebm::Type*, const ebm::Statement*, const ebm::Expression*>;

    struct MappingTable {
        MappingTable(const ebm::ExtendedBinaryModule& module)
            : module_(module) {
            build_maps();
        }

        bool valid() const;

        const ebm::Identifier* get_identifier(const ebm::IdentifierRef& ref) const;
        const ebm::StringLiteral* get_string_literal(const ebm::StringRef& ref) const;
        const ebm::Type* get_type(const ebm::TypeRef& ref) const;
        const ebm::Statement* get_statement(const ebm::StatementRef& ref) const;
        const ebm::Expression* get_expression(const ebm::ExpressionRef& ref) const;

        ObjectVariant get_object(const ebm::AnyRef& ref) const;

        std::optional<ebm::TypeKind> get_type_kind(const ebm::TypeRef& ref) const {
            if (const auto* type = get_type(ref)) {
                return type->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::StatementKind> get_statement_kind(const ebm::StatementRef& ref) const {
            if (const auto* stmt = get_statement(ref)) {
                return stmt->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::ExpressionKind> get_expression_kind(const ebm::ExpressionRef& ref) const {
            if (const auto* expr = get_expression(ref)) {
                return expr->body.kind;
            }
            return std::nullopt;
        }

        const ebm::Identifier* get_identifier(const ebm::StatementRef& ref) const;
        std::string get_identifier_or(const ebm::StatementRef& ref, std::string_view prefix = "") const;
        expected<std::string> get_identifier_or(const ebm::ExpressionRef& ref, std::string_view prefix = "") const;

        const ebm::Identifier* get_identifier(const ebm::ExpressionRef& ref) const;

        const ebm::Identifier* get_identifier(const ebm::TypeRef& ref) const;

        const ebm::Identifier* get_identifier(const ebm::AnyRef& ref) const;

        // same as get_statement(ebm::StatementRef{module().max_id.id})
        const ebm::Statement* get_entry_point() const;

        const ebm::ExtendedBinaryModule& module() const {
            return module_;
        }

        const std::vector<InverseRef>* get_inverse_ref(const ebm::AnyRef& ref) const;

        void register_default_prefix(ebm::StatementKind kind, std::string_view prefix) {
            default_identifier_prefix_[kind] = prefix;
        }

        std::string_view get_default_prefix(ebm::StatementRef ref) const;

        std::string_view get_default_prefix(ebm::StatementKind kind) const {
            auto found = default_identifier_prefix_.find(kind);
            if (found != default_identifier_prefix_.end()) {
                return found->second;
            }
            return "tmp";
        }

       private:
        const ebm::ExtendedBinaryModule& module_;
        // Caches for faster lookups
        std::unordered_map<std::uint64_t, const ebm::Identifier*> identifier_map_;
        std::unordered_map<std::uint64_t, const ebm::StringLiteral*> string_literal_map_;
        std::unordered_map<std::uint64_t, const ebm::Type*> type_map_;
        std::unordered_map<std::uint64_t, const ebm::Statement*> statement_map_;
        std::unordered_map<std::uint64_t, const ebm::Expression*> expression_map_;
        std::unordered_map<std::uint64_t, std::vector<InverseRef>> inverse_refs_;
        std::unordered_map<ebm::StatementKind, std::string> default_identifier_prefix_;
        void build_maps();
    };
}  // namespace ebmgen