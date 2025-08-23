#include "debug_printer.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include "ebm/extended_binary_module.hpp"
#include "helper/template_instance.h"
#include "common.hpp"

namespace ebmgen {

    // Constructor: Initializes maps for quick lookups
    DebugPrinter::DebugPrinter(const ebm::ExtendedBinaryModule& module, std::ostream& os)
        : module_(module), os_(os) {
    }

    void DebugPrinter::print_resolved_reference(const ebm::IdentifierRef& ref) const {
        const auto* ident = module_.get_identifier(ref);
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
        const auto* str_lit = module_.get_string_literal(ref);
        if (str_lit) {
            os_ << str_lit->body.data;
        }
        else {
            os_ << "(unknown string literal)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::TypeRef& ref) const {
        os_ << ebm::Type::visitor_name << " ";
        const auto* type = module_.get_type(ref);
        if (type) {
            os_ << to_string(type->body.kind);
            if (auto id = type->body.id()) {
                os_ << " ";
                print_resolved_reference(*id);
            }
        }
        else {
            os_ << "(unknown type)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::StatementRef& ref) const {
        os_ << ebm::Statement::visitor_name << " ";
        const auto* stmt = module_.get_statement(ref);
        if (stmt) {
            os_ << to_string(stmt->body.kind);
            auto ident = module_.get_identifier(stmt->id);
            if (ident) {
                os_ << " name:";
                os_ << ident->body.data;
            }
        }
        else {
            os_ << "(unknown statement)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::ExpressionRef& ref) const {
        os_ << ebm::Expression::visitor_name << " ";
        const auto* expr = module_.get_expression(ref);
        if (expr) {
            os_ << to_string(expr->body.kind);
            if (auto id = expr->body.id()) {
                os_ << " ";
                print_resolved_reference(*id);
            }
        }
        else {
            os_ << "(unknown expression)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::AnyRef& ref) const {
        if (const auto* ident = module_.get_identifier(ebm::IdentifierRef{ref.id})) {
            print_resolved_reference(ebm::IdentifierRef{ref.id});
        }
        else if (const auto* str_lit = module_.get_string_literal(ebm::StringRef{ref.id})) {
            print_resolved_reference(ebm::StringRef{ref.id});
        }
        else if (const auto* type = module_.get_type(ebm::TypeRef{ref.id})) {
            print_resolved_reference(ebm::TypeRef{ref.id});
        }
        else if (const auto* stmt = module_.get_statement(ebm::StatementRef{ref.id})) {
            print_resolved_reference(ebm::StatementRef{ref.id});
        }
        else if (const auto* expr = module_.get_expression(ebm::ExpressionRef{ref.id})) {
            print_resolved_reference(ebm::ExpressionRef{ref.id});
        }
        else {
            os_ << "(unknown reference)";
        }
    }

    void DebugPrinter::print_any_ref(auto value) const {
        os_ << " (ID: " << value.id.value() << " ";
        if (value.id.value() == 0) {
            os_ << "(null)";
        }
        else {
            print_resolved_reference(value);
        }
        os_ << ")";
    }

    // --- Generic print helpers ---
    template <typename T>
    void DebugPrinter::print_value(const T& value) const {
        if constexpr (std::is_enum_v<T>) {
            os_ << ebm::visit_enum(value) << " " << ebm::to_string(value) << "\n";
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
                print_any_ref(value);
                os_ << "\n";
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
        print_value(module_.module());
        os_ << "Inverse references:\n";
        indent_level_++;
        for (std::uint64_t i = 1; i <= module_.module().max_id.id.value(); ++i) {
            print_any_ref(ebm::AnyRef{i});
            auto found = module_.get_inverse_ref(ebm::AnyRef{i});
            if (!found) {
                os_ << ": (no references)\n";
                continue;
            }
            os_ << ":\n";
            indent_level_++;
            for (const auto& ref : *found) {
                indent();
                print_any_ref(ref.ref);
                os_ << " (from: " << ref.name;
                if (ref.index) {
                    os_ << "[" << *ref.index << "]";
                }
                os_ << ", hint: " << to_string(ref.hint) << ")\n";
            }
            indent_level_--;
        }
        indent_level_--;
    }

}  // namespace ebmgen