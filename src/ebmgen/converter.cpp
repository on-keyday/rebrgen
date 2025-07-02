#include "converter.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {

    expected<ebm::ExpressionRef> Converter::new_expr_id() {
        return varint(next_id++).transform(
            [](auto&& v) { return ebm::ExpressionRef{v}; });
    }

    expected<ebm::TypeRef> Converter::new_type_id() {
        return varint(next_id++).transform(
            [](auto&& v) { return ebm::TypeRef{v}; });
    }

    expected<ebm::StatementRef> Converter::new_stmt_id() {
        return varint(next_id++).transform(
            [](auto&& v) { return ebm::StatementRef{v}; });
    }

    expected<ebm::IdentifierRef> Converter::new_ident_id(bool is_anonymous) {
        return varint(next_id++).transform([&](auto&& v) {
            auto ref = ebm::IdentifierRef{v};
            ref.is_anonymous(is_anonymous);
            return ref;
        });
    }

    expected<ebm::StringRef> Converter::new_string_id() {
        return varint(next_id++).transform(
            [](auto&& v) { return ebm::StringRef{v}; });
    }

    expected<ebm::ExpressionRef> Converter::add_expr(ebm::ExpressionBody&& body) {
        auto serialized = serialize(body);
        if (!serialized) {
            return unexpect_error("Failed to serialize expression body: {}", serialized.error().error());
        }
        if (auto it = expression_cache.find(*serialized); it != expression_cache.end()) {
            return it->second;
        }
        auto expr_id = new_expr_id();
        if (!expr_id) {
            return expr_id;
        }
        ebm::Expression expr;
        expr.id = *expr_id;
        expr.body = std::move(body);
        ebm.expressions.push_back(std::move(expr));
        expression_map[expr_id->id.value()] = ebm.expressions.size() - 1;
        expression_cache[*serialized] = *expr_id;
        return *expr_id;
    }

    expected<ebm::StringRef> Converter::add_string(const std::string& str) {
        if (auto it = string_cache.find(str); it != string_cache.end()) {
            return it->second;
        }
        auto len = varint(str.size());
        if (!len) {
            return unexpect_error("Failed to create varint for string length: {}", len.error().error());
        }
        auto str_id = new_string_id();
        if (!str_id) {
            return str_id;
        }
        ebm::StringLiteral string;
        string.id = *str_id;
        string.value.length = *len;
        string.value.data = str;
        ebm.strings.push_back(std::move(string));
        string_map[str_id->id.value()] = ebm.strings.size() - 1;
        string_cache[str] = *str_id;
        return *str_id;
    }

    expected<ebm::TypeRef> Converter::add_type(ebm::TypeBody&& body) {
        auto serialized = serialize(body);
        if (!serialized) {
            return unexpect_error("Failed to serialize type body: {}", serialized.error().error());
        }
        if (auto it = type_cache.find(*serialized); it != type_cache.end()) {
            return it->second;
        }
        auto type_id = new_type_id();
        if (!type_id) {
            return type_id;
        }
        ebm::Type type;
        type.id = *type_id;
        type.body = std::move(body);
        ebm.types.push_back(std::move(type));
        type_map[type_id->id.value()] = ebm.types.size() - 1;
        type_cache[*serialized] = *type_id;
        return *type_id;
    }

    expected<ebm::IdentifierRef> Converter::add_anonymous_identifier() {
        auto id_ref = new_ident_id(true);
        if (!id_ref) {
            return id_ref;
        }
        return *id_ref;
    }

    expected<ebm::IdentifierRef> Converter::add_identifier(const std::string& name) {
        if (auto it = identifier_cache.find(name); it != identifier_cache.end()) {
            return it->second;
        }
        auto len = varint(name.size());
        if (!len) {
            return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
        }
        auto id_ref = new_ident_id(false);
        if (!id_ref) {
            return id_ref;
        }
        ebm::Identifier identifier;
        identifier.id = *id_ref;
        identifier.name.length = *len;
        identifier.name.data = name;
        ebm.identifiers.push_back(std::move(identifier));
        identifier_map[id_ref->id.value()] = ebm.identifiers.size() - 1;
        identifier_cache[name] = *id_ref;
        return *id_ref;
    }

    expected<ebm::StatementRef> Converter::add_statement(ebm::StatementBody&& body) {
        auto serialized = serialize(body);
        if (!serialized) {
            return unexpect_error("Failed to serialize statement body: {}", serialized.error().error());
        }
        if (auto it = statement_cache.find(*serialized); it != statement_cache.end()) {
            return it->second;
        }
        auto stmt_id = new_stmt_id();
        if (!stmt_id) {
            return stmt_id;
        }
        ebm::Statement stmt;
        stmt.id = *stmt_id;
        stmt.body = std::move(body);
        ebm.statements.push_back(std::move(stmt));
        statement_map[stmt_id->id.value()] = ebm.statements.size() - 1;
        statement_cache[*serialized] = *stmt_id;
        return *stmt_id;
    }

    expected<ebm::StatementRef> Converter::add_statement(ebm::StatementRef stmt_id, ebm::StatementBody&& body) {
        ebm::Statement stmt;
        stmt.id = stmt_id;
        stmt.body = std::move(body);
        ebm.statements.push_back(std::move(stmt));
        statement_map[stmt_id.id.value()] = ebm.statements.size() - 1;
        return stmt_id;
    }

    expected<ebm::StatementRef> Converter::convert_statement(const std::shared_ptr<ast::Node>& node) {
        if (auto it = visited_nodes.find(node); it != visited_nodes.end()) {
            return it->second;
        }
        auto new_ref = new_stmt_id();
        if (!new_ref) {
            return unexpect_error("Failed to create new statement ID: {}", new_ref.error().error());
        }
        visited_nodes[node] = *new_ref;
        return convert_statement_impl(*new_ref, node);
    }

    ebm::Identifier* Converter::get_identifier(ebm::IdentifierRef ref) {
        if (ref.id.value() == 0 || identifier_map.find(ref.id.value()) == identifier_map.end()) {
            return nullptr;
        }
        return &ebm.identifiers[identifier_map[ref.id.value()]];
    }

    ebm::StringLiteral* Converter::get_string(ebm::StringRef ref) {
        if (ref.id.value() == 0 || string_map.find(ref.id.value()) == string_map.end()) {
            return nullptr;
        }
        return &ebm.strings[string_map[ref.id.value()]];
    }

    ebm::Type* Converter::get_type(ebm::TypeRef ref) {
        if (ref.id.value() == 0 || type_map.find(ref.id.value()) == type_map.end()) {
            return nullptr;
        }
        return &ebm.types[type_map[ref.id.value()]];
    }

    ebm::Expression* Converter::get_expression(ebm::ExpressionRef ref) {
        if (ref.id.value() == 0 || expression_map.find(ref.id.value()) == expression_map.end()) {
            return nullptr;
        }
        return &ebm.expressions[expression_map[ref.id.value()]];
    }

    ebm::Statement* Converter::get_statement(ebm::StatementRef ref) {
        if (ref.id.value() == 0 || statement_map.find(ref.id.value()) == statement_map.end()) {
            return nullptr;
        }
        return &ebm.statements[statement_map[ref.id.value()]];
    }

    void Converter::convert_node(const std::shared_ptr<ast::Node>& node) {
        if (err) return;
        auto r = convert_statement(node);
        if (!r) {
            err = std::move(r.error());
        }
    }

    Error Converter::set_lengths() {
        auto identifiers_len = varint(ebm.identifiers.size());
        if (!identifiers_len) {
            return identifiers_len.error();
        }
        ebm.identifiers_len = *identifiers_len;

        auto strings_len = varint(ebm.strings.size());
        if (!strings_len) {
            return strings_len.error();
        }
        ebm.strings_len = *strings_len;

        auto types_len = varint(ebm.types.size());
        if (!types_len) {
            return types_len.error();
        }
        ebm.types_len = *types_len;

        auto statements_len = varint(ebm.statements.size());
        if (!statements_len) {
            return statements_len.error();
        }
        ebm.statements_len = *statements_len;

        auto expressions_len = varint(ebm.expressions.size());
        if (!expressions_len) {
            return expressions_len.error();
        }
        ebm.expressions_len = *expressions_len;

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
