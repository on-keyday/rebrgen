/*license*/
#include "transform.hpp"
#include <memory>
#include <type_traits>
#include "../common.hpp"
#include "control_flow_graph.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include <vector>
#include <testutil/timer.h>

namespace ebmgen {

    expected<void> derive_property_setter_getter(TransformContext& ctx) {
        auto& all_stmts = ctx.statement_repository().get_all();
        for (auto& s : all_stmts) {
            if (auto prop = s.body.property_decl()) {
                if (prop->merge_mode == ebm::MergeMode::STRICT_TYPE) {
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
