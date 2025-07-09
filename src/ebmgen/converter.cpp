#include "converter.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {

    expected<ebm::StatementRef> Converter::convert_statement(const std::shared_ptr<ast::Node>& node) {
        if (auto it = visited_nodes.find(node); it != visited_nodes.end()) {
            return it->second;
        }
        auto new_ref = statement_repo.new_id(ident_source);
        if (!new_ref) {
            return unexpect_error("Failed to create new statement ID: {}", new_ref.error().error());
        }
        visited_nodes[node] = *new_ref;
        return convert_statement_impl(*new_ref, node);
    }

    void Converter::convert_node(const std::shared_ptr<ast::Node>& node) {
        if (err) return;
        auto r = convert_statement(node);
        if (!r) {
            err = std::move(r.error());
        }
    }

    Error Converter::set_lengths() {
        auto identifiers_len = varint(identifier_repo.get_all().size());
        if (!identifiers_len) {
            return identifiers_len.error();
        }
        ebm.identifiers_len = *identifiers_len;
        ebm.identifiers = std::move(identifier_repo.get_all());

        auto strings_len = varint(string_repo.get_all().size());
        if (!strings_len) {
            return strings_len.error();
        }
        ebm.strings_len = *strings_len;
        ebm.strings = std::move(string_repo.get_all());

        auto types_len = varint(type_repo.get_all().size());
        if (!types_len) {
            return types_len.error();
        }
        ebm.types_len = *types_len;
        ebm.types = std::move(type_repo.get_all());

        auto statements_len = varint(statement_repo.get_all().size());
        if (!statements_len) {
            return statements_len.error();
        }
        ebm.statements_len = *statements_len;
        ebm.statements = std::move(statement_repo.get_all());

        auto expressions_len = varint(expression_repo.get_all().size());
        if (!expressions_len) {
            return expressions_len.error();
        }
        ebm.expressions_len = *expressions_len;
        ebm.expressions = std::move(expression_repo.get_all());

        return Error();
    }

    Error Converter::convert(const std::shared_ptr<brgen::ast::Node>& ast_root) {
        root = ast_root;
        convert_node(ast_root);
        if (err) {
            return err;
        }
        return set_lengths();
    }

}  // namespace ebmgen
