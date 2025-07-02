#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <ostream> // Use ostream for flexible output

#include "ebm/extended_binary_module.hpp"

namespace ebmgen {

class DebugPrinter {
public:
    DebugPrinter(const ebm::ExtendedBinaryModule& module, std::ostream& os) : module_(module), os_(os) {}

    void print_module() const;

private:
    const ebm::ExtendedBinaryModule& module_;
    std::ostream& os_; // Reference to the output stream

    void print_identifier(const ebm::Identifier& ident) const;
    void print_string_literal(const ebm::StringLiteral& str_lit) const;
    void print_type(const ebm::Type& type) const;
    void print_type_body(const ebm::TypeBody& body) const;
    void print_statement(const ebm::Statement& stmt) const;
    void print_statement_body(const ebm::StatementBody& body) const;
    void print_expression(const ebm::Expression& expr) const;
    void print_expression_body(const ebm::ExpressionBody& body) const;
    void print_debug_info(const ebm::DebugInfo& debug_info) const;

    // Helper to get actual object from ref
    const ebm::Identifier* get_identifier(const ebm::IdentifierRef& ref) const;
    const ebm::StringLiteral* get_string_literal(const ebm::StringRef& ref) const;
    const ebm::Type* get_type(const ebm::TypeRef& ref) const;
    const ebm::Statement* get_statement(const ebm::StatementRef& ref) const;
    const ebm::Expression* get_expression(const ebm::ExpressionRef& ref) const;

    // Indentation helper
    mutable int indent_level_ = 0;
    void indent() const {
        for (int i = 0; i < indent_level_; ++i) {
            os_ << "  "; // Write to the provided output stream
        }
    }
};

} // namespace ebmgen
