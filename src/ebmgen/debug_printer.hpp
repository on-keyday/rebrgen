#pragma once

#include <cstdint>
#include <iostream>
#include <map>
#include <ostream>

#include "ebm/extended_binary_module.hpp"

namespace ebmgen {

    class DebugPrinter {
       public:
        DebugPrinter(const ebm::ExtendedBinaryModule& module, std::ostream& os);

        void print_module() const;

       private:
        const ebm::ExtendedBinaryModule& module_;
        std::ostream& os_;  // Reference to the output stream

        // Caches for faster lookups
        mutable std::map<std::uint64_t, const ebm::Identifier*> identifier_map_;
        mutable std::map<std::uint64_t, const ebm::StringLiteral*> string_literal_map_;
        mutable std::map<std::uint64_t, const ebm::Type*> type_map_;
        mutable std::map<std::uint64_t, const ebm::Statement*> statement_map_;
        mutable std::map<std::uint64_t, const ebm::Expression*> expression_map_;

        void build_maps() const;

        const ebm::Identifier* get_identifier(const ebm::IdentifierRef& ref) const;
        const ebm::StringLiteral* get_string_literal(const ebm::StringRef& ref) const;
        const ebm::Type* get_type(const ebm::TypeRef& ref) const;
        const ebm::Statement* get_statement(const ebm::StatementRef& ref) const;
        const ebm::Expression* get_expression(const ebm::ExpressionRef& ref) const;

        void print_resolved_reference(const ebm::IdentifierRef& ref) const;
        void print_resolved_reference(const ebm::StringRef& ref) const;
        void print_resolved_reference(const ebm::TypeRef& ref) const;
        void print_resolved_reference(const ebm::StatementRef& ref) const;
        void print_resolved_reference(const ebm::ExpressionRef& ref) const;
        void print_resolved_reference(const ebm::AnyRef& ref) const;

        // Indentation helper
        mutable int indent_level_ = 0;
        void indent() const {
            for (int i = 0; i < indent_level_; ++i) {
                os_ << "  ";  // Write to the provided output stream
            }
        }

        template <typename T>
        void print_value(const T& value) const;

        template <typename T>
        void print_named_value(const char* name, const T& value) const;

        template <typename T>
        void print_named_value(const char* name, const T* value) const;
    };

}  // namespace ebmgen