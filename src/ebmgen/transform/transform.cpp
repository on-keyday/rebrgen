/*license*/
#include "transform.hpp"
#include <algorithm>
#include <functional>
#include <type_traits>
#include "../common.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/convert/helper.hpp"
#include "ebmgen/converter.hpp"
#include "wrap/cout.h"
#include <set>

namespace ebmgen {
    auto print_if_verbose(auto&&... args) {
        if (verbose_error) {
            (futils::wrap::cerr_wrap() << ... << args);
        }
    }

    expected<void> vectorized_io(TransformContext& tctx, bool write) {
        // Implementation of the grouping I/O transformation
        auto& all_statements = tctx.statement_repository().get_all();
        auto current_added = all_statements.size();
        auto current_alias = tctx.alias_vector().size();
        std::map<size_t, std::vector<std::pair<std::pair<size_t, size_t>, std::function<expected<ebm::StatementRef>()>>>> update;
        auto get_block = [&](ebm::StatementBody& body) {
            ebm::Block* block = nullptr;
            body.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>, ebm::Block*>) {
                    block = value;
                }
                else
                    VISITOR_RECURSE(visitor, name, value)
            });
            return block;
        };
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
                print_if_verbose("Read I/O groups:", ios.size(), "\n");
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
                        io_data.lowered_stmt = original_;
                        EBM_READ_DATA(vectorized_read, std::move(io_data));
                        return vectorized_read;
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

    expected<void> remove_unused(TransformContext& ctx) {
        MAYBE(max_id, ctx.max_id());
        std::map<size_t, std::vector<ebm::AnyRef>> used_refs;
        auto trial = [&] -> expected<size_t> {
            used_refs.clear();
            auto map_to = [&](const auto& vec) {
                for (const auto& item : vec) {
                    item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                        if constexpr (AnyRef<decltype(val)>) {
                            if (val.id.value() != 0) {
                                used_refs[val.id.value()].push_back(ebm::AnyRef{item.id.id});
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
                        print_if_verbose("Removing unused item: ", item.id.id.value(), "\n");
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
        MAYBE(first_size, trial());
        MAYBE(second_size, trial());
        while (first_size != second_size) {
            first_size = second_size;
            MAYBE(third_size, trial());
            second_size = third_size;
        }
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
        };
        remap(ctx.statement_repository().get_all());
        remap(ctx.identifier_repository().get_all());
        remap(ctx.type_repository().get_all());
        remap(ctx.string_repository().get_all());
        remap(ctx.expression_repository().get_all());
        for (auto& alias : ctx.alias_vector()) {
            if (auto it = old_to_new.find(alias.from.id.value()); it != old_to_new.end()) {
                alias.from.id = it->second.id;
            }
            if (auto it = old_to_new.find(alias.to.id.value()); it != old_to_new.end()) {
                alias.to.id = it->second.id;
            }
        }
        return {};
    }

    expected<void> transform(TransformContext& ctx) {
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
        MAYBE_VOID(remove_unused, remove_unused(ctx));
        return {};
    }
}  // namespace ebmgen
