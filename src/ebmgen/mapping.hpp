/*license*/
#pragma once
#include <ebm/extended_binary_module.hpp>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include "common.hpp"

namespace ebmgen {

    struct InverseRef {
        const char* name;
        std::optional<size_t> index;
        ebm::AnyRef ref;
        ebm::AliasHint hint;
    };

    struct MappingTable {
        MappingTable(const ebm::ExtendedBinaryModule& module)
            : module_(module) {
            build_maps();
        }
        const ebm::Identifier* get_identifier(const ebm::IdentifierRef& ref) const;
        const ebm::StringLiteral* get_string_literal(const ebm::StringRef& ref) const;
        const ebm::Type* get_type(const ebm::TypeRef& ref) const;
        const ebm::Statement* get_statement(const ebm::StatementRef& ref) const;
        const ebm::Expression* get_expression(const ebm::ExpressionRef& ref) const;

        std::optional<ebm::TypeKind> get_type_kind(const ebm::TypeRef& ref) const {
            if (const auto* type = get_type(ref)) {
                return type->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::StatementOp> get_statement_op(const ebm::StatementRef& ref) const {
            if (const auto* stmt = get_statement(ref)) {
                return stmt->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::ExpressionOp> get_expression_op(const ebm::ExpressionRef& ref) const {
            if (const auto* expr = get_expression(ref)) {
                return expr->body.kind;
            }
            return std::nullopt;
        }

        std::string get_identifier_or(const ebm::IdentifierRef& ref, const ebm::AnyRef& default_ref, std::string_view prefix = "tmp") const;

        template <AnyRef T>
        std::string get_identifier_or(const ebm::IdentifierRef& ref, const T& default_ref, std::string_view prefix = "tmp") const {
            return get_identifier_or(ref, ebm::AnyRef{default_ref.id}, prefix);
        }

        const ebm::Identifier* get_identifier(const ebm::StatementRef& ref) const;
        std::string get_identifier_or(const ebm::StatementRef& ref, std::string_view prefix = "tmp") const;

        const ebm::Identifier* get_identifier(const ebm::ExpressionRef& ref) const;

        // same as get_statement(ebm::StatementRef{module().max_id.id})
        const ebm::Statement* get_entry_point() const;

        const ebm::ExtendedBinaryModule& module() const {
            return module_;
        }

        const std::vector<InverseRef>* get_inverse_ref(const ebm::AnyRef& ref) const;

       private:
        const ebm::ExtendedBinaryModule& module_;
        // Caches for faster lookups
        std::unordered_map<std::uint64_t, const ebm::Identifier*> identifier_map_;
        std::unordered_map<std::uint64_t, const ebm::StringLiteral*> string_literal_map_;
        std::unordered_map<std::uint64_t, const ebm::Type*> type_map_;
        std::unordered_map<std::uint64_t, const ebm::Statement*> statement_map_;
        std::unordered_map<std::uint64_t, const ebm::Expression*> expression_map_;
        std::unordered_map<std::uint64_t, std::vector<InverseRef>> inverse_refs_;
        void build_maps();
    };
}  // namespace ebmgen