/*license*/
#include "transform.hpp"
#include <functional>
#include <type_traits>
#include "../common.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/convert/helper.hpp"
#include "ebmgen/converter.hpp"
#include "wrap/cout.h"

namespace ebmgen {
    expected<void> vectorized_io(TransformContext& tctx, bool write) {
        // Implementation of the grouping I/O transformation
        auto& all_statements = tctx.statement_repository().get_all();
        auto current_added = all_statements.size();
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
                futils::wrap::cout_wrap() << "Read I/O groups: " << ios.size() << "\n";
                for (auto& g : ios) {
                    futils::wrap::cout_wrap() << "Group size: " << g.size() << "\n";
                    std::optional<std::uint64_t> all_in_byte = 0;
                    std::uint64_t all_in_bits = 0;
                    for (auto& ref : g) {
                        futils::wrap::cout_wrap() << "  - Statement ID: " << std::get<1>(ref).id.value() << "\n";
                        futils::wrap::cout_wrap() << "    - Size: " << std::get<2>(ref)->size.size()->value() << " " << to_string(std::get<2>(ref)->size.unit) << "\n";
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
                    futils::wrap::cout_wrap()
                        << "Total size: " << total_size.size()->value() << " " << to_string(total_size.unit) << "\n";
                    auto io_data = make_io_data({}, data_typ, {}, total_size);
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
                futils::wrap::cout_wrap() << "Adding vectorized I/O statement: " << new_stmt.id.value() << "\n";
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
        return {};
    }

    expected<void> remove_unused(TransformContext& ctx) {}

    expected<void> transform(TransformContext& ctx) {
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
        return {};
    }
}  // namespace ebmgen
