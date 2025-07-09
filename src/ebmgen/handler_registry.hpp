#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <functional>
#include <unordered_map>

namespace ebmgen {
    enum class GenerateType {
        Normal,
        Encode,
        Decode,
    };
}

namespace std {
    template <>
    struct hash<std::pair<brgen::ast::NodeType, ebmgen::GenerateType>> {
        size_t operator()(const std::pair<brgen::ast::NodeType, ebmgen::GenerateType>& p) const {
            return hash<int>()(static_cast<int>(p.first)) ^ hash<int>()(static_cast<int>(p.second));
        }
    };

}  // namespace std
namespace ebmgen {

    class HandlerRegistry;  // Forward declaration to avoid circular dependency

    struct ConverterProxy {
       private:
        const GenerateType type;
        HandlerRegistry& converter;

       public:
        constexpr ConverterProxy(HandlerRegistry& conv, GenerateType t)
            : type(t), converter(conv) {}

        constexpr GenerateType get_type() const {
            return type;
        }

        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
    };

    // Defines the function signature for all statement handlers.
    // It takes a reference to the Converter to access its helper methods.
    using StatementHandler =
        std::function<expected<ebm::StatementRef>(ConverterProxy& c, ebm::StatementRef new_ref, const std::shared_ptr<ast::Node>& node)>;

    // A registry for the new, refactored handler functions.
    class HandlerRegistry {
       private:
        std::unordered_map<std::pair<ast::NodeType, GenerateType>, StatementHandler> statement_handlers;

        // This method will be implemented in the .cpp file.
        void register_statement_handlers();

       public:
        HandlerRegistry() {
            register_statement_handlers();
        }

        // Finds a handler for a given node type.
        // Returns nullptr if no handler is registered.
        StatementHandler* get_statement_handler(ConverterProxy& p, ast::NodeType type) {
            if (auto it = statement_handlers.find({type, p.get_type()}); it != statement_handlers.end()) {
                return &it->second;
            }
            return nullptr;
        }

       protected:
        // Provides a way for the implementation file to register handlers.
        void add_statement_handler(GenerateType p, ast::NodeType type, StatementHandler handler) {
            statement_handlers[{type, p}] = std::move(handler);
        }
    };

}  // namespace ebmgen
