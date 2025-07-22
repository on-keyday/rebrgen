#include "debug_printer.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include "ebm/extended_binary_module.hpp"
#include "helper/template_instance.h"
#include "common.hpp"

namespace ebmgen {

    // Constructor: Initializes maps for quick lookups
    DebugPrinter::DebugPrinter(const ebm::ExtendedBinaryModule& module, std::ostream& os)
        : module_(module), os_(os) {
        build_maps();
    }

    // Builds maps from vector data for faster access
    void DebugPrinter::build_maps() const {
        for (const auto& ident : module_.identifiers) {
            identifier_map_[ident.id.id.value()] = &ident;
        }
        for (const auto& str_lit : module_.strings) {
            string_literal_map_[str_lit.id.id.value()] = &str_lit;
        }
        for (const auto& type : module_.types) {
            type_map_[type.id.id.value()] = &type;
        }
        for (const auto& stmt : module_.statements) {
            statement_map_[stmt.id.id.value()] = &stmt;
        }
        for (const auto& expr : module_.expressions) {
            expression_map_[expr.id.id.value()] = &expr;
        }
    }

    // --- Helper functions to get objects from references ---
    const ebm::Identifier* DebugPrinter::get_identifier(const ebm::IdentifierRef& ref) const {
        auto it = identifier_map_.find(ref.id.value());
        return (it != identifier_map_.end()) ? it->second : nullptr;
    }

    const ebm::StringLiteral* DebugPrinter::get_string_literal(const ebm::StringRef& ref) const {
        auto it = string_literal_map_.find(ref.id.value());
        return (it != string_literal_map_.end()) ? it->second : nullptr;
    }

    const ebm::Type* DebugPrinter::get_type(const ebm::TypeRef& ref) const {
        auto it = type_map_.find(ref.id.value());
        return (it != type_map_.end()) ? it->second : nullptr;
    }

    const ebm::Statement* DebugPrinter::get_statement(const ebm::StatementRef& ref) const {
        auto it = statement_map_.find(ref.id.value());
        return (it != statement_map_.end()) ? it->second : nullptr;
    }

    const ebm::Expression* DebugPrinter::get_expression(const ebm::ExpressionRef& ref) const {
        auto it = expression_map_.find(ref.id.value());
        return (it != expression_map_.end()) ? it->second : nullptr;
    }

    template <class T, class U>
    concept has_visit = requires(T t, U u) {
        { t.visit(u) };
    };
    struct DummyFn {
        void operator()(auto&&, const char*, auto&&) const {}
    };

    void DebugPrinter::print_resolved_reference(const ebm::IdentifierRef& ref) const {
        const auto* ident = get_identifier(ref);
        os_ << ebm::Identifier::visitor_name << " ";
        if (ident) {
            os_ << ident->body.data;
        }
        else {
            os_ << "(unknown identifier)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::StringRef& ref) const {
        os_ << ebm::StringLiteral::visitor_name << " ";
        const auto* str_lit = get_string_literal(ref);
        if (str_lit) {
            os_ << str_lit->body.data;
        }
        else {
            os_ << "(unknown string literal)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::TypeRef& ref) const {
        os_ << ebm::Type::visitor_name << " ";
        const auto* type = get_type(ref);
        if (type) {
            os_ << to_string(type->body.kind);
        }
        else {
            os_ << "(unknown type)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::StatementRef& ref) const {
        os_ << ebm::Statement::visitor_name << " ";
        const auto* stmt = get_statement(ref);
        if (stmt) {
            os_ << to_string(stmt->body.statement_kind);
        }
        else {
            os_ << "(unknown statement)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::ExpressionRef& ref) const {
        os_ << ebm::Expression::visitor_name << " ";
        const auto* expr = get_expression(ref);
        if (expr) {
            os_ << to_string(expr->body.op);
        }
        else {
            os_ << "(unknown expression)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::AnyRef& ref) const {
        if (const auto* ident = get_identifier(ebm::IdentifierRef{ref.id})) {
            print_resolved_reference(ebm::IdentifierRef{ref.id});
        }
        else if (const auto* str_lit = get_string_literal(ebm::StringRef{ref.id})) {
            print_resolved_reference(ebm::StringRef{ref.id});
        }
        else if (const auto* type = get_type(ebm::TypeRef{ref.id})) {
            print_resolved_reference(ebm::TypeRef{ref.id});
        }
        else if (const auto* stmt = get_statement(ebm::StatementRef{ref.id})) {
            print_resolved_reference(ebm::StatementRef{ref.id});
        }
        else if (const auto* expr = get_expression(ebm::ExpressionRef{ref.id})) {
            print_resolved_reference(ebm::ExpressionRef{ref.id});
        }
        else {
            os_ << "(unknown reference)";
        }
    }

    // --- Generic print helpers ---
    template <typename T>
    void DebugPrinter::print_value(const T& value) const {
        if constexpr (std::is_enum_v<T>) {
            os_ << ebm::to_string(value) << "\n";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            os_ << (value ? "true" : "false") << "\n";
        }
        else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
            if (value.empty()) {
                os_ << "(empty vector)\n";
                return;
            }
            os_ << "\n";
            indent_level_++;
            for (const auto& elem : value) {
                indent();
                print_value(elem);
            }
            indent_level_--;
        }
        else if constexpr (std::is_same_v<T, ebm::Varint>) {
            os_ << value.value() << "\n";
        }
        else if constexpr (has_visit<T, DummyFn>) {
            os_ << value.visitor_name;
            if constexpr (AnyRef<T>) {
                os_ << " (ID: " << value.id.value() << " ";
                if (value.id.value() == 0) {
                    os_ << "(null)";
                }
                else {
                    print_resolved_reference(value);
                }
                os_ << ")\n";
            }
            else {
                os_ << "\n";
                indent_level_++;
                value.visit([&](auto&& visitor, const char* name, auto&& val) {
                    print_named_value(name, val);
                });
                indent_level_--;
            }
        }
        else if constexpr (std::is_same_v<T, std::uint8_t>) {
            os_ << static_cast<uint32_t>(value) << "\n";
        }
        else {
            os_ << value << "\n";
        }
    }

    template <typename T>
    void DebugPrinter::print_named_value(const char* name, const T& value) const {
        indent();
        os_ << name << ": ";
        print_value(value);
    }

    template <typename T>
    void DebugPrinter::print_named_value(const char* name, const T* value) const {
        if (value) {
            if constexpr (std::is_same_v<T, char>) {
                print_named_value(name, std::string(value));
            }
            else {
                print_named_value(name, *value);
            }
        }
    }

    // --- Main print methods ---
    void DebugPrinter::print_module() const {
        print_value(module_);
    }

}  // namespace ebmgen