#include "../converter.hpp"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {

    expected<std::pair<size_t, bool>> get_integral_size_and_sign(ConverterContext& ctx, ebm::TypeRef type) {
        for (;;) {
            auto type_ref = ctx.get_type(type);
            if (!type_ref) {
                return unexpect_error("Invalid type reference for max value");
            }
            if (type_ref->body.kind == ebm::TypeKind::INT || type_ref->body.kind == ebm::TypeKind::UINT) {
                return std::make_pair(*type_ref->body.size(), type_ref->body.kind == ebm::TypeKind::INT);
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
        EBM_NEW_OBJECT(zero, value_type);
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
                    ctx.add_visited_node(node->init, identifier_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    EBMU_BOOL_TYPE(bool_type);
                    EBM_COUNTER_LOOP_END(lowered_loop, i, target, inner_block_ref);
                    result_loop_stmt.item_var(identifier_def);
                    result_loop_stmt.lowered_statement = lowered_loop;
                }
                else if (auto range = ast::as<ast::RangeType>(bop->right->expr_type)) {
                    auto l = range->range.lock();
                    ebm::ExpressionRef start, end;
                    EBMA_CONVERT_TYPE(base_type, range->base_type);
                    if (l->start) {
                        MAYBE(s, ctx.convert_expr(l->start));
                        start = s;
                    }
                    else {
                        EBM_NEW_OBJECT(start_literal, base_type);
                        start = start_literal;
                    }
                    if (l->end) {
                        MAYBE(e, ctx.convert_expr(l->end));
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
                    ctx.add_visited_node(node->init, identifier_def);
                    MAYBE(body, convert_statement(node->body));

                    EBM_INCREMENT(inc, iter, counter_type);

                    ebm::Block inner_block;
                    inner_block.container.reserve(2);
                    append(inner_block, body);
                    append(inner_block, inc);

                    EBM_BLOCK(inner_block_ref, std::move(inner_block));

                    EBM_WHILE_LOOP(loop_stmt, cond, inner_block_ref);

                    ebm::Block outer_block;
                    outer_block.container.reserve(2);
                    append(outer_block, iter_def);
                    append(outer_block, loop_stmt);

                    EBM_BLOCK(block_ref, std::move(outer_block));
                    result_loop_stmt.item_var(identifier_def);
                    result_loop_stmt.lowered_statement = block_ref;
                }
                else if (ast::as<ast::ArrayType>(bop->right->expr_type)) {
                    EBM_ARRAY_SIZE(array_size, target);
                    EBMA_CONVERT_TYPE(element_type, bop->left->expr_type);
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(indexed, element_type, target, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, element_type, indexed, false, true);
                    ctx.add_visited_node(node->init, element_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    EBM_COUNTER_LOOP_END(loop_stmt, i, array_size, inner_block_ref);
                    result_loop_stmt.item_var(element_def);
                    result_loop_stmt.lowered_statement = loop_stmt;
                }
                else if (auto lit = ast::as<ast::StrLiteral>(bop->right)) {
                    // note: representation of string is encoded as base64 in bop->right->binary_value because
                    //       src2json generates AST as json
                    MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(bop->right)));
                    EBMU_UINT_TYPE(u8_t, 8);
                    EBMU_U8_N_ARRAY(u8_n_array, candidate.size());
                    EBM_NEW_OBJECT(new_obj_ref, u8_n_array);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);
                    ebm::Block block;
                    block.container.reserve(2 + candidate.size());
                    append(block, buffer_def);
                    MAYBE_VOID(ok, construct_string_array(ctx, block, buffer, candidate));
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(array_index, u8_t, buffer, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, u8_t, array_index, true, true);
                    ctx.add_visited_node(node->init, element_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    EBMU_INT_LITERAL(len, candidate.size());
                    EBM_COUNTER_LOOP_END(loop_stmt, i, len, inner_block_ref);
                    append(block, loop_stmt);
                    EBM_BLOCK(block_ref, std::move(block));
                    result_loop_stmt.item_var(element_def);
                    result_loop_stmt.lowered_statement = block_ref;
                }
                else {
                    return unexpect_error("Invalid loop init type : {}", node_type_to_string(bop->right->expr_type->node_type));
                }
                return make_loop(std::move(result_loop_stmt));
            }
            result_loop_stmt.loop_type = ebm::LoopType::FOR;  // C-like for loop (e.g., for (int i = 0; i < n; i++))
            MAYBE(init_ref, convert_statement(node->init));
            init_v = init_ref;
        }
        if (node->cond) {
            EBMA_CONVERT_EXPRESSION(cond_ref, node->cond);
            cond_v = cond_ref;
        }
        MAYBE(body, convert_statement(node->body));
        if (node->step) {
            MAYBE(step_ref, convert_statement(node->step));
            step_v = step_ref;
        }
        if (init_v || step_v) {
            result_loop_stmt.loop_type = ebm::LoopType::FOR;  // C-like for loop (e.g., for (int i = 0; i < n; i++))
            if (init_v) {
                result_loop_stmt.init(*init_v);
            }
            if (cond_v) {
                result_loop_stmt.condition(*cond_v);
            }
            if (step_v) {
                result_loop_stmt.increment(*step_v);
            }
        }
        else if (cond_v) {
            result_loop_stmt.loop_type = ebm::LoopType::WHILE;  // While loop
            result_loop_stmt.condition(*cond_v);
        }
        else {
            result_loop_stmt.loop_type = ebm::LoopType::INFINITE;  // Infinite loop
        }
        result_loop_stmt.body = body;
        return make_loop(std::move(result_loop_stmt));
    }

    expected<ebm::StatementRef> StatementConverter::convert_statement(const std::shared_ptr<ast::Node>& node) {
        if (auto it = ctx.is_visited(node)) {
            return *it;
        }
        MAYBE(new_ref, ctx.new_statement_id());
        ctx.add_visited_node(node, new_ref);
        return convert_statement_internal(new_ref, node);
    }

    expected<ebm::StatementRef> StatementConverter::convert_statement_internal(ebm::StatementRef new_id, const std::shared_ptr<ast::Node>& node) {
        ebm::StatementBody body;
        expected<void> result = unexpect_error("Unsupported statement type: {}", node_type_to_string(node->node_type));

        brgen::ast::visit(node, [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            result = convert_statement_impl(n, body);
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        return ctx.add_statement(new_id, std::move(body));
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Assert>& node, ebm::StatementBody& body) {
        MAYBE(cond, ctx.convert_expr(node->cond));
        MAYBE(body_, assert_statement_body(ctx, cond));
        body = std::move(body_);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Return>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::RETURN;
        if (node->expr) {
            MAYBE(expr_ref, ctx.convert_expr(node->expr));
            body.value(expr_ref);
        }
        else {
            body.value(ebm::ExpressionRef{});
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Break>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::BREAK;
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Continue>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::CONTINUE;
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::If>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::IF_STATEMENT;
        MAYBE(cond_ref, ctx.convert_expr(node->cond->expr));

        // Convert then block
        MAYBE(then_block, ctx.convert_statement(node->then));

        // Convert else block
        ebm::StatementRef else_block;
        if (node->els) {
            MAYBE(else_block_, ctx.convert_statement(node->els));
            else_block = else_block_;
        }

        body = make_if_statement(cond_ref, then_block, else_block);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Loop>& node, ebm::StatementBody& body) {
        MAYBE(loop_body, convert_loop_body(ast::cast_to<ast::Loop>(node)));
        body = std::move(loop_body);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Match>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::MATCH_STATEMENT;
        ebm::MatchStatement ebm_match_stmt;

        MAYBE(target_ref, ctx.convert_expr(node->cond->expr));
        ebm_match_stmt.target = target_ref;
        ebm_match_stmt.is_exhaustive(node->struct_union_type->exhaustive);

        for (auto& branch : node->branch) {
            MAYBE(branch_ref, ctx.convert_statement(branch));
            append(ebm_match_stmt.branches, branch_ref);
        }
        body.match_statement(std::move(ebm_match_stmt));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::IndentBlock>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::BLOCK;
        ebm::Block block_body;
        for (auto& element : node->elements) {
            MAYBE(stmt_ref, ctx.convert_statement(element));
            append(block_body, stmt_ref);
        }
        body.block(std::move(block_body));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::MatchBranch>& node, ebm::StatementBody& body) {
        ebm::MatchBranch ebm_branch;
        MAYBE(cond_ref, ctx.convert_expr(node->cond->expr));
        ebm_branch.condition = cond_ref;

        ebm::StatementRef branch_body_block;
        if (node->then) {
            MAYBE(stmt_ref, ctx.convert_statement(node->then));
            branch_body_block = stmt_ref;
        }
        ebm_branch.body = branch_body_block;
        body.match_branch(std::move(ebm_branch));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Program>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::PROGRAM_DECL;

        ebm::Block program_body_block;
        for (auto& p : node->elements) {
            MAYBE(stmt_ref, ctx.convert_statement(p));
            append(program_body_block, stmt_ref);
        }
        body.block(std::move(program_body_block));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Format>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::STRUCT_DECL;
        ebm::StructDecl struct_decl;
        MAYBE(name_ref, ctx.add_identifier(node->ident->ident));
        struct_decl.name = name_ref;
        if (node->body) {
            for (auto& element : node->body->struct_type->fields) {
                if (ast::as<ast::Field>(element)) {
                    auto field_element_shared_ptr = std::static_pointer_cast<ast::Field>(element);
                    auto node_to_convert = std::static_pointer_cast<ast::Node>(field_element_shared_ptr);
                    MAYBE(stmt_ref, ctx.convert_statement(node_to_convert));
                    append(struct_decl.fields, stmt_ref);
                }
            }
        }
        body.struct_decl(std::move(struct_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Enum>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::ENUM_DECL;
        ebm::EnumDecl ebm_enum_decl;
        MAYBE(name_ref, ctx.add_identifier(node->ident->ident));
        ebm_enum_decl.name = name_ref;
        if (node->base_type) {
            EBMA_CONVERT_TYPE(base_type_ref, node->base_type);
            ebm_enum_decl.base_type = base_type_ref;
        }
        for (auto& member : node->members) {
            MAYBE(ebm_enum_member_ref, ctx.convert_statement(member));
            // Append the enum member reference to the enum declaration
            append(ebm_enum_decl.members, ebm_enum_member_ref);
        }
        body.enum_decl(std::move(ebm_enum_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::EnumMember>& node, ebm::StatementBody& body) {
        ebm::EnumMemberDecl ebm_enum_member_decl;
        MAYBE(member_name_ref, ctx.add_identifier(node->ident->ident));
        ebm_enum_member_decl.name = member_name_ref;
        if (node->value) {
            MAYBE(value_expr_ref, ctx.convert_expr(node->value));
            ebm_enum_member_decl.value = value_expr_ref;
        }
        if (node->str_literal) {
            MAYBE(str_ref, ctx.add_string(node->str_literal->value));
            ebm_enum_member_decl.string_repr = str_ref;
        }
        body.statement_kind = ebm::StatementOp::ENUM_MEMBER_DECL;
        body.enum_member_decl(std::move(ebm_enum_member_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Function>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::FUNCTION_DECL;
        ebm::FunctionDecl func_decl;
        MAYBE(name_ref, ctx.add_identifier(node->ident->ident));
        func_decl.name = name_ref;
        if (node->return_type) {
            EBMA_CONVERT_TYPE(return_type_ref, node->return_type);
            func_decl.return_type = return_type_ref;
        }
        for (auto& param : node->parameters) {
            ebm::VariableDecl param_decl;
            MAYBE(param_name_ref, ctx.add_identifier(param->ident->ident));
            param_decl.name = param_name_ref;
            EBMA_CONVERT_TYPE(param_type_ref, param->field_type);
            param_decl.var_type = param_type_ref;
            ebm::StatementBody param_body;
            param_body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
            param_body.var_decl(std::move(param_decl));
            MAYBE(param_ref, ctx.add_statement(std::move(param_body)));
            append(func_decl.params, param_ref);
        }
        body.func_decl(std::move(func_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Metadata>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::METADATA;
        ebm::Metadata ebm_metadata;
        MAYBE(name_ref, ctx.add_identifier(node->name));
        ebm_metadata.name = name_ref;
        for (auto& value : node->values) {
            MAYBE(value_expr_ref, ctx.convert_expr(value));
            append(ebm_metadata.values, value_expr_ref);
        }
        body.metadata(std::move(ebm_metadata));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::State>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::STATE_DECL;
        ebm::StateDecl state_decl;
        MAYBE(name_ref, ctx.add_identifier(node->ident->ident));
        state_decl.name = name_ref;
        ebm::Block state_body_block;
        for (auto& element : node->body->elements) {
            MAYBE(stmt_ref, ctx.convert_statement(element));
            append(state_body_block, stmt_ref);
        }
        state_decl.body = std::move(state_body_block);
        body.state_decl(std::move(state_decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Field>& node, ebm::StatementBody& body) {
        if (auto union_type = ast::as<ast::UnionType>(node->field_type)) {
            body.statement_kind = ebm::StatementOp::PROPERTY_DECL;
            ebm::PropertyDecl prop_decl;
            MAYBE(field_name_ref, ctx.add_identifier(node->ident->ident));
            prop_decl.name = field_name_ref;

            EBMA_CONVERT_TYPE(property_type_ref, node->field_type);
            prop_decl.property_type = property_type_ref;

            if (auto parent_member = node->belong.lock()) {
                MAYBE(statement_ref, ctx.convert_statement(parent_member));
                prop_decl.parent_format = statement_ref;
            }
            else {
                prop_decl.parent_format = ebm::StatementRef{};
            }

            prop_decl.merge_mode = ebm::MergeMode::COMMON_TYPE;

            body.property_decl(std::move(prop_decl));
        }
        else if (node->bit_alignment != node->eventual_bit_alignment) {
            body.statement_kind = ebm::StatementOp::BIT_FIELD_DECL;
            ebm::BitFieldDecl bit_field_decl;
            MAYBE(name_ref, ctx.add_identifier(node->ident->ident));
            bit_field_decl.name = name_ref;

            if (auto parent_member = node->belong.lock()) {
                MAYBE(statement_ref, ctx.convert_statement(parent_member));
                bit_field_decl.parent_format = statement_ref;
            }
            else {
                bit_field_decl.parent_format = ebm::StatementRef{};
            }

            if (node->field_type->bit_size) {
                MAYBE(bit_size_val, varint(*node->field_type->bit_size));
                bit_field_decl.bit_size = bit_size_val;
            }
            else {
                return unexpect_error("Bit field type has no bit size");
            }

            bit_field_decl.packed_op_type = ebm::PackedOpType::FIXED;  // Default to FIXED for now

            body.bit_field_decl(std::move(bit_field_decl));
        }
        else {
            body.statement_kind = ebm::StatementOp::FIELD_DECL;
            ebm::FieldDecl field_decl;
            if (node->ident) {
                MAYBE(field_name_ref, ctx.add_identifier(node->ident->ident));
                field_decl.name = field_name_ref;
            }
            else {
                field_decl.name = ebm::IdentifierRef{};  // Anonymous field
            }
            EBMA_CONVERT_TYPE(type_ref, node->field_type);
            field_decl.field_type = type_ref;
            field_decl.is_state_variable(node->is_state_variable);
            body.field_decl(std::move(field_decl));
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ExplicitError>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::ERROR_REPORT;
        ebm::ErrorReport error_report;
        MAYBE(message_str_ref, ctx.add_string(node->message->value));
        error_report.message = message_str_ref;
        for (auto& arg : node->base->arguments) {
            MAYBE(arg_expr_ref, ctx.convert_expr(arg));
            append(error_report.arguments, arg_expr_ref);
        }
        body.error_report(std::move(error_report));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Import>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::IMPORT_MODULE;
        MAYBE(module_name_ref, ctx.add_identifier(node->path));
        body.module_name(module_name_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ImplicitYield>& node, ebm::StatementBody& body) {
        body.statement_kind = ebm::StatementOp::EXPRESSION;
        MAYBE(expr_ref, ctx.convert_expr(node->expr));
        body.expression(expr_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Binary>& node, ebm::StatementBody& body) {
        if (node->op == ast::BinaryOp::assign) {
            body.statement_kind = ebm::StatementOp::ASSIGNMENT;
            MAYBE(target_ref, ctx.convert_expr(node->left));
            body.target(target_ref);
            MAYBE(value_ref, ctx.convert_expr(node->right));
            body.value(value_ref);
        }
        else if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
            ebm::VariableDecl var_decl;
            auto name_ident = ast::as<ast::Ident>(node->left);
            if (!name_ident) {
                return unexpect_error("Left-hand side of define_assign/const_assign must be an identifier");
            }
            MAYBE(name_ref, ctx.add_identifier(name_ident->ident));
            var_decl.name = name_ref;

            EBMA_CONVERT_TYPE(type_ref, node->left->expr_type);
            var_decl.var_type = type_ref;

            if (node->right) {
                MAYBE(initial_value_ref, ctx.convert_expr(node->right));
                var_decl.initial_value = initial_value_ref;
            }
            var_decl.is_constant(node->op == ast::BinaryOp::const_assign);  // Set is_constant based on operator
            body.var_decl(std::move(var_decl));
        }
        else {
            body.statement_kind = ebm::StatementOp::EXPRESSION;
            MAYBE(expr_ref, ctx.convert_expr(ast::cast_to<ast::Expr>(node)));
            body.expression(expr_ref);
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Node>& node, ebm::StatementBody& body) {
        return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
    }

}  // namespace ebmgen
