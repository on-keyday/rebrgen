/*license*/
#include "transform.hpp"
#include <testutil/timer.h>
#include <queue>
#include "../convert/helper.hpp"
#include "bit_manipulator.hpp"
#include <set>

namespace ebmgen {

    struct Route {
        std::vector<std::shared_ptr<CFG>> route;
        size_t bit_size = 0;
    };

    auto get_io(ebm::Statement& stmt, bool write) {
        return write ? stmt.body.write_data() : stmt.body.read_data();
    }

    std::vector<Route> search_byte_aligned_route(CFGContext& tctx, const std::shared_ptr<CFG>& root, size_t root_size, bool write) {
        std::vector<Route> finalized_routes;
        std::queue<Route> candidates;
        candidates.push({
            .route = {root},
            .bit_size = root_size,
        });  // first route
        while (!candidates.empty()) {
            auto r = candidates.front();
            candidates.pop();
            for (auto& n : r.route.back()->next) {
                auto copy = r;
                copy.route.push_back(n);
                auto stmt = tctx.tctx.statement_repository().get(n->original_node);
                if (!stmt) {
                    continue;  // drop route
                }
                if (auto io_ = get_io(*stmt, write)) {
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
                candidates.push(std::move(copy));
            }
        }
        return finalized_routes;
    }

    expected<void> add_lowered_bit_io(CFGContext& tctx, ebm::StatementRef io_ref, std::vector<Route>& finalized_routes, bool write) {
        auto& ctx = tctx.tctx.context();
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
        EBMU_INT_LITERAL(zero, 0);
        EBM_DEFINE_ANONYMOUS_VARIABLE(current_bit_offset, count_t, zero);
        EBM_DEFINE_ANONYMOUS_VARIABLE(read_offset, count_t, zero);

        // added_bit = current_bit_offset + add_bit
        // new_size = (added_bit + 7) / 8
        // for read_offset < new_size:
        //    tmp_buffer[read_offset] = read u8
        //    read_offset++
        auto read_incremental = [&](ebm::ExpressionRef added_bit) -> expected<ebm::StatementRef> {
            EBMU_INT_LITERAL(seven, 7);
            EBMU_INT_LITERAL(eight, 8);
            EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, added_bit, seven);
            EBM_BINARY_OP(new_size, ebm::BinaryOp::div, count_t, new_size_bit, eight);
            EBM_BINARY_OP(condition, ebm::BinaryOp::less, bool_t, read_offset, new_size);
            ebm::Block block;
            {
                EBM_INDEX(indexed, u8_t, tmp_buffer, read_offset);
                auto data = make_io_data(io_ref, indexed, u8_t, {}, get_size(8));
                MAYBE(lowered, ctx.get_decoder_converter().decode_multi_byte_int_with_fixed_array(io_ref, 1, {}, indexed, u8_t));
                ebm::LoweredStatements lows;
                append(lows, make_lowered_statement(ebm::LoweringType::NAIVE, lowered));
                EBM_LOWERED_STATEMENTS(l, std::move(lows));
                data.lowered_statement = ebm::LoweredStatementRef{l};
                EBM_READ_DATA(read_to_temporary, std::move(data));
                append(block, read_to_temporary);
                EBM_INCREMENT(inc, read_offset, count_t);
                append(block, inc);
            }
            EBM_BLOCK(read, std::move(block));
            EBM_WHILE_LOOP(loop, condition, read);
            return loop;
        };
        auto flush_buffer = [&](ebm::ExpressionRef new_size_bit) -> expected<ebm::StatementRef> {
            EBMU_INT_LITERAL(eight, 8);
            EBM_BINARY_OP(div, ebm::BinaryOp::div, count_t, new_size_bit, eight);
            MAYBE(cur_offset, make_dynamic_size(div, ebm::SizeUnit::BYTE_DYNAMIC));
            auto write_buffer = make_io_data(io_ref, tmp_buffer, max_buffer_t, {}, cur_offset);
            EBM_WRITE_DATA(flush_buffer, std::move(write_buffer));
            return flush_buffer;
        };
        std::set<std::shared_ptr<CFG>> reached_route;
        for (auto& r : finalized_routes) {
            BitManipulator extractor(ctx, tmp_buffer, u8_t);
            for (size_t i = 0; i < r.route.size(); i++) {
                auto& c = r.route[i];
                if (reached_route.contains(c)) {
                    continue;
                }
                reached_route.insert(c);
                MAYBE(stmt, tctx.tctx.statement_repository().get(c->original_node));
                auto io_ = get_io(stmt, write);
                if (io_) {
                    auto io_copy = *io_;  // to avoid memory location movement
                    auto add_bit = io_copy.size.size()->value();
                    if (io_copy.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        add_bit *= 8;
                    }
                    EBMU_INT_LITERAL(bit_size, add_bit);
                    EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, current_bit_offset, bit_size);
                    EBM_ASSIGNMENT(update_current_bit_offset, current_bit_offset, new_size_bit);

                    EBMU_UINT_TYPE(unsigned_t, add_bit);
                    ebm::Block block;
                    if (i == 0) {
                        append(block, tmp_buffer_def);
                        append(block, current_bit_offset_def);
                    }
                    if (!write) {
                        MAYBE(io_cond, read_incremental(new_size_bit));
                        EBM_DEFAULT_VALUE(zero, unsigned_t);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_holder, unsigned_t, zero);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, tmp_holder);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, tmp_holder);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        EBM_CAST(casted, io_copy.data_type, unsigned_t, tmp_holder);
                        EBM_ASSIGNMENT(fin, io_copy.target, tmp_holder);
                        append(block, io_cond);
                        append(block, *assign);
                        append(block, fin);
                    }
                    else {
                        EBM_CAST(casted, unsigned_t, io_copy.data_type, io_copy.target);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, casted);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, casted);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        append(block, *assign);
                        if (i == r.route.size() - 1) {
                            MAYBE(flush, flush_buffer(new_size_bit));
                            append(block, flush);
                        }
                    }
                    append(block, update_current_bit_offset);
                    EBM_BLOCK(lowered_bit_operation, std::move(block));
                    if (!is_nil(io_copy.lowered_statement.id)) {
                        MAYBE(lowered_stmts, tctx.tctx.statement_repository().get(io_copy.lowered_statement.id));
                        MAYBE(stmts, lowered_stmts.body.lowered_statements());
                        append(stmts, make_lowered_statement(ebm::LoweringType::NAIVE, lowered_bit_operation));
                    }
                    else {
                        ebm::LoweredStatements block;
                        append(block, make_lowered_statement(ebm::LoweringType::NAIVE, lowered_bit_operation));
                        EBM_LOWERED_STATEMENTS(low, std::move(block))
                        MAYBE(stmt, tctx.tctx.statement_repository().get(c->original_node));  // refetch because memory is relocated
                        get_io(stmt, write)->lowered_statement = ebm::LoweredStatementRef{low};
                    }
                }
            }
        }
        return {};
    }

    expected<void> lowered_dynamic_bit_io(CFGContext& tctx, bool write) {
        auto& ctx = tctx.tctx.context();
        auto& all_statements = tctx.tctx.statement_repository().get_all();
        auto current_added = all_statements.size();

        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            for (auto& ref : block->container) {
                MAYBE(stmt, tctx.tctx.statement_repository().get(ref));
                if (auto r = get_io(stmt, write); r && r->size.unit == ebm::SizeUnit::BIT_FIXED) {
                    auto found = tctx.cfg_map.find(get_id(stmt.id));
                    if (found == tctx.cfg_map.end()) {
                        return unexpect_error("no cfg found for {}:{}", get_id(stmt.id), to_string(stmt.body.kind));
                    }
                    auto finalized_routes = search_byte_aligned_route(tctx, found->second, r->size.size()->value(), write);
                    if (finalized_routes.size()) {
                        MAYBE_VOID(added, add_lowered_bit_io(tctx, r->io_ref, finalized_routes, write));
                    }
                }
            }
        }
        return {};
    }

}  // namespace ebmgen