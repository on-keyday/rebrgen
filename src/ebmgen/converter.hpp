#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>

namespace ebmgen {

    class Converter {
        ebm::ExtendedBinaryModule& ebm;
        std::shared_ptr<ast::Node> root;
        Error err;
        std::uint64_t next_id = 1;

       private:
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

       public:
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

        void convert_node(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);

       public:
        Converter(ebm::ExtendedBinaryModule& ebm) : ebm(ebm) {}

        Error convert(const std::shared_ptr<brgen::ast::Node>& ast_root);
    };

}  // namespace ebmgen
