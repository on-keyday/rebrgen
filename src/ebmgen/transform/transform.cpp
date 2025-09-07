/*license*/
#include "transform.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include "../common.hpp"
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
        t.reset();
        std::erase_if(ctx.debug_locations(), [&](auto& d) {
            return !old_to_new.contains(d.ident.id.value());
        });
        print_if_verbose("Removed unreferenced debug information in ", t.delta<std::chrono::microseconds>(), "\n");
        for (auto& loc : ctx.debug_locations()) {
            if (auto it = old_to_new.find(loc.ident.id.value()); it != old_to_new.end()) {
                loc.ident.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.debug_locations().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        return {};
    }

    struct Route {
        std::vector<std::shared_ptr<CFG>> route;
        size_t bit_size = 0;
    };

    expected<void> lowered_dynamic_bit_io(CFGContext& tctx, bool write) {
        auto& ctx = tctx.tctx.context();
        auto& all_statements = tctx.tctx.statement_repository().get_all();
        auto current_added = all_statements.size();
        auto get_io = [&](ebm::Statement& stmt) {
            return write ? stmt.body.write_data() : stmt.body.read_data();
        };
        auto assign_to_target = [&](ebm::ExpressionRef ref, ebm::ExpressionRef mask, ebm::ExpressionRef shift_index) {

        };

        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            for (auto& ref : block->container) {
                MAYBE(stmt, tctx.tctx.statement_repository().get(ref));
                if (auto r = get_io(stmt); r && r->size.unit == ebm::SizeUnit::BIT_FIXED) {
                    auto found = tctx.cfg_map.find(stmt.id.id.value());
                    if (found == tctx.cfg_map.end()) {
                        return unexpect_error("no cfg found for {}:{}", stmt.id.id.value(), to_string(stmt.body.kind));
                    }
                    std::vector<Route> finalized_routes;
                    std::vector<Route> routes;
                    routes.push_back({
                        .route = {found->second},
                        .bit_size = r->size.size()->value(),
                    });  // first route
                    while (routes.size()) {
                        std::vector<Route> new_routes;
                        for (auto& r : routes) {
                            for (auto& n : r.route.back()->next) {
                                auto copy = r;
                                copy.route.push_back(n);
                                auto stmt = tctx.tctx.statement_repository().get(n->original_node);
                                if (!stmt) {
                                    continue;  // drop route
                                }
                                if (auto io_ = get_io(*stmt)) {
                                    if (io_->size.unit == ebm::SizeUnit::BIT_FIXED || io_->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                                        auto add_bit = io_->size.size()->value();
                                        if (io_->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                                            add_bit *= 8;
                                        }
                                        copy.bit_size += add_bit;
                                        if (copy.bit_size % 8 == 0) {
                                            finalized_routes.push_back(std::move(copy));
                                            continue;
                                        }
                                    }
                                    else {
                                        continue;  // drop route
                                    }
                                }
                                new_routes.push_back(std::move(copy));
                            }
                        }
                        routes = std::move(new_routes);
                    }
                    if (finalized_routes.size() > 1) {
                        print_if_verbose("Found ", finalized_routes.size(), " routes\n");
                        size_t max_bit_size = 0;
                        for (auto& r : finalized_routes) {
                            print_if_verbose("  - ", r.route.size(), " node with ", r.bit_size, " bit\n");
                            max_bit_size = (std::max)(max_bit_size, r.bit_size);
                        }
                        ebm::Block block;
                        EBMU_U8_N_ARRAY(max_buffer_t, max_bit_size / 8);
                        EBMU_COUNTER_TYPE(count_t);
                        EBM_DEFAULT_VALUE(default_buf_v, max_buffer_t);
                        EBMU_BOOL_TYPE(bool_t);
                        EBMU_U8(u8_t);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_buffer, max_buffer_t, default_buf_v);

                        // for read_offset < new_size:
                        //    tmp_buffer[read_offset] = read u8
                        //    read_offset++
                        auto read_incremental = [&](ebm::StatementRef io_ref, size_t& read_offset, size_t current_offset, size_t add_bit) -> expected<ebm::StatementRef> {
                            auto new_size = (current_offset + add_bit + 7) / 8;
                            ebm::Block block;
                            while (read_offset < new_size) {
                                EBMU_INT_LITERAL(i, read_offset);
                                EBM_INDEX(indexed, u8_t, tmp_buffer, i);
                                auto data = make_io_data(io_ref, indexed, u8_t, {}, get_size(8));
                                MAYBE(lowered, ctx.get_decoder_converter().decode_multi_byte_int_with_fixed_array(io_ref, 1, {}, indexed, u8_t));
                                ebm::LoweredStatements lows;
                                append(lows, make_lowered_statement(ebm::LoweringType::NAIVE, lowered));
                                EBM_LOWERED_STATEMENTS(l, std::move(lows));
                                data.lowered_statement = ebm::LoweredStatementRef{l};
                                EBM_READ_DATA(read_to_temporary, std::move(data));
                                append(block, read_to_temporary);
                                read_offset++;
                            }
                            if (!block.container.size()) {
                                return ebm::StatementRef{};  // no statement
                            }
                            EBM_BLOCK(read, std::move(block));
                            return read;
                        };
                        for (auto& r : finalized_routes) {
                            size_t current_offset = 0;
                            size_t read_offset = 0;
                            for (auto& c : r.route) {
                                MAYBE(stmt, tctx.tctx.statement_repository().get(c->original_node));
                                auto io_ = get_io(stmt);
                                if (io_) {
                                    auto add_bit = io_->size.size()->value();
                                    if (io_->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                                        add_bit *= 8;
                                    }
                                    if (!write) {
                                        MAYBE(io_cond, read_incremental(io_->io_ref, read_offset, current_offset, add_bit));
                                        EBM_DEFAULT_VALUE(zero, io_->data_type);
                                        EBMU_UINT_TYPE(unsigned_t, add_bit);
                                        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_holder, unsigned_t, zero);
                                        auto get_indexed = [&](size_t offset) -> expected<ebm::ExpressionRef> {
                                            EBMU_INT_LITERAL(offset_, offset);
                                            EBM_INDEX(idx, u8_t, tmp_buffer, offset_);
                                            return idx;
                                        };
                                        auto on_big_endian = [&] -> expected<ebm::StatementRef> {
                                            auto offset = current_offset / 8;
                                            auto bit_offset = current_offset % 8;
                                            size_t add_remain = add_bit;
                                            assert(add_bit >= 1);
                                            auto first_remaining = 8 - bit_offset;

                                            // explain with example
                                            // offset = 0
                                            // bit_offset = 3
                                            // unsigned_t = uint2
                                            // add_bit = 2
                                            // first_remaining = 8 - 3 = 5
                                            // 2 <= 5
                                            if (add_remain <= first_remaining) {  // in tmp_buffer[read_offset]
                                                // last_zero_bit = 5 - 2 = 3
                                                // mask = 0xff >> 3 = 0x1f
                                                // mask = 0x1f & (0xff << 3) = 0x1f & 0xf8 = 0x18
                                                auto last_zero_bit = first_remaining - add_remain;
                                                std::uint8_t mask = std::uint8_t(0xff) >> bit_offset;
                                                mask &= std::uint8_t(0xff) << last_zero_bit;
                                                MAYBE(idx, get_indexed(offset));
                                                EBMU_INT_LITERAL(masked, mask);
                                                // bits_ = tmp_buffer[0] & 0x18
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                if (last_zero_bit != 0) {
                                                    EBMU_INT_LITERAL(shift, last_zero_bit);
                                                    EBM_BINARY_OP(shift_, ebm::BinaryOp::right_shift, u8_t, bits_, shift);
                                                    // bits_ = ((tmp_buffer[0] & 0x18) >> 3)
                                                    bits_ = shift_;
                                                }
                                                // casted = uint2((tmp_buffer[0] & 0x18) >> 3)
                                                EBM_CAST(casted, unsigned_t, u8_t, bits_);
                                                EBM_ASSIGNMENT(assign, tmp_holder, casted);
                                                return assign;
                                            }
                                            // explain with example
                                            // offset = 0
                                            // bit_offset = 7
                                            // unsigned_t = uint10
                                            // add_bit = 1 + 8 + 1 = 10
                                            // first_remaining = 1
                                            std::optional<ebm::ExpressionRef> expr;
                                            if (bit_offset) {  // process first remaining bits (lsb)
                                                // mask = 0xff >> 7 = 0x01
                                                // shift = 10 - 1 = 9
                                                std::uint8_t mask = std::uint8_t(0xff) >> bit_offset;
                                                EBMU_INT_LITERAL(masked, mask);
                                                MAYBE(idx, get_indexed(offset));
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                EBMU_INT_LITERAL(shift, add_bit - first_remaining);
                                                EBM_CAST(casted, unsigned_t, u8_t, bits_);
                                                EBM_BINARY_OP(msb, ebm::BinaryOp::left_shift, unsigned_t, casted, shift);
                                                // expr = uint10(tmp_buffer[0] & 0x01) << 9
                                                expr = msb;
                                                bit_offset = 0;
                                                offset++;
                                                add_remain -= first_remaining;
                                                // here add_remain = 9
                                            }
                                            first_remaining %= 8;  // wrap around
                                            // remain_buffer = 1
                                            auto remain_buffer = add_remain / 8;
                                            for (size_t i = 0; i < remain_buffer; i++) {  // process bytes
                                                MAYBE(idx, get_indexed(offset));
                                                // shift_value = 10 - 1 - 1 * 8 = 10 - 9 = 1
                                                auto shift_value = add_bit - first_remaining - (i + 1) * 8;
                                                EBMU_INT_LITERAL(shift, shift_value);
                                                EBM_CAST(casted, unsigned_t, u8_t, idx);
                                                // shifted = uint10(tmp_buffer[1]) << 1
                                                EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, unsigned_t, casted, shift);
                                                if (expr) {
                                                    // expr = (uint10(tmp_buffer[0] & 0x01) << 9) | (uint10(tmp_buffer[1]) << 1)
                                                    EBM_BINARY_OP(or_, ebm::BinaryOp::bit_or, unsigned_t, *expr, shifted);
                                                    expr = or_;
                                                }
                                                else {
                                                    expr = shifted;
                                                }
                                                offset++;
                                            }
                                            // remain_bits = 9 % 8 = 1
                                            auto remain_bits = add_remain % 8;
                                            if (remain_bits) {  // process last remaining bits (msb)
                                                // mask = 0xff << (8 - 1) = 0xff << 7 = 0x80
                                                std::uint8_t mask = std::uint8_t(0xff) << (8 - remain_bits);
                                                MAYBE(idx, get_indexed(offset));
                                                EBMU_INT_LITERAL(masked, mask);
                                                // shift = 8 - 1 = 7
                                                EBMU_INT_LITERAL(shift, 8 - remain_bits);
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                EBM_BINARY_OP(shift_, ebm::BinaryOp::right_shift, u8_t, bits_, shift);
                                                // casted = uint10((tmp_buffer[2] & 0x80) >> 7)
                                                EBM_CAST(casted, unsigned_t, u8_t, shift_);
                                                if (expr) {
                                                    EBM_BINARY_OP(or_, ebm::BinaryOp::bit_or, unsigned_t, *expr, casted);
                                                    // expr = (uint10(tmp_buffer[0] & 0x01) << 9) | (uint10(tmp_buffer[1]) << 1) | uint10((tmp_buffer[2] & 0x80) >> 7)
                                                    expr = or_;
                                                }
                                                else {
                                                    expr = casted;
                                                }
                                            }
                                            if (!expr) {
                                                return unexpect_error("no expr");
                                            }
                                            EBM_ASSIGNMENT(assign, tmp_holder, *expr);
                                            return assign;
                                        };
                                        auto on_little_endian = [&] -> expected<ebm::StatementRef> {
                                            auto offset = current_offset / 8;
                                            auto bit_offset = current_offset % 8;
                                            size_t add_remain = add_bit;
                                            assert(add_bit >= 1);
                                            auto first_remaining = 8 - bit_offset;

                                            // explain with example
                                            // offset = 0
                                            // bit_offset = 4
                                            // unsigned_t = uint2
                                            // add_bit = 2
                                            // first_remaining = 8 - 4 = 4
                                            // 2 <= 4
                                            if (add_remain <= first_remaining) {  // in tmp_buffer[read_offset]
                                                // last_zero_bit = 4 - 2 = 2
                                                // mask = 0xff << 4 = 0xf0
                                                // mask = 0x1f & (0xff >> 2) = 0xf0 & 0x3f = 0x30
                                                auto last_zero_bit = first_remaining - add_remain;
                                                std::uint8_t mask = std::uint8_t(0xff) << bit_offset;
                                                mask &= std::uint8_t(0xff) >> last_zero_bit;
                                                MAYBE(idx, get_indexed(offset));
                                                EBMU_INT_LITERAL(masked, mask);
                                                // bits = tmp_buffer[0] & 0x30
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                if (bit_offset != 0) {
                                                    EBMU_INT_LITERAL(shift, bit_offset);
                                                    EBM_BINARY_OP(shift_, ebm::BinaryOp::right_shift, u8_t, bits_, shift);
                                                    // bits = (tmp_buffer[0] & 0x30) >> 4
                                                    bits_ = shift_;
                                                }
                                                // casted = uint2((tmp_buffer[0] & 0x30) >> 4)
                                                EBM_CAST(casted, unsigned_t, u8_t, bits_);
                                                EBM_ASSIGNMENT(assign, tmp_holder, casted);
                                                return assign;
                                            }
                                            // explain with example
                                            // offset = 0
                                            // bit_offset = 7
                                            // unsigned_t = uint10
                                            // add_bit = 1 + 8 + 1 = 10
                                            // first_remaining = 1
                                            std::optional<ebm::ExpressionRef> expr;
                                            if (bit_offset) {  // process first remaining bits (msb)
                                                // mask = 0xff << 7 = 0x80
                                                // shift = 7
                                                std::uint8_t mask = std::uint8_t(0xff) << bit_offset;
                                                EBMU_INT_LITERAL(masked, mask);
                                                MAYBE(idx, get_indexed(offset));
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                EBMU_INT_LITERAL(shift, bit_offset);
                                                EBM_BINARY_OP(shifted, ebm::BinaryOp::right_shift, unsigned_t, bits_, shift);
                                                EBM_CAST(lsb, unsigned_t, u8_t, shifted);
                                                // expr = uint10((tmp_buffer[0] & 0x80) >> 7)
                                                expr = lsb;
                                                bit_offset = 0;
                                                offset++;
                                                add_remain -= first_remaining;
                                            }
                                            first_remaining %= 8;  // wrap around
                                            // remain_buffer = 1
                                            auto remain_buffer = add_remain / 8;
                                            for (size_t i = 0; i < remain_buffer; i++) {  // process bytes
                                                MAYBE(idx, get_indexed(offset));
                                                // shift_value = 1 + 0 * 8 = 1
                                                auto shift_value = first_remaining + i * 8;
                                                EBMU_INT_LITERAL(shift, shift_value);
                                                EBM_CAST(casted, unsigned_t, u8_t, idx);
                                                // shifted = uint10(tmp_buffer[1]) << 1
                                                EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, unsigned_t, casted, shift);
                                                if (expr) {
                                                    EBM_BINARY_OP(or_, ebm::BinaryOp::bit_or, unsigned_t, *expr, shifted);
                                                    // expr = uint10((tmp_buffer[0] & 0x80) >> 7) | (uint10(tmp_buffer[1]) << 1)
                                                    expr = or_;
                                                }
                                                else {
                                                    expr = shifted;
                                                }
                                                offset++;
                                            }
                                            // remain_bits = 9 % 8 = 1
                                            auto remain_bits = add_remain % 8;
                                            if (remain_bits) {  // process last remaining bits (lsb)
                                                // mask = 0xff >> (8 - 1) = 0xff >> 7 = 0x01
                                                // shift = 1 + 1 * 8 = 9
                                                std::uint8_t mask = std::uint8_t(0xff) >> (8 - remain_bits);
                                                MAYBE(idx, get_indexed(offset));
                                                EBMU_INT_LITERAL(masked, mask);
                                                EBMU_INT_LITERAL(shift, first_remaining + remain_buffer * 8);
                                                EBM_BINARY_OP(bits_, ebm::BinaryOp::bit_and, u8_t, idx, masked);
                                                EBM_CAST(casted, unsigned_t, u8_t, bits_);
                                                // shift_ = uint10(tmp_buffer[2] & 0x01) << 9
                                                EBM_BINARY_OP(shift_, ebm::BinaryOp::left_shift, unsigned_t, casted, shift);
                                                if (expr) {
                                                    EBM_BINARY_OP(or_, ebm::BinaryOp::bit_or, unsigned_t, *expr, shift_);
                                                    // expr = uint10((tmp_buffer[0] & 0x80) >> 7) | (uint10(tmp_buffer[1]) << 1) | (uint10(tmp_buffer[2] & 0x01) << 9)
                                                    expr = or_;
                                                }
                                                else {
                                                    expr = shift_;
                                                }
                                            }
                                            if (!expr) {
                                                return unexpect_error("no expr");
                                            }
                                            EBM_ASSIGNMENT(assign, tmp_holder, *expr);
                                            return assign;
                                        };
                                        auto res = add_endian_specific(
                                            ctx, io_->attribute,
                                            on_little_endian,
                                            on_big_endian);
                                        if (!res) {
                                            return unexpect_error(std::move(res.error()));
                                        }
                                    }
                                    current_offset += add_bit;
                                }
                            }
                        }
                    }
                }
            }
        }
        return {};
    }

    /**
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
    */

    expected<void> detect_insertion_point(CFGContext& ctx) {
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
                print_if_verbose("Found ", found_cfg->second->prev.size(), " previous node for ", stmt.id.id.value(), "(", to_string(stmt.body.kind), ")\n");
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
        ctx.statement_repository().recalculate_cache();
        // internal CFG used optimization
        {
            CFGContext cfg_ctx{ctx};
            MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
            MAYBE_VOID(bit_io_read, lowered_dynamic_bit_io(cfg_ctx, false));
            MAYBE_VOID(bit_io_write, lowered_dynamic_bit_io(cfg_ctx, true));
            MAYBE_VOID(insertion_point, detect_insertion_point(cfg_ctx));
        }
        if (!debug) {
            MAYBE_VOID(remove_unused, remove_unused_object(ctx));
            ctx.recalculate_id_index_map();
        }
        // final cfg view
        CFGContext cfg_ctx{ctx};
        MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
        return cfg;
    }
}  // namespace ebmgen
