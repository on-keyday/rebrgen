/*license*/
#pragma once
#include "binary_module.hpp"
#include <unordered_map>
#include <helper/expected.h>
#include <error/error.h>
#include <core/ast/ast.h>
#include <core/ast/tool/ident.h>
#include "common.hpp"
namespace rebgn {

    using ObjectID = std::uint64_t;

    constexpr ObjectID null_id = 0;

    constexpr expected<Varint> varint(std::uint64_t n) {
        Varint v;
        if (n < 0x80) {
            v.prefix(0);
            v.value(n);
        }
        else if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        else if (n < 0x200000) {
            v.prefix(2);
            v.value(n);
        }
        else if (n < 0x10000000) {
            v.prefix(3);
            v.value(n);
        }
        else {
            return unexpect_error("Invalid varint value: {}", n);
        }
        return v;
    }

    struct Module {
        std::unordered_map<std::string, std::uint64_t> string_table;
        std::unordered_map<std::shared_ptr<ast::Ident>, std::uint64_t> ident_table;
        std::vector<Code> code;
        std::uint64_t object_id = 1;

        std::uint64_t prev_expr_id = null_id;

        expected<Varint> lookup_ident(std::shared_ptr<ast::Ident> ident) {
            if (!ident) {
                return new_id();  // ephemeral id
            }
            auto [base, _] = *ast::tool::lookup_base(ident);
            auto it = ident_table.find(base);
            if (it == ident_table.end()) {
                auto id = new_id();
                if (!id) {
                    return id;
                }
                ident_table[base] = id->value();
                return id;
            }
            return varint(it->second);
        }

        expected<Varint> new_id() {
            return varint(object_id++);
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
