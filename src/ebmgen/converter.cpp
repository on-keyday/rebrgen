#include "converter.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {

    void Converter::convert_node(const std::shared_ptr<ast::Node>& node) {
        if (err) return;
        if (auto expr = ast::as<ast::Expr>(node)) {
            auto ref = convert_expr(ast::cast_to<ast::Expr>(node));
            if (!ref) {
                err = ref.error();
            }
        }
    }

    expected<ebm::ExpressionRef> Converter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        ebm::ExpressionBody body;
        if (auto literal = ast::as<ast::IntLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_INT;
            auto value = literal->parse_as<std::uint64_t>();
            if (!value) {
                return unexpect_error("cannot parse int literal");
            }
            body.int_value(*value);
            return add_expr(std::move(body));
        }
        else {
            return unexpect_error("not implemented yet");
        }
    }

    Error Converter::convert(const std::shared_ptr<brgen::ast::Node>& ast_root) {
        root = ast_root;
        ast::traverse(root, [&](auto& node) {
            convert_node(node);
        });
        return err;
    }

}  // namespace ebmgen
