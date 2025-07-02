#include "handler_registry.hpp"
#include "converter.hpp"

namespace ebmgen {

    // This is where we will register the new, refactored handlers.
    // For now, it is empty. We will add handlers one by one.
    void HandlerRegistry::register_statement_handlers() {
        // Example of what will be added here later:
        // add_statement_handler(ast::NodeType::assert, &Converter::handle_assert_statement);
    }

}  // namespace ebmgen
