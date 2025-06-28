#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>

namespace ebmgen {

    class Converter {
        ebm::ExtendedBinaryModule& ebm;
        std::shared_ptr<ast::Node> root;
        Error err;

       private:
        std::uint64_t next_id = 1;

        expected<ebm::ExpressionRef> new_expr_id() {
            return varint(next_id++).transform([](auto&& v) {
                return ebm::ExpressionRef{v};
            });
        }

        expected<ebm::TypeRef> new_type_id() {
            return varint(next_id++).transform([](auto&& v) {
                return ebm::TypeRef{v};
            });
        }

        expected<ebm::Varint> new_stmt_id() {
            return varint(next_id++).transform([](auto&& v) {
                return ebm::Varint{v};
            });
        }

        expected<ebm::IdentifierRef> new_ident_id() {
            return varint(next_id++).transform([](auto&& v) {
                return ebm::IdentifierRef{v};
            });
        }

        expected<ebm::StringRef> new_string_id() {
            return varint(next_id++).transform([](auto&& v) {
                return ebm::StringRef{v};
            });
        }

        expected<std::string> serialize(const auto& body) {
            std::string buffer;
            futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            auto err = body.encode(w);
            if (err) {
                return unexpect_error(err);
            }
            return buffer;
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body) {
            auto expr_id = new_expr_id();
            if (!expr_id) {
                return expr_id;
            }
            // TODO: bodyをserializeして既存のexpressionと比較すると一致するやつが簡単に見つかるので
            // その場合は新しいIDを割り当てる必要はなくキャッシュしたのを返せると思う。
            ebm::Expression expr;
            expr.id = *expr_id;
            expr.body = std::move(body);
            ebm.expressions.push_back(std::move(expr));
            return *expr_id;
        }

        expected<ebm::StringRef> add_string(const std::string& str) {
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
            return *str_id;
        }

        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body) {
            auto type_id = new_type_id();
            if (!type_id) {
                return type_id;
            }
            ebm::Type type;
            type.id = *type_id;
            type.body = std::move(body);
            ebm.types.push_back(std::move(type));
            return *type_id;
        }

        expected<ebm::IdentifierRef> add_identifier(const std::string& name) {
            auto len = varint(name.size());
            if (!len) {
                return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
            }
            auto id_ref = new_ident_id();
            if (!id_ref) {
                return id_ref;
            }
            ebm::Identifier identifier;
            identifier.id = *id_ref;
            identifier.name.length = *len;
            identifier.name.data = name;
            ebm.identifiers.push_back(std::move(identifier));
            return *id_ref;
        }

        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type);

        void convert_node(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);

       public:
        Converter(ebm::ExtendedBinaryModule& ebm) : ebm(ebm) {}

        Error convert(const std::shared_ptr<brgen::ast::Node>& ast_root);
    };

}  // namespace ebmgen