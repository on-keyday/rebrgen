#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <unordered_set>

namespace ebmgen {

    class Converter {
       public:
        Converter(ebm::ExtendedBinaryModule& ebm) : ebm(ebm) {}

        Error convert(const std::shared_ptr<brgen::ast::Node>& ast_root);

       private:
        ebm::ExtendedBinaryModule& ebm;
        std::shared_ptr<ast::Node> root;
        Error err;
        std::uint64_t next_id = 1;
        std::unordered_map<std::shared_ptr<ast::Node>, ebm::StatementRef> visited_nodes;

        std::unordered_map<std::string, ebm::IdentifierRef> identifier_cache;
        std::unordered_map<std::string, ebm::StringRef> string_cache;
        std::unordered_map<std::string, ebm::TypeRef> type_cache;
        std::unordered_map<std::string, ebm::ExpressionRef> expression_cache;
        std::unordered_map<std::string, ebm::StatementRef> statement_cache;

        std::unordered_map<uint64_t, size_t> identifier_map;
        std::unordered_map<uint64_t, size_t> string_map;
        std::unordered_map<uint64_t, size_t> type_map;
        std::unordered_map<uint64_t, size_t> expression_map;
        std::unordered_map<uint64_t, size_t> statement_map;

        expected<ebm::ExpressionRef> new_expr_id();
        expected<ebm::TypeRef> new_type_id();
        expected<ebm::StatementRef> new_stmt_id();
        expected<ebm::IdentifierRef> new_ident_id(bool is_anonymous);
        expected<ebm::StringRef> new_string_id();

        template <typename T>
        expected<std::string> serialize(const T& body) {
            std::string buffer;
            futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            auto err = body.encode(w);
            if (err) {
                return unexpect_error(err);
            }
            return buffer;
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body);
        expected<ebm::StringRef> add_string(const std::string& str);
        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body);
        expected<ebm::IdentifierRef> add_anonymous_identifier();
        expected<ebm::IdentifierRef> add_identifier(const std::string& name);
        expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body);
        expected<ebm::StatementRef> add_statement(ebm::StatementRef stmt_id, ebm::StatementBody&& body);

        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type);
        expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);

        ebm::Identifier* get_identifier(ebm::IdentifierRef ref);
        ebm::StringLiteral* get_string(ebm::StringRef ref);
        ebm::Type* get_type(ebm::TypeRef ref);
        ebm::Expression* get_expression(ebm::ExpressionRef ref);
        ebm::Statement* get_statement(ebm::StatementRef ref);

        void convert_node(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
        expected<ebm::StatementRef> convert_statement_impl(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);

        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);

        Error set_lengths();
    };

}  // namespace ebmgen
