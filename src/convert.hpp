/*license*/
#pragma once
#include "binary_module.hpp"
#include <unordered_map>
#include <helper/expected.h>
#include <error/error.h>
#include <core/ast/ast.h>
#include <core/ast/tool/ident.h>
namespace rebgn {
    namespace ast = brgen::ast;
    using Error = futils::error::Error<>;

    template <typename T>
    using expected = futils::helper::either::expected<T, futils::error::Error<>>;

    using ObjectID = std::uint64_t;

    constexpr ObjectID null_id = 0;

    struct Module {
        std::unordered_map<std::string, std::uint64_t> string_table;
        std::unordered_map<std::shared_ptr<ast::Ident>, std::uint64_t> ident_table;
        std::vector<Code> code;
        std::uint64_t object_id = 1;

        std::uint64_t lookup_ident(std::shared_ptr<ast::Ident> ident) {
            if (!ident) {
                return null_id;
            }
            auto [base, _] = *ast::tool::lookup_base(ident);
            auto it = ident_table.find(base);
            if (it == ident_table.end()) {
                auto id = new_id();
                ident_table[base] = id;
                return id;
            }
            return it->second;
        }

        std::uint64_t new_id() {
            return object_id++;
        }

        void op(AbstractOp op) {
            Code c;
            c.op = op;
            code.push_back(c);
        }

        void op(AbstractOp op, auto&& set) {
            Code c;
            c.op = op;
            set(c);
            code.push_back(c);
        }
    };

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node);

}  // namespace rebgn
