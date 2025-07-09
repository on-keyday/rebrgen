#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include "handler_registry.hpp"

namespace ebmgen {

    struct IdentifierSource {
       private:
        std::uint64_t next_id = 1;

       public:
        expected<ebm::Varint> new_id() {
            auto v = varint(next_id);
            if (!v) {
                return unexpect_error(std::move(v.error()));
            }
            next_id++;
            return v;
        }
    };

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

    template <class ID, class Instance, class Body>
    struct ReferenceRepository {
        expected<ID> new_id(IdentifierSource& source) {
            return source.new_id().and_then([this](ebm::Varint id) -> expected<ID> {
                return ID{id};
            });
        }
        expected<ID> add(ID id, Body&& body) {
            identifier_map[id.id.value()] = instances.size();
            Instance instance;
            instance.id = id;
            instance.body = std::move(body);
            instances.push_back(std::move(instance));
            return id;
        }

        expected<ID> add(IdentifierSource& source, Body&& body) {
            auto serialized = serialize(body);
            if (!serialized) {
                return unexpect_error(std::move(serialized.error()));
            }
            if (auto it = cache.find(*serialized); it != cache.end()) {
                return it->second;
            }
            auto id = new_id(source);
            if (!id) {
                return unexpect_error(std::move(id.error()));
            }
            return add(*id, std::move(body));
        }

        Instance* get(const ID& id) {
            if (id.id.value() == 0 || identifier_map.find(id.id.value()) == identifier_map.end()) {
                return nullptr;
            }
            return &instances[identifier_map[id.id.value()]];
        }

        std::vector<Instance>& get_all() {
            return instances;
        }

       private:
        std::unordered_map<std::string, ID> cache;
        std::unordered_map<uint64_t, size_t> identifier_map;
        std::vector<Instance> instances;
    };

    class Converter {
       public:
        Converter(ebm::ExtendedBinaryModule& ebm)
            : ebm(ebm) {}

        Error convert(const std::shared_ptr<brgen::ast::Node>& ast_root);

       private:
        friend struct ConverterProxy;
        ebm::ExtendedBinaryModule& ebm;
        std::shared_ptr<ast::Node> root;
        Error err;
        // std::uint64_t next_id = 1;
        IdentifierSource ident_source;
        GenerateType current_generate_type = GenerateType::Normal;
        std::unordered_map<std::shared_ptr<ast::Node>, ebm::StatementRef> visited_nodes;

        ReferenceRepository<ebm::IdentifierRef, ebm::Identifier, ebm::String> identifier_repo;
        ReferenceRepository<ebm::StringRef, ebm::StringLiteral, ebm::String> string_repo;
        ReferenceRepository<ebm::TypeRef, ebm::Type, ebm::TypeBody> type_repo;
        ReferenceRepository<ebm::ExpressionRef, ebm::Expression, ebm::ExpressionBody> expression_repo;
        ReferenceRepository<ebm::StatementRef, ebm::Statement, ebm::StatementBody> statement_repo;

        expected<ebm::IdentifierRef> add_identifier(const std::string& name) {
            auto len = varint(name.size());
            if (!len) {
                return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = name;
            string_literal.length = *len;
            return identifier_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body) {
            return type_repo.add(ident_source, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementRef id, ebm::StatementBody&& body) {
            return statement_repo.add(id, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body) {
            return statement_repo.add(ident_source, std::move(body));
        }

        expected<ebm::StringRef> add_string(const std::string& str) {
            auto len = varint(str.size());
            if (!len) {
                return unexpect_error("Failed to create varint for string length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = str;
            string_literal.length = *len;
            return string_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body) {
            return expression_repo.add(ident_source, std::move(body));
        }

        /*
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
        expected<ebm::IdentifierRef> new_ident_id();
        expected<ebm::StringRef> new_string_id();

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body);
        expected<ebm::StringRef> add_string(const std::string& str);
        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body);
        expected<ebm::IdentifierRef> add_identifier(const std::string& name);
        expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body);
        expected<ebm::StatementRef> add_statement(ebm::StatementRef stmt_id, ebm::StatementBody&& body);

        ebm::Identifier* get_identifier(ebm::IdentifierRef ref);
        ebm::StringLiteral* get_string(ebm::StringRef ref);
        ebm::Type* get_type(ebm::TypeRef ref);
        ebm::Expression* get_expression(ebm::ExpressionRef ref);
        ebm::Statement* get_statement(ebm::StatementRef ref);

        */

        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type);
        expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);

        void convert_node(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
        expected<ebm::StatementRef> convert_statement_impl(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);
        Error encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        Error decode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);

        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);

        Error set_lengths();
    };

}  // namespace ebmgen
