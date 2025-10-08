#include "core/ast/node/statement.h"
#include "../converter.hpp"
#include "core/ast/ast.h"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "core/ast/node/type.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>
#include <memory>
#include <vector>

namespace ebmgen {

    expected<std::pair<size_t, bool>> get_integral_size_and_sign(ConverterContext& ctx, ebm::TypeRef type) {
        for (;;) {
            auto type_ref = ctx.repository().get_type(type);
            if (!type_ref) {
                return unexpect_error("Invalid type reference for max value");
            }
            if (type_ref->body.kind == ebm::TypeKind::INT || type_ref->body.kind == ebm::TypeKind::UINT) {
                return std::make_pair(type_ref->body.size()->value(), type_ref->body.kind == ebm::TypeKind::INT);
            }
            else if (type_ref->body.kind == ebm::TypeKind::ENUM) {
                auto base_type = type_ref->body.base_type();
                if (!base_type || base_type->id.value() == 0) {
                    return unexpect_error("Enum type must have a base type");
                }
                type = *base_type;
            }
            else {
                return unexpect_error("Unsupported type for max value: {}", to_string(type_ref->body.kind));
            }
        }
    }

    expected<ebm::ExpressionRef> get_max_value_expr(ConverterContext& ctx, ebm::TypeRef type) {
        MAYBE(size_and_signed, get_integral_size_and_sign(ctx, type));
        auto [size, is_signed] = size_and_signed;
        EBMU_UINT_TYPE(value_type, size);
        EBM_DEFAULT_VALUE(zero, value_type);
        EBM_UNARY_OP(max_unsigned, ebm::UnaryOp::bit_not, value_type, zero);
        auto result = max_unsigned;
        if (is_signed) {
            EBMU_INT_LITERAL(one, 1);
            EBM_BINARY_OP(max_signed, ebm::BinaryOp::right_shift, value_type, max_unsigned, one);
            result = max_signed;
        }
        EBM_CAST(max_value_lowered, type, value_type, result);

        EBM_MAX_VALUE(max_value_expr, type, max_value_lowered);
        return max_value_expr;
    }

    expected<ebm::StatementBody> StatementConverter::convert_loop_body(const std::shared_ptr<ast::Loop>& node) {
        ebm::LoopStatement result_loop_stmt;
        std::optional<ebm::StatementRef> init_v, step_v;
        std::optional<ebm::ExpressionRef> cond_v;
        if (node->init) {
            auto make_init_visited = [&](ebm::StatementRef ident_def) {
                const auto _scope = ctx.state().set_current_generate_type(GenerateType::Normal);
                ctx.state().add_visited_node(node->init, ident_def);
            };
            if (auto bop = ast::as<ast::Binary>(node->init);
                bop && bop->op == ast::BinaryOp::in_assign) {  // `for x in y`
                result_loop_stmt.loop_type = ebm::LoopType::FOR_EACH;
                auto ident = ast::as<ast::Ident>(bop->left);
                if (!ident) {
                    return unexpect_error("Invalid loop init target :{}", node_type_to_string(bop->left->node_type));
                }
                EBMA_CONVERT_EXPRESSION(target, bop->right);
                EBMA_ADD_IDENTIFIER(ident_ref, ident->ident);
                result_loop_stmt.collection(target);
                if (ast::as<ast::IntType>(bop->right->expr_type)) {
                    EBMA_CONVERT_TYPE(expr_type, bop->left->expr_type);
                    EBM_COUNTER_LOOP_START_CUSTOM(i, expr_type);
                    EBM_DEFINE_VARIABLE(identifier, ident_ref, expr_type, i, true, false);
                    make_init_visited(identifier_def);
                    EBMA_CONVERT_STATEMENT(inner_block_ref, node->body);
                    ebm::Block block;
                    block.container.reserve(2);
                    append(block, identifier_def);
                    append(block, inner_block_ref);
                    EBM_BLOCK(outer_block_ref, std::move(block));
                    EBMU_BOOL_TYPE(bool_type);
                    EBM_COUNTER_LOOP_END(lowered_loop, i, target, outer_block_ref);
                    result_loop_stmt.item_var(identifier_def);
                    result_loop_stmt.lowered_statement = ebm::LoweredStatementRef{lowered_loop};
                    result_loop_stmt.body = inner_block_ref;
                    result_loop_stmt.next_lowered_loop = ebm::LoweredStatementRef{lowered_loop};
                }
                else if (auto range = ast::as<ast::RangeType>(bop->right->expr_type)) {
                    auto l = range->range.lock();
                    ebm::ExpressionRef start, end;
                    EBMA_CONVERT_TYPE(base_type, range->base_type);
                    if (l->start) {
                        EBMA_CONVERT_EXPRESSION(s, l->start);
                        start = s;
                    }
                    else {
                        EBM_DEFAULT_VALUE(start_literal, base_type);
                        start = start_literal;
                    }
                    if (l->end) {
                        EBMA_CONVERT_EXPRESSION(e, l->end);
                        end = e;
                    }
                    else {
                        MAYBE(max_value_expr, get_max_value_expr(ctx, base_type));
                        end = max_value_expr;
                    }
                    MAYBE(size_and_signed, get_integral_size_and_sign(ctx, base_type));
                    auto [n, is_signed] = size_and_signed;
                    EBMU_COUNTER_TYPE(counter_type);
                    EBM_CAST(start_casted, counter_type, base_type, start);
                    EBM_CAST(end_casted, counter_type, base_type, end);

                    EBM_DEFINE_ANONYMOUS_VARIABLE(iter, counter_type, start_casted);
                    EBMU_BOOL_TYPE(bool_type);
                    EBM_BINARY_OP(cond, l->op == ast::BinaryOp::range_inclusive ? ebm::BinaryOp::less_or_eq : ebm::BinaryOp::less, bool_type, iter, end_casted);

                    EBM_DEFINE_VARIABLE(identifier, ident_ref, counter_type, iter, true, false);
                    make_init_visited(identifier_def);
                    EBMA_CONVERT_STATEMENT(inner_body, node->body);
                    ebm::Block body;
                    body.container.reserve(2);
                    append(body, identifier_def);
                    append(body, inner_body);
                    EBM_BLOCK(outer_block_ref, std::move(body));

                    EBM_INCREMENT(inc, iter, counter_type);

                    EBM_FOR_LOOP(loop_stmt, iter_def, cond, inc, outer_block_ref);

                    result_loop_stmt.item_var(identifier_def);
                    result_loop_stmt.lowered_statement = ebm::LoweredStatementRef{loop_stmt};
                    result_loop_stmt.body = inner_body;
                    result_loop_stmt.next_lowered_loop = ebm::LoweredStatementRef{loop_stmt};
                }
                else if (ast::as<ast::ArrayType>(bop->right->expr_type)) {
                    EBM_ARRAY_SIZE(array_size, target);
                    EBMA_CONVERT_TYPE(element_type, bop->left->expr_type);
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(indexed, element_type, target, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, element_type, indexed, false, true);
                    make_init_visited(element_def);
                    EBMA_CONVERT_STATEMENT(inner_block_ref, node->body);

                    ebm::Block outer_block;
                    outer_block.container.reserve(2);
                    append(outer_block, element_def);
                    append(outer_block, inner_block_ref);
                    EBM_BLOCK(outer_block_ref, std::move(outer_block));

                    EBM_COUNTER_LOOP_END(loop_stmt, i, array_size, outer_block_ref);
                    result_loop_stmt.item_var(element_def);
                    result_loop_stmt.lowered_statement = ebm::LoweredStatementRef{loop_stmt};
                    result_loop_stmt.body = inner_block_ref;
                    result_loop_stmt.next_lowered_loop = ebm::LoweredStatementRef{loop_stmt};
                }
                else if (auto lit = ast::as<ast::StrLiteral>(bop->right)) {
                    // note: representation of string is encoded as base64 in bop->right->binary_value because
                    //       src2json generates AST as json
                    MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(bop->right)));
                    EBMU_UINT_TYPE(u8_t, 8);
                    EBMU_U8_N_ARRAY(u8_n_array, candidate.size());
                    EBM_DEFAULT_VALUE(new_obj_ref, u8_n_array);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);
                    ebm::Block block;
                    block.container.reserve(2 + candidate.size());
                    append(block, buffer_def);
                    MAYBE_VOID(ok, construct_string_array(ctx, block, buffer, candidate));
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(array_index, u8_t, buffer, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, u8_t, array_index, true, true);
                    make_init_visited(element_def);
                    EBMA_CONVERT_STATEMENT(inner_block_ref, node->body);

                    ebm::Block outer_block;
                    outer_block.container.reserve(2);
                    append(outer_block, element_def);
                    append(outer_block, inner_block_ref);
                    EBM_BLOCK(outer_block_ref, std::move(outer_block));

                    EBMU_INT_LITERAL(len, candidate.size());
                    EBM_COUNTER_LOOP_END(loop_stmt, i, len, outer_block_ref);
                    append(block, loop_stmt);
                    EBM_BLOCK(block_ref, std::move(block));
                    result_loop_stmt.item_var(element_def);
                    result_loop_stmt.lowered_statement = ebm::LoweredStatementRef{block_ref};
                    result_loop_stmt.body = inner_block_ref;
                    result_loop_stmt.next_lowered_loop = ebm::LoweredStatementRef{loop_stmt};
                }
                else {
                    return unexpect_error("Invalid loop init type : {}", node_type_to_string(bop->right->expr_type->node_type));
                }
                return make_loop(std::move(result_loop_stmt));
            }
            result_loop_stmt.loop_type = ebm::LoopType::FOR;  // C-like for loop (e.g., for (int i = 0; i < n; i++))
            EBMA_CONVERT_STATEMENT(init_ref, node->init);
            init_v = init_ref;
        }
        if (node->cond) {
            EBMA_CONVERT_EXPRESSION(cond_ref, node->cond);
            cond_v = cond_ref;
        }
        EBMA_CONVERT_STATEMENT(body, node->body);
        if (node->step) {
            EBMA_CONVERT_STATEMENT(step_ref, node->step);
            step_v = step_ref;
        }
        if (init_v || step_v) {
            result_loop_stmt.loop_type = ebm::LoopType::FOR;  // C-like for loop (e.g., for (int i = 0; i < n; i++))
            if (init_v) {
                result_loop_stmt.init(*init_v);
            }
            if (cond_v) {
                result_loop_stmt.condition(make_condition(*cond_v));
            }
            if (step_v) {
                result_loop_stmt.increment(*step_v);
            }
        }
        else if (cond_v) {
            result_loop_stmt.loop_type = ebm::LoopType::WHILE;  // While loop
            result_loop_stmt.condition(make_condition(*cond_v));
        }
        else {
            result_loop_stmt.loop_type = ebm::LoopType::INFINITE;  // Infinite loop
        }
        result_loop_stmt.body = body;
        return make_loop(std::move(result_loop_stmt));
    }

    expected<ebm::StatementRef> StatementConverter::convert_statement(const std::shared_ptr<ast::Node>& node) {
        if (auto it = ctx.state().is_visited(node)) {
            return *it;
        }
        MAYBE(new_ref, ctx.repository().new_statement_id());
        ctx.state().add_visited_node(node, new_ref);
        return convert_statement(new_ref, node);
    }

    expected<ebm::StatementRef> StatementConverter::convert_statement(ebm::StatementRef new_id, const std::shared_ptr<ast::Node>& node) {
        ebm::StatementBody body;
        expected<void> result = unexpect_error("Unsupported statement type: {}", node_type_to_string(node->node_type));

        brgen::ast::visit(node, [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            result = convert_statement_impl(n, new_id, body);
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        EBMA_ADD_STATEMENT(ref, new_id, std::move(body));
        ctx.repository().add_debug_loc(node->loc, new_id);
        return ref;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Assert>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        EBMA_CONVERT_EXPRESSION(cond, node->cond);
        MAYBE(body_, assert_statement_body(ctx, cond));
        body = std::move(body_);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Return>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::RETURN;
        if (node->expr) {
            EBMA_CONVERT_EXPRESSION(expr_ref, node->expr);
            body.value(expr_ref);
        }
        else {
            body.value(ebm::ExpressionRef{});
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Break>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        MAYBE(loop_id, ctx.state().get_current_loop_id());
        body = make_break(loop_id);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Continue>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        MAYBE(loop_id, ctx.state().get_current_loop_id());
        body = make_continue(loop_id);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::If>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::IF_STATEMENT;
        EBMA_CONVERT_EXPRESSION(cond_ref, node->cond->expr);

        // Convert then block
        EBMA_CONVERT_STATEMENT(then_block, node->then);

        // Convert else block
        ebm::StatementRef else_block;
        if (node->els) {
            EBMA_CONVERT_STATEMENT(else_block_, node->els);
            else_block = else_block_;
        }

        body = make_if_statement(cond_ref, then_block, else_block);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Loop>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        auto loop_ = ctx.state().set_current_loop_id(id);
        MAYBE(loop_body, convert_loop_body(node));
        body = std::move(loop_body);
        return {};
    }

    expected<void> StatementConverter::derive_match_lowered_if(ebm::MatchStatement& match_stmt, bool trial_match) {
        std::vector<std::pair<ebm::StatementRef, ebm::IfStatement>> lowered_if;
        for (auto& b : match_stmt.branches.container) {
            auto if_cond = ctx.repository().get_statement(b)->body.match_branch();
            auto cond_expr = ctx.repository().get_expression(if_cond->condition.cond);
            // any range -> else block
            if (cond_expr->body.kind == ebm::ExpressionKind::RANGE &&
                is_nil(*cond_expr->body.start()) &&
                is_nil(*cond_expr->body.end())) {
                if (lowered_if.size()) {
                    lowered_if.back().second.else_block = if_cond->body;
                    break;
                }
                if (trial_match) {
                    return unexpect_error("Trial match is not supported yet");
                }
                match_stmt.lowered_if_statement = ebm::LoweredStatementRef{if_cond->body};
                break;
            }
            ebm::IfStatement tmp_if;
            tmp_if.then_block = if_cond->body;
            if (is_nil(match_stmt.target)) {
                tmp_if.condition = if_cond->condition;
            }
            else {
                EBMU_BOOL_TYPE(bool_type);
                MAYBE(equal, ctx.get_expression_converter().convert_equal(match_stmt.target, if_cond->condition.cond));
                tmp_if.condition = make_condition(equal);
            }
            MAYBE(if_id, ctx.repository().new_statement_id());
            if (lowered_if.size()) {
                lowered_if.back().second.else_block = if_id;
            }
            lowered_if.push_back({if_id, std::move(tmp_if)});
        }
        for (auto& [if_id, if_stmt] : lowered_if) {
            if (is_nil(match_stmt.lowered_if_statement.id)) {
                match_stmt.lowered_if_statement = ebm::LoweredStatementRef{if_id};
            }
            ebm::StatementBody if_body;
            if_body.kind = ebm::StatementKind::IF_STATEMENT;
            if_body.if_statement(std::move(if_stmt));
            EBMA_ADD_STATEMENT(new_stmt_id, if_id, std::move(if_body));
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Match>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::MATCH_STATEMENT;
        ebm::MatchStatement match_stmt;
        if (node->cond) {
            EBMA_CONVERT_EXPRESSION(target_ref, node->cond->expr);
            match_stmt.target = target_ref;
        }
        match_stmt.is_exhaustive(node->struct_union_type->exhaustive);

        for (auto& branch : node->branch) {
            EBMA_CONVERT_STATEMENT(branch_ref, branch);
            append(match_stmt.branches, branch_ref);
        }

        MAYBE_VOID(derived, derive_match_lowered_if(match_stmt, node->trial_match));

        body.match_statement(std::move(match_stmt));
        return {};
    }

    expected<std::optional<ebm::StatementRef>> handle_variant_alternative(ConverterContext& ctx, ebm::TypeRef alt_type, ebm::InitCheckType typ) {
        MAYBE(struct_type, ctx.repository().get_type(alt_type));
        MAYBE(base_struct_id, struct_type.body.id());
        MAYBE(base_struct, ctx.repository().get_statement(base_struct_id));
        MAYBE(struct_decl, base_struct.body.struct_decl());
        MAYBE(related_variant, ctx.repository().get_type(struct_decl.related_variant));
        MAYBE(related_field, related_variant.body.related_field());
        ebm::InitCheck check;
        check.init_check_type = typ;
        check.target_field = related_field;
        check.expect_type = alt_type;
        ebm::StatementBody init_check;
        init_check.kind = ebm::StatementKind::INIT_CHECK;
        init_check.init_check(std::move(check));
        EBMA_ADD_STATEMENT(init_check_ref, std::move(init_check));
        return init_check_ref;
    }

    expected<std::optional<ebm::StatementRef>> handle_variant_alternative(ConverterContext& ctx, const std::shared_ptr<ast::StructType>& s) {
        if (ctx.state().get_current_generate_type() == ebm::GenerateType::Normal) {
            return std::nullopt;
        }
        auto alt_type = ctx.state().get_cached_type(s);
        if (is_nil(alt_type)) {
            return std::nullopt;
        }
        return handle_variant_alternative(ctx, alt_type, ctx.state().get_current_generate_type() == ebm::GenerateType::Encode ? ebm::InitCheckType::encode : ebm::InitCheckType::decode);
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::IndentBlock>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::BLOCK;
        ebm::Block block_body;
        const auto _scope = ctx.state().set_current_block(&block_body);
        MAYBE(variant_alt, handle_variant_alternative(ctx, node->struct_type));
        if (variant_alt) {
            append(block_body, *variant_alt);
        }
        for (auto& element : node->elements) {
            EBMA_CONVERT_STATEMENT(stmt_ref, element);
            append(block_body, stmt_ref);
        }
        body.block(std::move(block_body));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::MatchBranch>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::MATCH_BRANCH;
        ebm::MatchBranch ebm_branch;
        EBMA_CONVERT_EXPRESSION(cond_ref, node->cond->expr);
        ebm_branch.condition = make_condition(cond_ref);

        ebm::StatementRef branch_body_block;
        if (node->then) {
            EBMA_CONVERT_STATEMENT(stmt_ref, node->then);
            branch_body_block = stmt_ref;
        }
        ebm_branch.body = branch_body_block;
        body.match_branch(std::move(ebm_branch));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Program>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::PROGRAM_DECL;

        ebm::Block program_body_block;
        const auto _scope = ctx.state().set_current_block(&program_body_block);
        for (auto& p : node->elements) {
            EBMA_CONVERT_STATEMENT(stmt_ref, p);
            append(program_body_block, stmt_ref);
        }
        body.block(std::move(program_body_block));
        return {};
    }

    expected<ebm::TypeRef> get_coder_return(ConverterContext& ctx, bool enc) {
        ebm::TypeBody b;
        b.kind = enc ? ebm::TypeKind::ENCODER_RETURN : ebm::TypeKind::DECODER_RETURN;
        EBMA_ADD_TYPE(coder_return, std::move(b));
        return coder_return;
    }

    expected<ebm::TypeRef> get_coder_input(ConverterContext& ctx, bool enc) {
        ebm::TypeBody b;
        b.kind = enc ? ebm::TypeKind::ENCODER_INPUT : ebm::TypeKind::DECODER_INPUT;
        EBMA_ADD_TYPE(coder_input, std::move(b));
        return coder_input;
    }

    expected<ebm::StatementRef> StatementConverter::convert_struct_decl(const std::shared_ptr<ast::StructType>& node, ebm::TypeRef related_variant) {
        const auto _mode = ctx.state().set_current_generate_type(GenerateType::Normal);
        if (auto v = ctx.state().is_visited(node)) {
            return *v;
        }
        if (auto locked_base = node->base.lock()) {
            if (ast::as<ast::Format>(locked_base) || ast::as<ast::State>(locked_base)) {
                EBMA_CONVERT_STATEMENT(name_ref, locked_base);  // Convert the struct declaration
                ctx.state().add_visited_node(node, name_ref);
                return name_ref;
            }
            else if (ast::as<ast::MatchBranch>(locked_base) || ast::as<ast::If>(locked_base)) {
                ebm::StatementBody stmt;
                stmt.kind = ebm::StatementKind::STRUCT_DECL;
                MAYBE(name_ref, ctx.repository().new_statement_id());
                ctx.state().add_visited_node(node, name_ref);
                MAYBE(struct_decl, ctx.get_statement_converter().convert_struct_decl({}, node, related_variant));
                stmt.struct_decl(std::move(struct_decl));
                EBMA_ADD_STATEMENT(_, name_ref, std::move(stmt));
                return name_ref;
            }
            else {
                return unexpect_error("StructType base must be a Format or State :{}", node_type_to_string(locked_base->node_type));
            }
        }
        else {
            return unexpect_error("StructType has no base");
        }
    }

    expected<ebm::StructDecl> StatementConverter::convert_struct_decl(ebm::IdentifierRef name, const std::shared_ptr<ast::StructType>& node, ebm::TypeRef related_variant) {
        ebm::StructDecl struct_decl;
        struct_decl.name = name;
        struct_decl.is_recursive(node->recursive);
        struct_decl.related_variant = related_variant;
        {
            const auto _mode = ctx.state().set_current_generate_type(GenerateType::Normal);
            for (auto& element : node->fields) {
                if (ast::as<ast::Field>(element)) {
                    EBMA_CONVERT_STATEMENT(stmt_ref, element);
                    append(struct_decl.fields, stmt_ref);
                }
            }
        }
        return struct_decl;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Format>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        if (ctx.state().get_current_generate_type() != GenerateType::Normal) {
            return unexpect_error("unexpected node type: {}", to_string(ctx.state().get_current_generate_type()));
        }
        body.kind = ebm::StatementKind::STRUCT_DECL;
        EBMA_ADD_IDENTIFIER(name_ref, node->ident->ident);
        MAYBE(encoder_input, get_coder_input(ctx, true));
        MAYBE(decoder_input, get_coder_input(ctx, false));
        auto get_type = [&](std::shared_ptr<ast::Function> fn, GenerateType typ) -> expected<ebm::TypeRef> {
            ebm::TypeBody b;
            b.kind = typ == GenerateType::Encode ? ebm::TypeKind::ENCODER_RETURN : ebm::TypeKind::DECODER_RETURN;
            EBMA_ADD_TYPE(fn_return, std::move(b));
            if (fn) {
                MAYBE(fn_type_body, ctx.get_type_converter().convert_function_type(fn->func_type));
                fn_type_body.return_type(fn_return);
                b = std::move(fn_type_body);
            }
            else {
                b.kind = ebm::TypeKind::FUNCTION;
                b.return_type(fn_return);
            }
            auto& param = *b.params();
            param.len = varint(param.len.value() + 1).value();
            param.container.insert(param.container.begin(), typ == GenerateType::Encode ? encoder_input : decoder_input);
            EBMA_ADD_TYPE(fn_typ, std::move(b));
            return fn_typ;
        };
        MAYBE(enc_id, ctx.repository().new_statement_id());
        MAYBE(dec_id, ctx.repository().new_statement_id());
        MAYBE(enc_type, get_type(node->encode_fn.lock(), GenerateType::Encode));
        MAYBE(dec_type, get_type(node->decode_fn.lock(), GenerateType::Decode));
        EBM_IDENTIFIER(encode, enc_id, enc_type);
        EBM_IDENTIFIER(decode, dec_id, dec_type);
        MAYBE(struct_decl, ctx.get_statement_converter().convert_struct_decl(name_ref, node->body->struct_type));

        EBM_DEFINE_PARAMETER(writer, {}, encoder_input, false);
        EBM_DEFINE_PARAMETER(reader, {}, decoder_input, false);
        // TODO: strictly analyze state variable usage in ast
        std::vector<std::pair<ebm::ExpressionRef, ebm::StatementRef>> state_vars;
        for (auto& v : node->state_variables) {
            auto locked = v.lock();
            EBMA_CONVERT_TYPE(var_type, locked->field_type);
            EBMA_ADD_IDENTIFIER(var_name, locked->ident->ident);
            EBM_DEFINE_PARAMETER(var, var_name, var_type, true);
            state_vars.emplace_back(var, var_def);
        }

        ctx.state().add_format_encode_decode(node, encode, enc_type, writer, writer_def,
                                             decode, dec_type, reader, reader_def,
                                             state_vars);
        const auto _node = ctx.state().set_current_node(node);
        auto handle = [&](ebm::StatementRef fn_ref, std::shared_ptr<ast::Function> fn, ebm::StatementRef coder_input, GenerateType typ) -> expected<void> {
            const auto _mode = ctx.state().set_current_generate_type(typ);
            ebm::FunctionDecl derived_fn;
            if (fn) {
                MAYBE(decl, ctx.get_statement_converter().convert_function_decl(fn, typ, coder_input));
                for (auto& st : state_vars) {
                    append(decl.params, st.second);
                }
                derived_fn = std::move(decl);
            }
            else {
                derived_fn.parent_format = id;
                EBMA_ADD_IDENTIFIER(enc_name, typ == GenerateType::Encode ? "encode" : "decode");
                MAYBE(coder_return, get_coder_return(ctx, typ == GenerateType::Encode));
                derived_fn.name = enc_name;
                derived_fn.return_type = coder_return;
                append(derived_fn.params, coder_input);
                for (auto& st : state_vars) {
                    append(derived_fn.params, st.second);
                }
                EBMA_CONVERT_STATEMENT(body, node->body);
                derived_fn.body = body;
            }
            ebm::StatementBody b;
            b.kind = ebm::StatementKind::FUNCTION_DECL;
            b.func_decl(std::move(derived_fn));
            EBMA_ADD_STATEMENT(stmt, fn_ref, std::move(b));
            fn_ref = stmt;
            return {};
        };
        MAYBE_VOID(ok1, handle(enc_id, node->encode_fn.lock(), writer_def, GenerateType::Encode));
        MAYBE_VOID(ok2, handle(dec_id, node->decode_fn.lock(), reader_def, GenerateType::Decode));
        struct_decl.encode_fn = enc_id;
        struct_decl.decode_fn = dec_id;
        body.struct_decl(std::move(struct_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Enum>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::ENUM_DECL;
        ebm::EnumDecl ebm_enum_decl;
        EBMA_ADD_IDENTIFIER(name_ref, node->ident->ident);
        ebm_enum_decl.name = name_ref;
        if (node->base_type) {
            EBMA_CONVERT_TYPE(base_type_ref, node->base_type);
            ebm_enum_decl.base_type = base_type_ref;
        }
        for (auto& member : node->members) {
            EBMA_CONVERT_STATEMENT(ebm_enum_member_ref, member);
            append(ebm_enum_decl.members, ebm_enum_member_ref);
        }
        body.enum_decl(std::move(ebm_enum_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::EnumMember>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        ebm::EnumMemberDecl ebm_enum_member_decl;
        EBMA_ADD_IDENTIFIER(member_name_ref, node->ident->ident);
        ebm_enum_member_decl.name = member_name_ref;
        if (node->value) {
            EBMA_CONVERT_EXPRESSION(value_expr_ref, node->value);
            ebm_enum_member_decl.value = value_expr_ref;
        }
        if (node->str_literal) {
            EBMA_ADD_STRING(str_ref, node->str_literal->value);
            ebm_enum_member_decl.string_repr = str_ref;
        }
        body.kind = ebm::StatementKind::ENUM_MEMBER_DECL;
        body.enum_member_decl(std::move(ebm_enum_member_decl));
        return {};
    }

    expected<ebm::FunctionDecl> StatementConverter::convert_function_decl(const std::shared_ptr<ast::Function>& node, GenerateType typ, ebm::StatementRef coder_input_ref) {
        ebm::FunctionDecl func_decl;
        if (auto parent = node->belong.lock()) {
            auto n = ctx.state().set_current_generate_type(GenerateType::Normal);
            EBMA_CONVERT_STATEMENT(parent_ref, parent);
            func_decl.parent_format = parent_ref;
        }
        EBMA_ADD_IDENTIFIER(name_ref, node->ident->ident);
        func_decl.name = name_ref;
        if (auto typ = ctx.state().get_current_generate_type();
            typ == GenerateType::Encode || typ == GenerateType::Decode) {
            MAYBE(coder_return, get_coder_return(ctx, typ == GenerateType::Encode));
            func_decl.return_type = coder_return;
            append(func_decl.params, coder_input_ref);
        }
        else {
            if (node->return_type) {
                EBMA_CONVERT_TYPE(return_type_ref, node->return_type);
                func_decl.return_type = return_type_ref;
            }
            else {
                EBMU_VOID_TYPE(void_);
                func_decl.return_type = void_;
            }
        }
        for (auto& param : node->parameters) {
            EBMA_ADD_IDENTIFIER(param_name_ref, param->ident->ident);
            EBMA_CONVERT_TYPE(param_type_ref, param->field_type);
            EBM_DEFINE_PARAMETER(param_ref, param_name_ref, param_type_ref, false);
            append(func_decl.params, param_ref_def);
        }
        const auto _mode = ctx.state().set_current_generate_type(typ);
        EBMA_CONVERT_STATEMENT(fn_body, node->body);
        func_decl.body = fn_body;
        return func_decl;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Function>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::FUNCTION_DECL;
        MAYBE(decl, convert_function_decl(node, GenerateType::Normal, {}));
        body.func_decl(std::move(decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Metadata>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::METADATA;
        ebm::Metadata ebm_metadata;
        EBMA_ADD_IDENTIFIER(name_ref, node->name);
        ebm_metadata.name = name_ref;
        for (auto& value : node->values) {
            EBMA_CONVERT_EXPRESSION(value_expr_ref, value);
            append(ebm_metadata.values, value_expr_ref);
        }
        body.metadata(std::move(ebm_metadata));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ScopedStatement>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        MAYBE(variant_alt, handle_variant_alternative(ctx, node->struct_type));
        if (variant_alt) {
            ebm::Block block_body;
            append(block_body, *variant_alt);
            EBMA_CONVERT_STATEMENT(stmt_ref, node->statement);
            append(block_body, stmt_ref);
            body.kind = ebm::StatementKind::BLOCK;
            body.block(std::move(block_body));
            return {};
        }
        else {
            expected<void> result;
            ast::visit(node->statement, [&](auto&& n) {
                using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node->statement)>, std::shared_ptr>::template param_at<0>;
                result = convert_statement_impl(n, id, body);
            });
            return result;
        }
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::State>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::STRUCT_DECL;
        ebm::StructDecl state_decl;
        EBMA_ADD_IDENTIFIER(name_ref, node->ident->ident);
        state_decl.name = name_ref;
        ebm::Block state_body_block;
        for (auto& element : node->body->elements) {
            EBMA_CONVERT_STATEMENT(stmt_ref, element);
            append(state_body_block, stmt_ref);
        }
        state_decl.fields = std::move(state_body_block);
        body.struct_decl(std::move(state_decl));
        return {};
    }

    expected<ebm::StatementBody> with_io_changed(ConverterContext& ctx, ebm::StatementRef* parent_io_def, ebm::ExpressionRef sub_byte_io, ebm::StatementRef sub_byte_io_def, bool is_enc, auto&& do_io) {
        // this changed io_.[encoder|decoder]_input[def] are used in [encode|decode]_field_type
        MAYBE(io_, ctx.state().get_format_encode_decode(ctx.state().get_current_node()));
        auto& original = (is_enc ? io_.encoder_input : io_.decoder_input);
        auto& original_def = (is_enc ? io_.encoder_input_def : io_.decoder_input_def);
        auto io_changed = futils::helper::defer([&, original, original_def] {
            (is_enc ? io_.encoder_input : io_.decoder_input) = original;
            (is_enc ? io_.encoder_input_def : io_.decoder_input_def) = original_def;
        });
        *parent_io_def = original_def;
        original = sub_byte_io;
        original_def = sub_byte_io_def;
        return do_io();
    }

    expected<ebm::StatementBody> convert_field_serialize(ConverterContext& ctx, const std::shared_ptr<ast::Field>& node, ebm::StatementRef id) {
        ebm::StatementBody body;
        const bool is_enc = ctx.state().get_current_generate_type() == GenerateType::Encode;
        MAYBE(def_ref, ctx.state().is_visited(node, GenerateType::Normal));
        auto def = ctx.repository().get_statement(def_ref)->body.field_decl();
        EBM_IDENTIFIER(def_id, def_ref, def->field_type);
        std::optional<ebm::StatementRef> assert_stmt;
        std::optional<ebm::SubByteRange> sub_range;
        ebm::StatementRef sub_range_id = id;
        auto do_io = [&] -> expected<ebm::StatementBody> {
            if (is_enc) {
                MAYBE(body, ctx.get_encoder_converter().encode_field_type(node->field_type, def_id, node));
                return body;
            }
            else {
                MAYBE(body, ctx.get_decoder_converter().decode_field_type(node->field_type, def_id, node));
                return body;
            }
        };
        if (node->arguments && node->arguments->arguments.size()) {
            if (node->arguments->arguments.size() != 1) {
                return unexpect_error("Currently field argument must be 1");
            }
            EBMA_CONVERT_EXPRESSION(cond, node->arguments->arguments[0]);
            EBMU_BOOL_TYPE(bool_type);
            MAYBE(eq, ctx.get_expression_converter().convert_equal(def_id, cond));
            MAYBE(assert_, assert_statement(ctx, eq));
            assert_stmt = assert_;
        }
        if (node->arguments && (node->arguments->sub_byte_length || node->arguments->sub_byte_expr)) {
            if (assert_stmt) {  // outer statement exists, so use its own id instead
                MAYBE(own_id, ctx.repository().new_statement_id());
                sub_range_id = own_id;
            }
            auto sr = ebm::SubByteRange{.range_type = ebm::SubByteRangeType::bytes};
            sr.stream_type = is_enc ? ebm::StreamType::OUTPUT : ebm::StreamType::INPUT;
            if (node->arguments->sub_byte_length) {
                EBMA_CONVERT_EXPRESSION(len, node->arguments->sub_byte_length);
                if (node->arguments->sub_byte_begin) {
                    sr.range_type = ebm::SubByteRangeType::seek_bytes;
                    EBMA_CONVERT_EXPRESSION(offset, node->arguments->sub_byte_begin);
                    sr.offset(offset);
                }
                sr.length(len);
            }
            else {
                sr.range_type = ebm::SubByteRangeType::expression;
                EBMA_CONVERT_EXPRESSION(data, node->arguments->sub_byte_expr);
                sr.expression(data);
            }
            MAYBE(input_typ, get_coder_input(ctx, is_enc));
            EBM_SUB_RANGE_INIT(init, input_typ, sub_range_id);
            EBM_DEFINE_ANONYMOUS_VARIABLE(sub_byte_io, input_typ, init);
            sr.io_ref = sub_byte_io_def;
            sub_range = std::move(sr);
            MAYBE(subrange_body, with_io_changed(ctx, &sub_range->parent_io_ref, sub_byte_io, sub_byte_io_def, is_enc, do_io));
            EBMA_ADD_STATEMENT(io_stmt, std::move(subrange_body));
            sub_range->io_statement = io_stmt;
            body.kind = ebm::StatementKind::SUB_BYTE_RANGE;
            body.sub_byte_range(std::move(*sub_range));
        }
        else {
            MAYBE(body_, do_io());
            body = std::move(body_);
        }
        if (assert_stmt) {
            ebm::StatementRef inner;
            if (body.kind == ebm::StatementKind::SUB_BYTE_RANGE) {
                EBMA_ADD_STATEMENT(inner_, sub_range_id, std::move(body));
                inner = inner_;
            }
            else {
                EBMA_ADD_STATEMENT(inner_, std::move(body));
                inner = inner_;
            }
            ebm::Block with_assert;
            if (is_enc) {
                append(with_assert, *assert_stmt);
                append(with_assert, inner);
            }
            else {
                append(with_assert, inner);
                append(with_assert, *assert_stmt);
            }
            body.kind = ebm::StatementKind::BLOCK;
            body.block(std::move(with_assert));
        }
        return body;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Field>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        if (auto union_type = ast::as<ast::UnionType>(node->field_type)) {
            if (ctx.state().get_current_generate_type() != GenerateType::Normal) {
                return unexpect_error("Property declaration is only allowed in normal generate type");
            }
            MAYBE(body_, convert_property_decl(node));
            body = std::move(body_);
        }
        else if (!node->is_state_variable && ctx.state().get_current_generate_type() != GenerateType::Normal) {
            MAYBE(body_, convert_field_serialize(ctx, node, id));
            body = std::move(body_);
        }
        else {
            body.kind = ebm::StatementKind::FIELD_DECL;
            ebm::FieldDecl field_decl;
            if (node->ident) {
                EBMA_ADD_IDENTIFIER(field_name_ref, node->ident->ident);
                field_decl.name = field_name_ref;
            }
            EBMA_CONVERT_TYPE(type_ref, node->field_type, node);
            field_decl.field_type = type_ref;
            field_decl.is_state_variable(node->is_state_variable);
            if (auto locked = node->belong_struct.lock(); locked && node->belong.lock()) {
                MAYBE(parent_member_ref, ctx.get_statement_converter().convert_struct_decl(locked));
                field_decl.parent_struct = parent_member_ref;
            }
            body.field_decl(std::move(field_decl));
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ExplicitError>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::ERROR_REPORT;
        ebm::ErrorReport error_report;
        MAYBE(decoded, decode_base64(node->message));
        EBMA_ADD_STRING(message_str_ref, decoded);
        error_report.message = message_str_ref;
        for (auto& arg : node->base->arguments) {
            EBMA_CONVERT_EXPRESSION(arg_expr_ref, arg);
            append(error_report.arguments, arg_expr_ref);
        }
        body.error_report(std::move(error_report));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Import>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementKind::IMPORT_MODULE;
        EBMA_ADD_IDENTIFIER(module_name_ref, node->path);
        body.module_name(module_name_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ImplicitYield>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        MAYBE(yield_stmt, ctx.state().get_current_yield_statement());
        MAYBE(stmt, ctx.repository().get_statement(yield_stmt));
        MAYBE(var_decl, stmt.body.var_decl());
        body.kind = ebm::StatementKind::YIELD;
        EBM_IDENTIFIER(temp_var, yield_stmt, var_decl.var_type);
        body.target(temp_var);
        EBMA_CONVERT_EXPRESSION(expr_ref, node->expr);
        body.value(expr_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Binary>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        auto assign_with_op = convert_assignment_binary_op(node->op);
        if (node->op == ast::BinaryOp::assign) {
            body.kind = ebm::StatementKind::ASSIGNMENT;
            EBMA_CONVERT_EXPRESSION(target_ref, node->left);
            body.target(target_ref);
            EBMA_CONVERT_EXPRESSION(value_ref, node->right);
            body.value(value_ref);
        }
        else if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            body.kind = ebm::StatementKind::VARIABLE_DECL;
            ebm::VariableDecl var_decl;
            auto name_ident = ast::as<ast::Ident>(node->left);
            if (!name_ident) {
                return unexpect_error("Left-hand side of define_assign/const_assign must be an identifier");
            }
            EBMA_ADD_IDENTIFIER(name_ref, name_ident->ident);
            var_decl.name = name_ref;

            EBMA_CONVERT_TYPE(type_ref, node->left->expr_type);
            var_decl.var_type = type_ref;

            if (node->right) {
                EBMA_CONVERT_EXPRESSION(initial_value_ref, node->right);
                var_decl.initial_value = initial_value_ref;
            }
            var_decl.is_constant(node->op == ast::BinaryOp::const_assign);  // Set is_constant based on operator
            body.var_decl(std::move(var_decl));
        }
        else if (assign_with_op) {
            body.kind = ebm::StatementKind::ASSIGNMENT;
            EBMA_CONVERT_EXPRESSION(calc, ast::cast_to<ast::Expr>(node));
            EBMA_CONVERT_EXPRESSION(target_ref, node->left);
            body.target(target_ref);
            body.value(calc);
        }
        else {
            body.kind = ebm::StatementKind::EXPRESSION;
            EBMA_CONVERT_EXPRESSION(expr_ref, ast::cast_to<ast::Expr>(node));
            body.expression(expr_ref);
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Node>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        if (ast::as<ast::Expr>(node)) {
            body.kind = ebm::StatementKind::EXPRESSION;
            EBMA_CONVERT_EXPRESSION(expr_ref, ast::cast_to<ast::Expr>(node));
            body.expression(expr_ref);
            return {};
        }
        return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
    }

}  // namespace ebmgen
