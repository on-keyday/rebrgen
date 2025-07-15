#include "converter.hpp"
#include <core/ast/traverse.h>
#include "convert/helper.hpp"
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

    expected<ebm::EndianExpr> Converter::get_endian(ebm::Endian base, bool sign) {
        ebm::EndianExpr e;
        e.sign(sign);
        e.endian(base);
        if (base != ebm::Endian::unspec) {
            return e;
        }
        e.endian(global_endian);
        if (on_function) {
            e.endian(local_endian);
        }
        if (e.endian() == ebm::Endian::dynamic) {
            e.dynamic_ref = current_dynamic_endian;
        }
        return e;
    }

    bool Converter::set_endian(ebm::Endian e, ebm::StatementRef id) {
        if (on_function) {
            local_endian = e;
            current_dynamic_endian = id;
            return true;
        }
        if (e == ebm::Endian::dynamic) {
            return false;
        }
        global_endian = e;
        return true;
    }

    expected<void> Converter::set_lengths() {
        MAYBE(identifiers_len, varint(identifier_repo.get_all().size()));
        ebm.identifiers_len = identifiers_len;
        ebm.identifiers = std::move(identifier_repo.get_all());

        MAYBE(strings_len, varint(string_repo.get_all().size()));
        ebm.strings_len = strings_len;
        ebm.strings = std::move(string_repo.get_all());

        MAYBE(types_len, varint(type_repo.get_all().size()));
        ebm.types_len = types_len;
        ebm.types = std::move(type_repo.get_all());

        MAYBE(statements_len, varint(statement_repo.get_all().size()));
        ebm.statements_len = statements_len;
        ebm.statements = std::move(statement_repo.get_all());

        MAYBE(expressions_len, varint(expression_repo.get_all().size()));
        ebm.expressions_len = expressions_len;
        ebm.expressions = std::move(expression_repo.get_all());

        return {};
    }

    expected<void> Converter::convert(const std::shared_ptr<brgen::ast::Node>& ast_root) {
        root = ast_root;
        MAYBE_VOID(ok, convert_statement(ast_root));
        return set_lengths();
    }

}  // namespace ebmgen
