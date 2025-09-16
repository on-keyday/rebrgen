/*license*/
#include "transform.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <type_traits>
#include "../common.hpp"
#include "bit_manipulator.hpp"
#include "control_flow_graph.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/convert/helper.hpp"
#include "ebmgen/converter.hpp"
#include <set>
#include <vector>
#include <testutil/timer.h>

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

    expected<void> derive_property_setter_getter(TransformContext& ctx) {
        auto& all_stmts = ctx.statement_repository().get_all();
        for (auto& s : all_stmts) {
            if (auto prop = s.body.property_decl()) {
            }
        }
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
                if (get_id(start_ref)) {
                    MAYBE(start, flatten_expression(tctx, block, start_ref));
                    start_ref = start;
                }
                if (get_id(end_ref)) {
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

    // TODO: make this work or remove this
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
            auto found_cfg = ctx.cfg_map.find(get_id(stmt.id));
            if (found_cfg != ctx.cfg_map.end()) {
                cfg_count++;
                print_if_verbose("Found ", found_cfg->second->prev.size(), " previous node for ", get_id(stmt.id), "(", to_string(stmt.body.kind), ")\n");
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
                    if (!is_nil(val)) {
                        toplevel_expressions.insert(get_id(val));
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
                print_if_verbose("Flatten expression ", expr, "(", expr_ptr ? to_string(expr_ptr->body.kind) : "<unknown>", ")", " into ", flattened.container.size(), " statements and mapped to ", get_id(new_ref), "(", mapped_ptr ? to_string(mapped_ptr->body.kind) : "<unknown>", ")\n");
                for (auto& flt : flattened.container) {
                    auto stmt_ptr = ctx.statement_repository().get(flt);
                    print_if_verbose("  - Statement ID: ", get_id(flt), "(", stmt_ptr ? to_string(stmt_ptr->body.kind) : "<unknown>", ")\n");
                }
                flattened_expressions.emplace_back(std::move(flattened), new_ref);
            }
            else if (expr != get_id(new_ref)) {
                auto expr_ptr = ctx.expression_repository().get(ebm::ExpressionRef{expr});
                auto mapped_ptr = ctx.expression_repository().get(new_ref);
                print_if_verbose("Flatten expression ", expr, "(", expr_ptr ? to_string(expr_ptr->body.kind) : "<unknown>", ")", " into no statements but mapped to ", get_id(new_ref), "(", mapped_ptr ? to_string(mapped_ptr->body.kind) : "<unknown>", ")\n");
            }
        }
        print_if_verbose("Total flattened expressions: ", flattened_expressions.size(), "\n");
        */
        return {};
    }

    expected<CFGList> transform(TransformContext& ctx, bool debug) {
        // internal CFG used optimization
        {
            CFGContext cfg_ctx{ctx};
            MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
            MAYBE_VOID(bit_io_read, lowered_dynamic_bit_io(cfg_ctx, false));
            MAYBE_VOID(bit_io_write, lowered_dynamic_bit_io(cfg_ctx, true));
            MAYBE_VOID(insertion_point, detect_insertion_point(cfg_ctx));
        }
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
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
