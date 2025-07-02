#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <functional>
#include <unordered_map>

namespace ebmgen {

    class Converter;  // Forward declaration to avoid circular dependency

    // Defines the function signature for all statement handlers.
    // It takes a reference to the Converter to access its helper methods.
    using StatementHandler =
        std::function<expected<ebm::StatementRef>(Converter&, ebm::StatementRef, const std::shared_ptr<ast::Node>&)>;

    // A registry for the new, refactored handler functions.
    class HandlerRegistry {
       private:
        std::unordered_map<ast::NodeType, StatementHandler> statement_handlers;

        // This method will be implemented in the .cpp file.
        void register_statement_handlers();

       public:
        HandlerRegistry() {
            register_statement_handlers();
        }

        // Finds a handler for a given node type.
        // Returns nullptr if no handler is registered.
        StatementHandler* get_statement_handler(ast::NodeType type) {
            if (auto it = statement_handlers.find(type); it != statement_handlers.end()) {
                return &it->second;
            }
            return nullptr;
        }

       protected:
        // Provides a way for the implementation file to register handlers.
        void add_statement_handler(ast::NodeType type, StatementHandler handler) {
            statement_handlers[type] = std::move(handler);
        }
    };

}  // namespace ebmgen
