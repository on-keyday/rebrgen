#include "../converter.hpp"

namespace ebmgen {
    expected<ebm::StatementRef> Converter::convert_statement_impl(ebm::StatementRef new_id, const std::shared_ptr<ast::Node>& node) {
        ebm::StatementBody body;
        if (auto assert_stmt = ast::as<ast::Assert>(node)) {
            body.statement_kind = ebm::StatementOp::ASSERT;
            auto cond_ref = convert_expr(assert_stmt->cond);
            if (!cond_ref) {
                return unexpect_error(std::move(cond_ref.error()));
            }
            body.condition(*cond_ref);
            return add_statement(new_id, std::move(body));
        }
        else if (auto return_stmt = ast::as<ast::Return>(node)) {
            body.statement_kind = ebm::StatementOp::RETURN;
            if (return_stmt->expr) {
                auto expr_ref = convert_expr(return_stmt->expr);
                if (!expr_ref) {
                    return unexpect_error(std::move(expr_ref.error()));
                }
                body.value(*expr_ref);
            }
            return add_statement(new_id, std::move(body));
        }
        else if (auto break_stmt = ast::as<ast::Break>(node)) {
            body.statement_kind = ebm::StatementOp::BREAK;
            return add_statement(new_id, std::move(body));
        }
        else if (auto continue_stmt = ast::as<ast::Continue>(node)) {
            body.statement_kind = ebm::StatementOp::CONTINUE;
            return add_statement(new_id, std::move(body));
        }
        else if (auto if_stmt = ast::as<ast::If>(node)) {
            body.statement_kind = ebm::StatementOp::IF_STATEMENT;
            ebm::IfStatement ebm_if_stmt;
            auto cond_ref = convert_expr(if_stmt->cond->expr);
            if (!cond_ref) {
                return unexpect_error(std::move(cond_ref.error()));
            }
            ebm_if_stmt.condition = *cond_ref;

            // Convert then block
            ebm::Block then_block;
            for (auto& element : if_stmt->then->elements) {
                auto stmt_ref = convert_statement(element);
                if (!stmt_ref) {
                    return unexpect_error(std::move(stmt_ref.error()));
                }
                append(then_block, *stmt_ref);
            }
            ebm_if_stmt.then_block = std::move(then_block);

            // Convert else block
            ebm::Block else_block;
            if (if_stmt->els) {
                // Assuming else is an IndentBlock or another If
                if (auto indent_block = ast::as<ast::IndentBlock>(if_stmt->els)) {
                    for (auto& element : indent_block->elements) {
                        auto stmt_ref = convert_statement(std::static_pointer_cast<ast::Stmt>(element));
                        if (!stmt_ref) {
                            return unexpect_error(std::move(stmt_ref.error()));
                        }
                        append(else_block, *stmt_ref);
                    }
                }
                else if (auto else_if_stmt = ast::as<ast::If>(if_stmt->els)) {
                    // Nested if-else if
                    auto stmt_ref = convert_statement(std::static_pointer_cast<ast::Stmt>(if_stmt->els));
                    if (!stmt_ref) {
                        return unexpect_error(std::move(stmt_ref.error()));
                    }
                    append(else_block, *stmt_ref);
                }
                else {
                    return unexpect_error("Unsupported node type for else block");
                }
            }
            ebm_if_stmt.else_block = std::move(else_block);

            body.if_statement(std::move(ebm_if_stmt));
            return add_statement(new_id, std::move(body));
        }
        else if (auto loop_stmt = ast::as<ast::Loop>(node)) {
            body.statement_kind = ebm::StatementOp::LOOP_STATEMENT;
            ebm::LoopStatement ebm_loop_stmt;

            // Determine loop type and set corresponding fields
            if (loop_stmt->init && loop_stmt->cond && loop_stmt->step) {
                // For-each loop (assuming init, cond, step implies for-each)
                ebm_loop_stmt.loop_type = ebm::LoopType::FOR_EACH;
                // TODO: Handle item_var and collection for FOR_EACH
            }
            else if (loop_stmt->cond) {
                // While loop
                ebm_loop_stmt.loop_type = ebm::LoopType::WHILE;
                auto cond_ref = convert_expr(loop_stmt->cond);
                if (!cond_ref) {
                    return unexpect_error(std::move(cond_ref.error()));
                }
                ebm_loop_stmt.condition(*cond_ref);
            }
            else {
                // Infinite loop
                ebm_loop_stmt.loop_type = ebm::LoopType::INFINITE;
            }

            // Convert loop body
            ebm::Block loop_body_block;
            if (loop_stmt->body) {
                for (auto& element : loop_stmt->body->elements) {
                    auto stmt_ref = convert_statement(element);
                    if (!stmt_ref) {
                        return unexpect_error(std::move(stmt_ref.error()));
                    }
                    append(loop_body_block, *stmt_ref);
                }
            }
            ebm_loop_stmt.body = std::move(loop_body_block);
            body.loop(std::move(ebm_loop_stmt));
            return add_statement(new_id, std::move(body));
        }
        else if (auto match_stmt = ast::as<ast::Match>(node)) {
            body.statement_kind = ebm::StatementOp::MATCH_STATEMENT;
            ebm::MatchStatement ebm_match_stmt;

            auto target_ref = convert_expr(match_stmt->cond->expr);
            if (!target_ref) {
                return unexpect_error(std::move(target_ref.error()));
            }
            ebm_match_stmt.target = *target_ref;
            ebm_match_stmt.is_exhaustive(match_stmt->struct_union_type->exhaustive);

            for (auto& branch : match_stmt->branch) {
                ebm::MatchBranch ebm_branch;
                auto cond_ref = convert_expr(branch->cond->expr);
                if (!cond_ref) {
                    return unexpect_error(std::move(cond_ref.error()));
                }
                ebm_branch.condition = *cond_ref;

                ebm::Block branch_body_block;
                if (branch->then) {
                    if (auto indent_block = ast::as<ast::IndentBlock>(branch->then)) {
                        for (auto& element : indent_block->elements) {
                            auto stmt_ref = convert_statement(element);
                            if (!stmt_ref) {
                                return unexpect_error(std::move(stmt_ref.error()));
                            }
                            append(branch_body_block, *stmt_ref);
                        }
                    }
                    else if (auto stmt_element = ast::as<ast::ScopedStatement>(branch->then)) {
                        auto stmt_ref = convert_statement(stmt_element->statement);
                        if (!stmt_ref) {
                            return unexpect_error(std::move(stmt_ref.error()));
                        }
                        append(branch_body_block, *stmt_ref);
                    }
                    else {
                        return unexpect_error("Unsupported node type for match branch body");
                    }
                }
                auto ok = set_length(branch_body_block);
                if (!ok) {
                    return unexpect_error(std::move(ok.error()));
                }
                ebm_branch.body = std::move(branch_body_block);
                ebm::StatementBody ebm_branch_stmt;
                ebm_branch_stmt.statement_kind = ebm::StatementOp::MATCH_BRANCH;
                ebm_branch_stmt.match_branch(std::move(ebm_branch));
                auto branch_ref = add_statement(std::move(ebm_branch_stmt));
                if (!branch_ref) {
                    return unexpect_error(std::move(branch_ref.error()));
                }
                append(ebm_match_stmt.branches, *branch_ref);
            }
            return set_length(ebm_match_stmt.branches).and_then([&] {
                body.match_statement(std::move(ebm_match_stmt));
                return add_statement(new_id, std::move(body));
            });
        }
        else if (auto program_stmt = ast::as<ast::Program>(node)) {
            body.statement_kind = ebm::StatementOp::PROGRAM_DECL;

            ebm::Block program_body_block;
            for (auto& p : program_stmt->elements) {
                auto stmt_ref = convert_statement(p);
                if (!stmt_ref) {
                    return unexpect_error(std::move(stmt_ref.error()));
                }
                append(program_body_block, *stmt_ref);
            }
            return set_length(program_body_block).and_then([&] {
                body.body(std::move(program_body_block));
                return add_statement(new_id, std::move(body));
            });
        }
        else if (auto format = ast::as<ast::Format>(node)) {
            body.statement_kind = ebm::StatementOp::STRUCT_DECL;
            ebm::StructDecl struct_decl;
            auto name_ref = add_identifier(format->ident->ident);
            if (!name_ref) {
                return unexpect_error(std::move(name_ref.error()));
            }
            struct_decl.name = *name_ref;
            if (format->body) {
                for (auto& element : format->body->struct_type->fields) {
                    auto stmt_ref = convert_statement(element);
                    if (!stmt_ref) {
                        return unexpect_error(std::move(stmt_ref.error()));
                    }
                    append(struct_decl.fields, *stmt_ref);
                }
            }
            return set_length(struct_decl.fields).and_then([&] {
                body.struct_decl(std::move(struct_decl));
                return add_statement(new_id, std::move(body));
            });
        }
        else if (auto enum_decl = ast::as<ast::Enum>(node)) {
            body.statement_kind = ebm::StatementOp::ENUM_DECL;
            ebm::EnumDecl ebm_enum_decl;
            auto name_ref = add_identifier(enum_decl->ident->ident);
            if (!name_ref) {
                return unexpect_error(std::move(name_ref.error()));
            }
            ebm_enum_decl.name = *name_ref;
            if (enum_decl->base_type) {
                auto base_type_ref = convert_type(enum_decl->base_type);
                if (!base_type_ref) {
                    return unexpect_error(std::move(base_type_ref.error()));
                }
                ebm_enum_decl.base_type = *base_type_ref;
            }
            for (auto& member : enum_decl->members) {
                ebm::EnumMemberDecl ebm_enum_member_decl;
                auto member_name_ref = add_identifier(member->ident->ident);
                if (!member_name_ref) {
                    return unexpect_error(std::move(member_name_ref.error()));
                }
                ebm_enum_member_decl.name = *member_name_ref;
                if (member->value) {
                    auto value_expr_ref = convert_expr(member->value);
                    if (!value_expr_ref) {
                        return unexpect_error(std::move(value_expr_ref.error()));
                    }
                    ebm_enum_member_decl.value = *value_expr_ref;
                }
                if (member->str_literal) {
                    auto str_ref = add_string(member->str_literal->value);
                    if (!str_ref) {
                        return unexpect_error(std::move(str_ref.error()));
                    }
                    ebm_enum_member_decl.string_repr = *str_ref;
                }
                ebm::StatementBody ebm_enum_member_body;
                ebm_enum_member_body.statement_kind = ebm::StatementOp::ENUM_MEMBER_DECL;
                ebm_enum_member_body.enum_member_decl(std::move(ebm_enum_member_decl));
                auto ebm_enum_member_ref = add_statement(std::move(ebm_enum_member_body));
                if (!ebm_enum_member_ref) {
                    return unexpect_error(std::move(ebm_enum_member_ref.error()));
                }
                // Append the enum member reference to the enum declaration
                append(ebm_enum_decl.members, *ebm_enum_member_ref);
            }
            auto ok = set_length(ebm_enum_decl.members);
            if (!ok) {
                return unexpect_error(std::move(ok.error()));
            }
            body.enum_decl(std::move(ebm_enum_decl));
            return add_statement(new_id, std::move(body));
        }
        else if (auto func = ast::as<ast::Function>(node)) {
            body.statement_kind = ebm::StatementOp::FUNCTION_DECL;
            ebm::FunctionDecl func_decl;
            auto name_ref = add_identifier(func->ident->ident);
            if (!name_ref) {
                return unexpect_error(std::move(name_ref.error()));
            }
            func_decl.name = *name_ref;
            if (func->return_type) {
                auto return_type_ref = convert_type(func->return_type);
                if (!return_type_ref) {
                    return unexpect_error(std::move(return_type_ref.error()));
                }
                func_decl.return_type = *return_type_ref;
            }
            for (auto& param : func->parameters) {
                ebm::VariableDecl param_decl;
                auto param_name_ref = add_identifier(param->ident->ident);
                if (!param_name_ref) {
                    return unexpect_error(std::move(param_name_ref.error()));
                }
                param_decl.name = *param_name_ref;
                auto param_type_ref = convert_type(param->field_type);
                if (!param_type_ref) {
                    return unexpect_error(std::move(param_type_ref.error()));
                }
                param_decl.var_type = *param_type_ref;
                ebm::StatementBody param_body;
                param_body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
                param_body.var_decl(std::move(param_decl));
                auto param_ref = add_statement(std::move(param_body));
                if (!param_ref) {
                    return unexpect_error(std::move(param_ref.error()));
                }
                append(func_decl.params, *param_ref);
            }
            auto ok = set_length(func_decl.params);
            if (!ok) {
                return unexpect_error(std::move(ok.error()));
            }
            body.func_decl(std::move(func_decl));
            return add_statement(new_id, std::move(body));
        }
        else if (auto metadata_stmt = ast::as<ast::Metadata>(node)) {
            body.statement_kind = ebm::StatementOp::METADATA;
            ebm::Metadata ebm_metadata;
            auto name_ref = add_identifier(metadata_stmt->name);
            if (!name_ref) {
                return unexpect_error(std::move(name_ref.error()));
            }
            ebm_metadata.name = *name_ref;
            for (auto& value : metadata_stmt->values) {
                auto value_expr_ref = convert_expr(value);
                if (!value_expr_ref) {
                    return unexpect_error(std::move(value_expr_ref.error()));
                }
                append(ebm_metadata.values, *value_expr_ref);
            }
            auto ok = set_length(ebm_metadata.values);
            if (!ok) {
                return unexpect_error(std::move(ok.error()));
            }
            body.metadata(std::move(ebm_metadata));
            return add_statement(new_id, std::move(body));
        }
        else if (auto state_stmt = ast::as<ast::State>(node)) {
            body.statement_kind = ebm::StatementOp::STATE_DECL;
            ebm::StatementBody state_decl_body;
            ebm::StateDecl state_decl;
            auto name_ref = add_identifier(state_stmt->ident->ident);
            if (!name_ref) {
                return unexpect_error(std::move(name_ref.error()));
            }
            state_decl.name = *name_ref;
            ebm::Block state_body_block;
            for (auto& element : state_stmt->body->elements) {
                auto stmt_ref = convert_statement(element);
                if (!stmt_ref) {
                    return unexpect_error(std::move(stmt_ref.error()));
                }
                append(state_body_block, *stmt_ref);
            }
            return set_length(state_body_block).and_then([&] {
                state_decl.body = std::move(state_body_block);
                body.state_decl(std::move(state_decl));
                return add_statement(new_id, std::move(body));
            });
        }
        else if (auto field = ast::as<ast::Field>(node)) {
            if (auto union_type = ast::as<ast::UnionType>(field->field_type)) {
                body.statement_kind = ebm::StatementOp::PROPERTY_DECL;
                ebm::PropertyDecl prop_decl;
                auto field_name_ref = add_identifier(field->ident->ident);
                if (!field_name_ref) {
                    return unexpect_error(std::move(field_name_ref.error()));
                }
                prop_decl.name = *field_name_ref;
                auto property_type_ref = convert_type(std::static_pointer_cast<ast::UnionType>(field->field_type));
                if (!property_type_ref) {
                    return unexpect_error(std::move(property_type_ref.error()));
                }
                prop_decl.property_type = *property_type_ref;
                // TODO: Handle parent_format and merge_mode
                body.property_decl(std::move(prop_decl));
                return add_statement(new_id, std::move(body));
            }
            else {
                body.statement_kind = ebm::StatementOp::FIELD_DECL;
                ebm::FieldDecl field_decl;
                if (field->ident) {
                    auto field_name_ref = add_identifier(field->ident->ident);
                    if (!field_name_ref) {
                        return unexpect_error(std::move(field_name_ref.error()));
                    }
                    field_decl.name = *field_name_ref;
                }
                else {
                    auto anonymous = add_anonymous_identifier();
                    if (!anonymous) {
                        return unexpect_error(std::move(anonymous.error()));
                    }
                    field_decl.name = *anonymous;
                }
                auto type_ref = convert_type(field->field_type);
                if (!type_ref) {
                    return unexpect_error(std::move(type_ref.error()));
                }
                field_decl.field_type = *type_ref;
                // TODO: Handle parent_struct and is_state_variable
                body.field_decl(std::move(field_decl));
                return add_statement(new_id, std::move(body));
            }
        }
        // TODO: Implement conversion for different statement types
        else if (auto binary_expr = ast::as<ast::Binary>(node)) {
            if (binary_expr->op == ast::BinaryOp::define_assign || binary_expr->op == ast::BinaryOp::const_assign) {
                body.statement_kind = ebm::StatementOp::VARIABLE_DECL;
                ebm::VariableDecl var_decl;
                auto name_ident = ast::as<ast::Ident>(binary_expr->left);
                if (!name_ident) {
                    return unexpect_error("Left-hand side of define_assign/const_assign must be an identifier");
                }
                auto name_ref = add_identifier(name_ident->ident);
                if (!name_ref) {
                    return unexpect_error(std::move(name_ref.error()));
                }
                var_decl.name = *name_ref;

                auto type_ref = convert_type(binary_expr->left->expr_type);
                if (!type_ref) {
                    return unexpect_error(std::move(type_ref.error()));
                }
                var_decl.var_type = *type_ref;

                if (binary_expr->right) {
                    auto initial_value_ref = convert_expr(binary_expr->right);
                    if (!initial_value_ref) {
                        return unexpect_error(std::move(initial_value_ref.error()));
                    }
                    var_decl.initial_value = *initial_value_ref;
                }
                var_decl.is_constant(binary_expr->op == ast::BinaryOp::const_assign);  // Set is_constant based on operator
                body.var_decl(std::move(var_decl));
                return add_statement(new_id, std::move(body));
            }
            // Fall through to expression conversion if not define_assign or const_assign
            body.statement_kind = ebm::StatementOp::EXPRESSION;
            auto expr_ref = convert_expr(ast::cast_to<ast::Expr>(node));
            if (!expr_ref) {
                return unexpect_error(std::move(expr_ref.error()));
            }
            body.expression(*expr_ref);
            return add_statement(new_id, std::move(body));
        }
        return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
    }
}
