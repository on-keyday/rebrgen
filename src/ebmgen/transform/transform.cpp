/*license*/
#include "transform.hpp"
#include <algorithm>
#include <functional>
#include <type_traits>
#include "../common.hpp"
#include "binary/writer.h"
#include "control_flow_graph.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/convert/helper.hpp"
#include "ebmgen/converter.hpp"
#include "wrap/cout.h"
#include <set>
#include <testutil/timer.h>

namespace ebmgen {
    auto print_if_verbose(auto&&... args) {
        if (verbose_error) {
            (futils::wrap::cerr_wrap() << ... << args);
        }
    }

    auto get_block(ebm::StatementBody& body) {
        ebm::Block* block = nullptr;
        body.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, ebm::Block*>) {
                block = value;
            }
            else
                VISITOR_RECURSE(visitor, name, value)
        });
        return block;
    }

    expected<void> vectorized_io(TransformContext& tctx, bool write) {
        // Implementation of the grouping I/O transformation
        auto& all_statements = tctx.statement_repository().get_all();
        auto current_added = all_statements.size();
        auto current_alias = tctx.alias_vector().size();
        std::map<size_t, std::vector<std::pair<std::pair<size_t, size_t>, std::function<expected<ebm::StatementRef>()>>>> update;
        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            auto iter = block->container.begin();
            std::vector<std::tuple<size_t, ebm::StatementRef, ebm::IOData*>> io;
            std::vector<std::vector<std::tuple<size_t, ebm::StatementRef, ebm::IOData*>>> ios;
            for (size_t j = 0; j < block->container.size(); ++j, ++iter) {
                auto stmt = tctx.statement_repository().get(*iter);
                if (!stmt) {
                    return unexpect_error("Invalid statement reference in block: {}", iter->id.value());
                }
                if (auto n = (write ? stmt->body.write_data() : stmt->body.read_data()); n && (n->size.unit == ebm::SizeUnit::BIT_FIXED || n->size.unit == ebm::SizeUnit::BYTE_FIXED)) {
                    io.push_back({j, *iter, n});
                }
                else {
                    if (io.size() > 1) {
                        ios.push_back(std::move(io));
                        io.clear();
                    }
                }
            }
            if (io.size() > 1) {
                ios.push_back(std::move(io));
            }
            if (ios.size()) {
                print_if_verbose(write ? "Write" : "Read", " I/O groups:", ios.size(), "\n");
                for (auto& g : ios) {
                    print_if_verbose("  Group size: ", g.size(), "\n");
                    std::optional<std::uint64_t> all_in_byte = 0;
                    std::uint64_t all_in_bits = 0;
                    for (auto& ref : g) {
                        print_if_verbose("    - Statement ID: ", std::get<1>(ref).id.value(), "\n");
                        print_if_verbose("    - Size: ", std::get<2>(ref)->size.size()->value(), " ", to_string(std::get<2>(ref)->size.unit), "\n");
                        if (all_in_byte) {
                            if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                                all_in_byte = all_in_byte.value() + std::get<2>(ref)->size.size()->value();
                            }
                            else {
                                all_in_byte = std::nullopt;  // Mixed units, cannot use byte
                            }
                        }
                        if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BIT_FIXED) {
                            all_in_bits += std::get<2>(ref)->size.size()->value();
                        }
                        else if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                            all_in_bits += std::get<2>(ref)->size.size()->value() * 8;
                        }
                        else {
                            return unexpect_error("Unsupported size unit in read I/O: {}", to_string(std::get<2>(ref)->size.unit));
                        }
                    }
                    ebm::Size total_size;
                    ebm::TypeRef data_typ;
                    if (all_in_byte) {
                        MAYBE(fixed_bytes, make_fixed_size(*all_in_byte, ebm::SizeUnit::BYTE_FIXED));
                        total_size = fixed_bytes;
                        auto& ctx = tctx.context();
                        EBMU_U8_N_ARRAY(typ, *all_in_byte);
                        data_typ = typ;
                    }
                    else {
                        if (all_in_bits % 8 == 0) {
                            MAYBE(fixed_bytes, make_fixed_size(all_in_bits / 8, ebm::SizeUnit::BYTE_FIXED));
                            total_size = fixed_bytes;
                            auto& ctx = tctx.context();
                            EBMU_U8_N_ARRAY(typ, all_in_bits / 8);
                            data_typ = typ;
                        }
                        else {
                            MAYBE(fixed_bits, make_fixed_size(all_in_bits, ebm::SizeUnit::BIT_FIXED));
                            total_size = fixed_bits;
                            auto& ctx = tctx.context();
                            EBMU_UINT_TYPE(typ, all_in_bits);
                            data_typ = typ;
                        }
                    }
                    print_if_verbose("Total size: ", total_size.size()->value(), " ", to_string(total_size.unit), "\n");
                    auto io_data = make_io_data(std::get<2>(g[0])->io_ref, {}, data_typ, {}, total_size);
                    io_data.attribute.vectorized(true);
                    ebm::Block original_io;
                    for (auto& ref : g) {
                        append(original_io, std::get<1>(ref));
                    }
                    std::pair<size_t, size_t> group_range = {std::get<0>(g.front()), std::get<0>(g.back())};
                    update[i].emplace_back(group_range, [=, &tctx, original_io = std::move(original_io), io_data = std::move(io_data)]() mutable -> expected<ebm::StatementRef> {
                        auto& ctx = tctx.context();
                        EBM_BLOCK(original_, std::move(original_io));
                        io_data.lowered_statement = ebm::LoweredStatementRef{original_};
                        if (write) {
                            EBM_WRITE_DATA(vectorized_write, std::move(io_data));
                            return vectorized_write;
                        }
                        else {
                            EBM_READ_DATA(vectorized_read, std::move(io_data));
                            return vectorized_read;
                        }
                    });
                }
            }
        }
        std::map<std::uint64_t, ebm::StatementRef> map_statements;
        for (auto& [block_id, updates] : update) {
            auto& stmt = tctx.statement_repository().get_all()[block_id];
            auto block = get_block(stmt.body);
            if (!block) {
                return unexpect_error("Failed to get block from statement");
            }
            std::vector<std::pair<std::pair<size_t, size_t>, ebm::StatementRef>> new_statements;
            for (auto& [group_range, updater] : updates) {
                MAYBE(new_stmt, updater());
                print_if_verbose("Adding vectorized I/O statement for block ", block_id, ": ", new_stmt.id.value(), "\n");
                new_statements.emplace_back(group_range, std::move(new_stmt));
            }
            ebm::Block updated_block;
            size_t g = 0;
            for (size_t b = 0; b < block->container.size(); ++b) {
                if (g < new_statements.size() && new_statements[g].first.first == b) {
                    append(updated_block, new_statements[g].second);
                    b = new_statements[g].first.second;
                    g++;
                    continue;
                }
                append(updated_block, block->container[b]);
            }
            auto& ctx = tctx.context();
            EBM_BLOCK(new_block, std::move(updated_block));
            map_statements.emplace(stmt.id.id.value(), new_block);
        }
        for (size_t i = 0; i < current_added; i++) {
            auto& stmt = tctx.statement_repository().get_all()[i];
            stmt.body.visit([&](auto&& visitor, const char* name, auto&& value) {
                if constexpr (std::is_same_v<ebm::StatementRef&, decltype(value)>) {
                    auto found = map_statements.find(value.id.value());
                    if (found != map_statements.end()) {
                        value = found->second;
                    }
                }
                else
                    VISITOR_RECURSE(visitor, name, value)
            });
        }
        for (size_t i = 0; i < current_alias; i++) {
            auto& alias = tctx.alias_vector()[i];
            if (alias.hint == ebm::AliasHint::STATEMENT) {
                auto found = map_statements.find(alias.to.id.value());
                if (found != map_statements.end()) {
                    alias.to.id = found->second.id;
                }
            }
        }
        return {};
    }

    template <class B>
    concept has_body_kind = requires(B b) {
        { b.body.kind };
    };

    expected<void> remove_unused_object(TransformContext& ctx) {
        MAYBE(max_id, ctx.max_id());
        std::map<size_t, std::vector<ebm::AnyRef>> used_refs;
        auto trial = [&] -> expected<size_t> {
            used_refs.clear();
            auto map_to = [&](const auto& vec) {
                for (const auto& item : vec) {
                    item.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                        if constexpr (AnyRef<decltype(val)>) {
                            if (val.id.value() != 0) {
                                used_refs[val.id.value()].push_back(ebm::AnyRef{item.id.id});
                            }
                        }
                        else
                            VISITOR_RECURSE_CONTAINER(visitor, name, val)
                        else VISITOR_RECURSE(visitor, name, val)
                    });
                }
            };
            map_to(ctx.statement_repository().get_all());
            map_to(ctx.identifier_repository().get_all());
            map_to(ctx.type_repository().get_all());
            map_to(ctx.string_repository().get_all());
            map_to(ctx.expression_repository().get_all());
            for (auto& alias : ctx.alias_vector()) {
                if (used_refs.find(alias.from.id.value()) == used_refs.end()) {
                    print_if_verbose("Removing unused alias: ", alias.from.id.value(), "\n");
                    continue;  // Skip unused aliases
                }
                switch (alias.hint) {
                    case ebm::AliasHint::IDENTIFIER:
                        used_refs[alias.to.id.value()].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::STATEMENT:
                        used_refs[alias.to.id.value()].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::STRING:
                        used_refs[alias.to.id.value()].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::EXPRESSION:
                        used_refs[alias.to.id.value()].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::TYPE:
                        used_refs[alias.to.id.value()].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::ALIAS:
                        return unexpect_error("Alias hint should not contains ALIAS: {} -> {}", alias.from.id.value(), alias.to.id.value());
                }
            }
            std::set<std::uint64_t> should_remove;
            for (size_t i = 2 /*0 is nil, 1 is root node*/; i < max_id.value(); i++) {
                auto found = used_refs.find(i);
                if (found != used_refs.end()) {
                    continue;
                }
                should_remove.insert(i);
            }
            // simply remove
            auto remove = [&](auto&& rem) {
                std::erase_if(rem, [&](const auto& item) {
                    if (should_remove.find(item.id.id.value()) != should_remove.end()) {
                        print_if_verbose("Removing unused item: ", item.id.id.value());
                        if constexpr (has_body_kind<decltype(item)>) {
                            print_if_verbose("(", to_string(item.body.kind), ")");
                        }
                        print_if_verbose("\n");
                        return true;
                    }
                    return false;
                });
            };
            remove(ctx.statement_repository().get_all());
            remove(ctx.identifier_repository().get_all());
            remove(ctx.type_repository().get_all());
            remove(ctx.string_repository().get_all());
            remove(ctx.expression_repository().get_all());
            std::erase_if(ctx.alias_vector(), [&](const auto& alias) {
                return should_remove.find(alias.from.id.value()) != should_remove.end() ||
                       should_remove.find(alias.to.id.value()) != should_remove.end();
            });
            return used_refs.size();
        };
        futils::test::Timer t;
        MAYBE(first_size, trial());
        MAYBE(second_size, trial());
        while (first_size != second_size) {
            first_size = second_size;
            MAYBE(third_size, trial());
            second_size = third_size;
        }
        print_if_verbose("Removed unused items in ", t.next_step<std::chrono::microseconds>(), "\n");
        std::vector<std::tuple<ebm::AnyRef, size_t>> most_used;
        for (const auto& [id, refs] : used_refs) {
            most_used.emplace_back(ebm::AnyRef{id}, refs.size());
        }
        std::stable_sort(most_used.begin(), most_used.end(), [](const auto& a, const auto& b) {
            return std::get<1>(a) > std::get<1>(b);
        });
        std::map<size_t, ebm::AnyRef> old_to_new;
        ctx.set_max_id(0);  // reset
        for (auto& mapping : most_used) {
            MAYBE(new_id, ctx.new_id());
            old_to_new[std::get<0>(mapping).id.value()] = new_id;
        }
        MAYBE(entry_id, ctx.new_id());
        old_to_new[1] = entry_id;
        auto remap = [&](auto& vec) {
            t.reset();
            for (auto& item : vec) {
                if (auto it = old_to_new.find(item.id.id.value()); it != old_to_new.end()) {
                    item.id.id = it->second.id;
                }
                item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                    if constexpr (AnyRef<decltype(val)>) {
                        if (val.id.value() != 0) {
                            auto it = old_to_new.find(val.id.value());
                            if (it != old_to_new.end()) {
                                val.id = it->second.id;
                            }
                        }
                    }
                    else if constexpr (is_container<decltype(val)>) {
                        for (size_t i = 0; i < val.container.size(); ++i) {
                            visitor(visitor, name, val.container[i], i);
                        }
                    }
                    else
                        VISITOR_RECURSE(visitor, name, val)
                });
            }
            print_if_verbose("Remap ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
            t.reset();
            std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
                return a.id.id.value() < b.id.id.value();
            });
            print_if_verbose("Sort ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        };
        remap(ctx.statement_repository().get_all());
        remap(ctx.identifier_repository().get_all());
        remap(ctx.type_repository().get_all());
        remap(ctx.string_repository().get_all());
        remap(ctx.expression_repository().get_all());
        t.reset();
        for (auto& alias : ctx.alias_vector()) {
            if (auto it = old_to_new.find(alias.from.id.value()); it != old_to_new.end()) {
                alias.from.id = it->second.id;
            }
            if (auto it = old_to_new.find(alias.to.id.value()); it != old_to_new.end()) {
                alias.to.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        std::sort(ctx.alias_vector().begin(), ctx.alias_vector().end(), [](const auto& a, const auto& b) {
            return a.from.id.value() < b.from.id.value();
        });
        print_if_verbose("Sort ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        return {};
    }

    expected<void> lowered_dynamic_bit_io(CFGContext& tctx) {
        auto& all_statements = tctx.tctx.statement_repository().get_all();
        auto current_added = all_statements.size();
        auto current_alias = tctx.tctx.alias_vector().size();
        std::map<size_t, std::vector<std::pair<std::pair<size_t, size_t>, std::function<expected<ebm::StatementRef>()>>>> update;
        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            auto found = tctx.cfg_map.find(all_statements[i].id.id.value());
            if (found == tctx.cfg_map.end()) {
                continue;
            }
            if (found->second->prev.size() != 0) {
                continue;
            }
        }
        return {};
    }

    bool is_flatten_target(ebm::ExpressionOp op) {
        switch (op) {
            case ebm::ExpressionOp::CONDITIONAL_STATEMENT:
            case ebm::ExpressionOp::READ_DATA:
            case ebm::ExpressionOp::WRITE_DATA:
            case ebm::ExpressionOp::GET_STREAM_OFFSET:
            case ebm::ExpressionOp::CAN_READ_STREAM:
                return true;
            default:
                return false;
        }
    }

    expected<ebm::ExpressionRef> transparent_identifier(TransformContext& tctx, ebm::Block& block, ebm::ExpressionRef expr_ref) {
        // return expr_ref;
        auto& ctx = tctx.context();
        auto expr = ctx.repository().get_expression(expr_ref);
        auto expr_type = expr->body.type;
        EBM_DEFINE_VARIABLE(transparent_ref, {}, expr_type, expr_ref, false, true);
        append(block, transparent_ref_def);
        return transparent_ref;
    }

    expected<ebm::ExpressionRef> flatten_expression(TransformContext& tctx, ebm::Block& block, ebm::ExpressionRef expr_ref) {
        auto& ctx = tctx.context();
        auto expr = ctx.repository().get_expression(expr_ref);
        auto expr_type = expr->body.type;
        switch (expr->body.kind) {
            case ebm::ExpressionOp::LITERAL_INT:
            case ebm::ExpressionOp::LITERAL_INT64:
            case ebm::ExpressionOp::LITERAL_CHAR:
            case ebm::ExpressionOp::LITERAL_STRING:
            case ebm::ExpressionOp::LITERAL_BOOL:
            case ebm::ExpressionOp::LITERAL_TYPE:
            case ebm::ExpressionOp::IDENTIFIER:
            case ebm::ExpressionOp::MAX_VALUE:
            case ebm::ExpressionOp::DEFAULT_VALUE:
            case ebm::ExpressionOp::IS_LITTLE_ENDIAN:
            case ebm::ExpressionOp::IS_ERROR:
            case ebm::ExpressionOp::GET_REMAINING_BYTES:
            case ebm::ExpressionOp::GET_STREAM_OFFSET:
            case ebm::ExpressionOp::CAN_READ_STREAM: {
                // return transparent_identifier(tctx, block, expr_ref);
                return expr_ref;
            }
            case ebm::ExpressionOp::BINARY_OP: {
                auto left = *expr->body.left();
                auto right = *expr->body.right();
                auto bop = *expr->body.bop();
                // short circuit
                if (bop == ebm::BinaryOp::logical_and || bop == ebm::BinaryOp::logical_or) {
                    EBMU_BOOL_TYPE(typ);
                    EBM_DEFAULT_VALUE(initial_ref, typ);
                    MAYBE(left, flatten_expression(tctx, block, left));
                    EBM_DEFINE_ANONYMOUS_VARIABLE(result, typ, left);
                    append(block, result_def);
                    ebm::Block then_block;
                    MAYBE(right, flatten_expression(tctx, then_block, right));
                    EBM_ASSIGNMENT(assign, result, right);
                    ebm::StatementRef then_ref;
                    if (then_block.container.size()) {
                        append(then_block, assign);
                        EBM_BLOCK(then_, std::move(then_block));
                        then_ref = then_;
                    }
                    else {
                        then_ref = assign;
                    }
                    EBM_BINARY_OP(cond, bop == ebm::BinaryOp::logical_and ? ebm::BinaryOp::not_equal : ebm::BinaryOp::equal, typ, result, initial_ref);
                    EBM_IF_STATEMENT(if_stmt, cond, then_ref, {});
                    append(block, if_stmt);
                    return result;
                }
                MAYBE(left_p, flatten_expression(tctx, block, left));
                MAYBE(right_p, flatten_expression(tctx, block, right));
                EBM_BINARY_OP(bop_ref, bop, expr_type, left_p, right_p);
                return transparent_identifier(tctx, block, bop_ref);
            }
            case ebm::ExpressionOp::UNARY_OP: {
                auto inner = *expr->body.operand();
                auto uop = *expr->body.uop();
                MAYBE(inner_p, flatten_expression(tctx, block, inner));
                EBM_UNARY_OP(uop_ref, uop, expr_type, inner_p);
                return transparent_identifier(tctx, block, uop_ref);
            }
            case ebm::ExpressionOp::ARRAY_SIZE: {
                auto array = *expr->body.array_expr();
                MAYBE(array_p, flatten_expression(tctx, block, array));
                EBM_ARRAY_SIZE(size_ref, array_p);
                return transparent_identifier(tctx, block, size_ref);
            }
            case ebm::ExpressionOp::CALL: {
                auto call_desc = *expr->body.call_desc();  // copy to avoid re-fetching
                MAYBE(callee_p, flatten_expression(tctx, block, call_desc.callee));
                ebm::Expressions args;
                for (auto& arg : call_desc.arguments.container) {
                    MAYBE(arg_p, flatten_expression(tctx, block, arg));
                    append(args, arg_p);
                }
                call_desc.callee = callee_p;
                call_desc.arguments = std::move(args);
                EBM_CALL(call_ref, expr_type, std::move(call_desc));
                return transparent_identifier(tctx, block, call_ref);
            }
            case ebm::ExpressionOp::INDEX_ACCESS: {
                auto index = *expr->body.index();
                auto base = *expr->body.base();
                MAYBE(base_p, flatten_expression(tctx, block, base));
                MAYBE(index_p, flatten_expression(tctx, block, index));
                EBM_INDEX(index_ref, expr_type, base_p, index_p);
                return transparent_identifier(tctx, block, index_ref);
            }
            case ebm::ExpressionOp::MEMBER_ACCESS: {
                auto base = *expr->body.base();
                auto member = *expr->body.member();
                MAYBE(base_p, flatten_expression(tctx, block, base));
                EBM_MEMBER_ACCESS(member_ref, expr_type, base_p, member);
                return transparent_identifier(tctx, block, member_ref);
            }
            case ebm::ExpressionOp::WRITE_DATA: {
                auto target_expr = *expr->body.target_expr();
                auto io_ = *expr->body.io_statement();
                MAYBE(target_p, flatten_expression(tctx, block, target_expr));
                append(block, io_);
                return expr_ref;
            }
            case ebm::ExpressionOp::READ_DATA: {
                auto target_stmt = *expr->body.target_stmt();
                auto io_ = *expr->body.io_statement();
                append(block, target_stmt);
                append(block, io_);
                EBM_IDENTIFIER(id, target_stmt, expr_type);
                return id;
            }
            case ebm::ExpressionOp::RANGE: {
                auto start_ref = *expr->body.start();
                auto end_ref = *expr->body.end();
                if (start_ref.id.value()) {
                    MAYBE(start, flatten_expression(tctx, block, start_ref));
                    start_ref = start;
                }
                if (end_ref.id.value()) {
                    MAYBE(end, flatten_expression(tctx, block, end_ref));
                    end_ref = end;
                }
                EBM_RANGE(range_ref, expr_type, start_ref, end_ref);
                return transparent_identifier(tctx, block, range_ref);
            }
            case ebm::ExpressionOp::TYPE_CAST: {
                auto from_type = *expr->body.from_type();
                auto inner = *expr->body.source_expr();
                auto cast_kind = *expr->body.cast_kind();
                MAYBE(inner_p, flatten_expression(tctx, block, inner));
                EBM_CAST(cast_ref, expr_type, from_type, inner_p);
                return transparent_identifier(tctx, block, cast_ref);
            }
            case ebm::ExpressionOp::CONDITIONAL_STATEMENT: {
                auto target_stmt = *expr->body.target_stmt();
                auto conditional_stmt = *expr->body.conditional_stmt();
                append(block, target_stmt);
                append(block, conditional_stmt);
                EBM_IDENTIFIER(id, target_stmt, expr_type);
                return id;
            }
        }
        return unexpect_error("Unsupported flatten expression kind: {}", to_string(expr->body.kind));
    }

    expected<void> flatten_io_expression(CFGContext& ctx) {
        size_t count = 0;
        size_t cfg_count = 0;
        for (auto& stmt : ctx.tctx.statement_repository().get_all()) {
            std::optional<ebm::Condition*> cond_ref;
            stmt.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                if constexpr (std::is_same_v<std::decay_t<decltype(val)>, ebm::Condition>) {
                    cond_ref = &val;
                }
                else
                    VISITOR_RECURSE_CONTAINER(visitor, name, val)
                else VISITOR_RECURSE(visitor, name, val)
            });
            if (!cond_ref) {
                continue;
            }
            count++;
            auto found_cfg = ctx.cfg_map.find(stmt.id.id.value());
            if (found_cfg != ctx.cfg_map.end()) {
                cfg_count++;
            }
        }
        print_if_verbose("Found ", count, " conditional statements\n");
        print_if_verbose("Found ", cfg_count, " conditional statements in CFG\n");
        /*
        auto& statements = ctx.tctx.statement_repository().get_all();
        std::set<std::uint64_t> toplevel_expressions;
        for (auto& stmt : statements) {
            stmt.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                if constexpr (std::is_same_v<std::decay_t<decltype(val)>, ebm::ExpressionRef>) {
                    if (val.id.value() != 0) {
                        toplevel_expressions.insert(val.id.value());
                    }
                }
                else
                    VISITOR_RECURSE_CONTAINER(visitor, name, val)
                else VISITOR_RECURSE(visitor, name, val)
            });
        }
        print_if_verbose("Found ", toplevel_expressions.size(), " top-level expressions while expressions are ", ctx.expression_repository().get_all().size(), "\n");
        std::vector<std::pair<ebm::Block, ebm::ExpressionRef>> flattened_expressions;
        for (auto& expr : toplevel_expressions) {
            ebm::Block flattened;
            MAYBE(new_ref, flatten_expression(ctx, flattened, ebm::ExpressionRef{expr}));
            if (flattened.container.size()) {
                auto expr_ptr = ctx.expression_repository().get(ebm::ExpressionRef{expr});
                auto mapped_ptr = ctx.expression_repository().get(new_ref);
                print_if_verbose("Flatten expression ", expr, "(", expr_ptr ? to_string(expr_ptr->body.kind) : "<unknown>", ")", " into ", flattened.container.size(), " statements and mapped to ", new_ref.id.value(), "(", mapped_ptr ? to_string(mapped_ptr->body.kind) : "<unknown>", ")\n");
                for (auto& flt : flattened.container) {
                    auto stmt_ptr = ctx.statement_repository().get(flt);
                    print_if_verbose("  - Statement ID: ", flt.id.value(), "(", stmt_ptr ? to_string(stmt_ptr->body.kind) : "<unknown>", ")\n");
                }
                flattened_expressions.emplace_back(std::move(flattened), new_ref);
            }
            else if (expr != new_ref.id.value()) {
                auto expr_ptr = ctx.expression_repository().get(ebm::ExpressionRef{expr});
                auto mapped_ptr = ctx.expression_repository().get(new_ref);
                print_if_verbose("Flatten expression ", expr, "(", expr_ptr ? to_string(expr_ptr->body.kind) : "<unknown>", ")", " into no statements but mapped to ", new_ref.id.value(), "(", mapped_ptr ? to_string(mapped_ptr->body.kind) : "<unknown>", ")\n");
            }
        }
        print_if_verbose("Total flattened expressions: ", flattened_expressions.size(), "\n");
        */
        return {};
    }

    expected<CFGList> transform(TransformContext& ctx, bool debug) {
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
        if (!debug) {
            MAYBE_VOID(remove_unused, remove_unused_object(ctx));
            ctx.recalculate_id_index_map();
        }
        CFGContext cfg_ctx{ctx};
        MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
        return cfg;
    }
}  // namespace ebmgen
