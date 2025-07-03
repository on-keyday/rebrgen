#include "handler_registry.hpp"
#include "converter.hpp"
#include "common.hpp"

namespace ebmgen {

    expected<ebm::StatementRef> ConverterProxy::convert_statement(const std::shared_ptr<ast::Node>& node) {
        // Get the handler for the current node type
        auto handler = converter.get_statement_handler(*this, node->node_type);
        if (!handler) {
            return unexpect_error("No handler registered for node type: {}", node_type_to_string(node->node_type));
        }
        // Call the handler with the current node
        return (*handler)(*this, ebm::StatementRef{}, node);
    }

    // This is where we will register the new, refactored handlers.
    // For now, it is empty. We will add handlers one by one.
    void HandlerRegistry::register_statement_handlers() {
        // Example of what will be added here later:
        // add_statement_handler(ast::NodeType::assert, &Converter::handle_assert_statement);
    }

}  // namespace ebmgen
