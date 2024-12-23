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
        if (n < 0x40) {
            v.prefix(0);
            v.value(n);
        }
        else if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        else if (n < 0x400000) {
            v.prefix(2);
            v.value(n);
        }
        else if (n < 0x40000000) {
            v.prefix(3);
            v.value(n);
        }
        else {
            return unexpect_error("Invalid varint value: {}", n);
        }
        return v;
    }

    // Control Flow Graph
    // basic_block: list of Module.code index
    // next: list of next CFG
    // prev: list of previous CFG
    struct CFG {
        std::vector<size_t> basic_block;
        std::vector<std::shared_ptr<CFG>> next;
        std::vector<std::weak_ptr<CFG>> prev;
    };

    struct Module {
        std::unordered_map<std::string, ObjectID> string_table;
        std::unordered_map<ObjectID, std::string> string_table_rev;
        std::unordered_map<std::shared_ptr<ast::Ident>, ObjectID> ident_table;
        std::unordered_map<ObjectID, std::shared_ptr<ast::Ident>> ident_table_rev;
        std::unordered_map<ObjectID, std::uint64_t> ident_index_table;
        std::unordered_map<std::shared_ptr<ast::StructType>, ObjectID> struct_table;
        std::vector<RangePacked> programs;
        std::vector<IdentRange> ident_to_ranges;
        std::vector<Code> code;
        std::uint64_t object_id = 1;
        std::vector<std::shared_ptr<CFG>> cfgs;

        void map_struct(std::shared_ptr<ast::StructType> s, ObjectID id) {
            struct_table[s] = id;
        }

        Varint lookup_struct(std::shared_ptr<ast::StructType> s) {
            auto it = struct_table.find(s);
            if (it == struct_table.end()) {
                return Varint();
            }
            return Varint(it->second);
        }

       private:
        std::uint64_t prev_expr_id = null_id;

       public:
        expected<Varint> get_prev_expr() {
            if (prev_expr_id == null_id) {
                return unexpect_error("No previous expression");
            }
            auto expr = varint(prev_expr_id);
            prev_expr_id = null_id;
            return expr;
        }

        void set_prev_expr(std::uint64_t id) {
            prev_expr_id = id;
        }

        expected<Varint> lookup_string(const std::string& str) {
            auto str_ref = string_table.find(str);
            if (str_ref == string_table.end()) {
                auto ident = new_id();
                if (!ident) {
                    return ident;
                }
                string_table.emplace(str, ident->value());
                string_table_rev.emplace(ident->value(), str);
                return ident;
            }
            return varint(str_ref->second);
        }

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
                ident_table_rev[id->value()] = base;
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
            code.push_back(std::move(c));
        }

        void op(AbstractOp op, auto&& set) {
            Code c;
            c.op = op;
            set(c);
            auto ident = c.ident();
            code.push_back(std::move(c));
            if (ident) {
                ident_index_table[ident->value()] = code.size() - 1;
            }
        }
    };

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node);

    Error save(Module& m, futils::binary::writer& w);

    Error optimize(Module& m, const std::shared_ptr<ast::Node>& node);

    void write_cfg(futils::binary::writer& w, Module& m);
}  // namespace rebgn
