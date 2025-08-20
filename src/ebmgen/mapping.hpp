/*license*/
#pragma once
#include <ebm/extended_binary_module.hpp>
#include <map>
#include <cstdint>
#include <vector>

namespace ebmgen {

    struct InverseRef {
        const char* name;
        std::optional<size_t> index;
        ebm::AnyRef ref;
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

        const ebm::ExtendedBinaryModule& module() const {
            return module_;
        }

        const std::vector<InverseRef>* get_inverse_ref(const ebm::AnyRef& ref) const;

       private:
        const ebm::ExtendedBinaryModule& module_;
        // Caches for faster lookups
        std::map<std::uint64_t, const ebm::Identifier*> identifier_map_;
        std::map<std::uint64_t, const ebm::StringLiteral*> string_literal_map_;
        std::map<std::uint64_t, const ebm::Type*> type_map_;
        std::map<std::uint64_t, const ebm::Statement*> statement_map_;
        std::map<std::uint64_t, const ebm::Expression*> expression_map_;
        std::map<std::uint64_t, std::vector<InverseRef>> inverse_refs_;
        void build_maps();
    };
}  // namespace ebmgen