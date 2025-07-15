#include "../converter.hpp"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"

namespace ebmgen {

    expected<std::pair<size_t, bool>> get_integral_size_and_sign(auto& type_repo, ebm::TypeRef type) {
        for (;;) {
            auto type_ref = type_repo.get(type);
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

    expected<ebm::ExpressionRef> Converter::get_max_value_expr(ebm::TypeRef type) {
        MAYBE(size_and_signed, get_integral_size_and_sign(type_repo, type));
        auto [size, is_signed] = size_and_signed;
        MAYBE(value_type, get_unsigned_n_int(size));
        EBM_NEW_OBJECT(zero, value_type);
        EBM_UNARY_OP(max_unsigned, ebm::UnaryOp::bit_not, value_type, zero);
        auto result = max_unsigned;
        if (is_signed) {
            MAYBE(one, get_int_literal(1));
            EBM_BINARY_OP(max_signed, ebm::BinaryOp::right_shift, value_type, max_unsigned, one);
            result = max_signed;
        }
        EBM_CAST(max_value_lowered, type, value_type, result);

        EBM_MAX_VALUE(max_value_expr, type, max_value_lowered);
        return max_value_expr;
    }

    expected<ebm::StatementBody> Converter::convert_loop_body(const std::shared_ptr<ast::Loop>& node) {
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
                MAYBE(target, convert_expr(bop->right));
                MAYBE(ident_ref, add_identifier(ident->ident));
                result_loop_stmt.collection(target);
                if (ast::as<ast::IntType>(bop->right->expr_type)) {
                    MAYBE(expr_type, convert_type(bop->left->expr_type));
                    EBM_COUNTER_LOOP_START_CUSTOM(i, expr_type);
                    EBM_DEFINE_VARIABLE(identifier, ident_ref, expr_type, i, true, false);
                    visited_nodes.emplace(node->init, identifier_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    MAYBE(bool_type, get_bool_type());
                    EBM_COUNTER_LOOP_END(lowered_loop, i, target, inner_block_ref);
                    result_loop_stmt.item_var(identifier_def);
                    result_loop_stmt.lowered_statement = lowered_loop;
                }
                else if (auto range = ast::as<ast::RangeType>(bop->right->expr_type)) {
                    auto l = range->range.lock();
                    ebm::ExpressionRef start, end;
                    MAYBE(base_type, convert_type(range->base_type));
                    if (l->start) {
                        MAYBE(s, convert_expr(l->start));
                        start = s;
                    }
                    else {
                        EBM_NEW_OBJECT(start_literal, base_type);
                        start = start_literal;
                    }
                    if (l->end) {
                        MAYBE(e, convert_expr(l->end));
                        end = e;
                    }
                    else {
                        MAYBE(max_value_expr, get_max_value_expr(base_type));
                        end = max_value_expr;
                    }
                    MAYBE(size_and_signed, get_integral_size_and_sign(type_repo, base_type));
                    auto [n, is_signed] = size_and_signed;
                    MAYBE(counter_type, get_unsigned_n_int(n));
                    EBM_CAST(start_casted, counter_type, base_type, start);
                    EBM_CAST(end_casted, counter_type, base_type, end);

                    EBM_DEFINE_ANONYMOUS_VARIABLE(iter, counter_type, start_casted);
                    MAYBE(bool_type, get_bool_type());
                    EBM_BINARY_OP(cond, l->op == ast::BinaryOp::range_inclusive ? ebm::BinaryOp::less_or_eq : ebm::BinaryOp::less, bool_type, iter, end_casted);

                    EBM_DEFINE_VARIABLE(identifier, ident_ref, counter_type, iter, true, false);
                    visited_nodes.emplace(node->init, identifier_def);
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
                    MAYBE(element_type, convert_type(bop->left->expr_type));
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(indexed, element_type, target, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, element_type, indexed, false, true);
                    visited_nodes.emplace(node->init, element_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    EBM_COUNTER_LOOP_END(loop_stmt, i, array_size, inner_block_ref);
                    result_loop_stmt.item_var(element_def);
                    result_loop_stmt.lowered_statement = loop_stmt;
                }
                else if (auto lit = ast::as<ast::StrLiteral>(bop->right)) {
                    // note: representation of string is encoded as base64 in bop->right->binary_value because
                    //       src2json generates AST as json
                    MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(bop->right)));
                    MAYBE(u8_t, get_unsigned_n_int(8));
                    MAYBE(u8_n_array, get_u8_n_array(candidate.size()));
                    EBM_NEW_OBJECT(new_obj_ref, u8_n_array);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);
                    ebm::Block block;
                    block.container.reserve(2 + candidate.size());
                    append(block, buffer_def);
                    MAYBE_VOID(ok, construct_string_array(block, buffer, candidate));
                    EBM_COUNTER_LOOP_START(i);
                    EBM_INDEX(array_index, u8_t, buffer, i);
                    EBM_DEFINE_VARIABLE(element, ident_ref, u8_t, array_index, true, true);
                    visited_nodes.emplace(node->init, element_def);
                    MAYBE(inner_block_ref, convert_statement(node->body));
                    MAYBE(len, get_int_literal(candidate.size()));
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
            MAYBE(cond_ref, convert_expr(node->cond));
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

    expected<ebm::StatementRef> Converter::convert_statement_impl(ebm::StatementRef new_id, const std::shared_ptr<ast::Node>& node) {
        ebm::StatementBody body;
        if (auto assert_stmt = ast::as<ast::Assert>(node)) {
            MAYBE(cond, convert_expr(assert_stmt->cond));
            MAYBE(body_, assert_statement_body(cond));
            body = std::move(body_);
        }
        else if (auto return_stmt = ast::as<ast::Return>(node)) {
            body.statement_kind = ebm::StatementOp::RETURN;
            if (return_stmt->expr) {
                MAYBE(expr_ref, convert_expr(return_stmt->expr));
                body.value(expr_ref);
            }
            else {
                body.value(ebm::ExpressionRef{});
            }
        }
        else if (auto break_stmt = ast::as<ast::Break>(node)) {
            body.statement_kind = ebm::StatementOp::BREAK;
        }
        else if (auto continue_stmt = ast::as<ast::Continue>(node)) {
            body.statement_kind = ebm::StatementOp::CONTINUE;
        }
        else if (auto if_stmt = ast::as<ast::If>(node)) {
            body.statement_kind = ebm::StatementOp::IF_STATEMENT;
            MAYBE(cond_ref, convert_expr(if_stmt->cond->expr));

            // Convert then block
            MAYBE(then_block, convert_statement(if_stmt->then));

            // Convert else block
            ebm::StatementRef else_block;
            if (if_stmt->els) {
                MAYBE(else_block_, convert_statement(if_stmt->els));
                else_block = else_block_;
            }

            body = make_if_statement(cond_ref, then_block, else_block);
        }
        else if (auto loop_stmt = ast::as<ast::Loop>(node)) {
            MAYBE(loop_body, convert_loop_body(ast::cast_to<ast::Loop>(node)));
            body = std::move(loop_body);
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
                auto branch_ref = convert_statement(branch);
                if (!branch_ref) {
                    return unexpect_error(std::move(branch_ref.error()));
                }
                append(ebm_match_stmt.branches, *branch_ref);
            }
            body.match_statement(std::move(ebm_match_stmt));
        }
        else if (auto indent_block = ast::as<ast::IndentBlock>(node)) {
            body.statement_kind = ebm::StatementOp::BLOCK;
            ebm::Block block_body;
            for (auto& element : indent_block->elements) {
                auto stmt_ref = convert_statement(element);
                if (!stmt_ref) {
                    return unexpect_error(std::move(stmt_ref.error()));
                }
                append(block_body, *stmt_ref);
            }
            body.block(std::move(block_body));
        }
        else if (auto match_branch = ast::as<ast::MatchBranch>(node)) {
            ebm::MatchBranch ebm_branch;
            auto cond_ref = convert_expr(match_branch->cond->expr);
            if (!cond_ref) {
                return unexpect_error(std::move(cond_ref.error()));
            }
            ebm_branch.condition = *cond_ref;

            ebm::StatementRef branch_body_block;
            if (match_branch->then) {
                auto stmt_ref = convert_statement(match_branch->then);
                if (!stmt_ref) {
                    return unexpect_error(std::move(stmt_ref.error()));
                }
                branch_body_block = *stmt_ref;
            }
            ebm_branch.body = branch_body_block;
            body.match_branch(std::move(ebm_branch));
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
            body.block(std::move(program_body_block));
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
                    if (ast::as<ast::Field>(element)) {
                        auto field_element_shared_ptr = std::static_pointer_cast<ast::Field>(element);
                        auto node_to_convert = std::static_pointer_cast<ast::Node>(field_element_shared_ptr);
                        auto stmt_ref = convert_statement(node_to_convert);
                        if (!stmt_ref) {
                            return unexpect_error(std::move(stmt_ref.error()));
                        }
                        append(struct_decl.fields, *stmt_ref);
                    }
                }
            }
            body.struct_decl(std::move(struct_decl));
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
                auto ebm_enum_member_ref = convert_statement(member);
                if (!ebm_enum_member_ref) {
                    return unexpect_error(std::move(ebm_enum_member_ref.error()));
                }
                // Append the enum member reference to the enum declaration
                append(ebm_enum_decl.members, *ebm_enum_member_ref);
            }
            body.enum_decl(std::move(ebm_enum_decl));
        }
        else if (auto enum_member = ast::as<ast::EnumMember>(node)) {
            ebm::EnumMemberDecl ebm_enum_member_decl;
            auto member_name_ref = add_identifier(enum_member->ident->ident);
            if (!member_name_ref) {
                return unexpect_error(std::move(member_name_ref.error()));
            }
            ebm_enum_member_decl.name = *member_name_ref;
            if (enum_member->value) {
                auto value_expr_ref = convert_expr(enum_member->value);
                if (!value_expr_ref) {
                    return unexpect_error(std::move(value_expr_ref.error()));
                }
                ebm_enum_member_decl.value = *value_expr_ref;
            }
            if (enum_member->str_literal) {
                auto str_ref = add_string(enum_member->str_literal->value);
                if (!str_ref) {
                    return unexpect_error(std::move(str_ref.error()));
                }
                ebm_enum_member_decl.string_repr = *str_ref;
            }
            body.statement_kind = ebm::StatementOp::ENUM_MEMBER_DECL;
            body.enum_member_decl(std::move(ebm_enum_member_decl));
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
            body.func_decl(std::move(func_decl));
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
            body.metadata(std::move(ebm_metadata));
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
            state_decl.body = std::move(state_body_block);
            body.state_decl(std::move(state_decl));
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

                auto property_type_ref = convert_type(field->field_type);
                if (!property_type_ref) {
                    return unexpect_error(std::move(property_type_ref.error()));
                }
                prop_decl.property_type = *property_type_ref;

                if (auto parent_member = field->belong.lock()) {
                    auto statement_ref = convert_statement(parent_member);
                    if (!statement_ref) {
                        return unexpect_error(std::move(statement_ref.error()));
                    }
                    prop_decl.parent_format = *statement_ref;
                }
                else {
                    prop_decl.parent_format = ebm::StatementRef{};
                }

                prop_decl.merge_mode = ebm::MergeMode::COMMON_TYPE;

                body.property_decl(std::move(prop_decl));
            }
            else if (field->bit_alignment != field->eventual_bit_alignment) {
                body.statement_kind = ebm::StatementOp::BIT_FIELD_DECL;
                ebm::BitFieldDecl bit_field_decl;
                auto name_ref = add_identifier(field->ident->ident);
                if (!name_ref) {
                    return unexpect_error(std::move(name_ref.error()));
                }
                bit_field_decl.name = *name_ref;

                if (auto parent_member = field->belong.lock()) {
                    auto statement_ref = convert_statement(parent_member);
                    if (!statement_ref) {
                        return unexpect_error(std::move(statement_ref.error()));
                    }
                    bit_field_decl.parent_format = *statement_ref;
                }
                else {
                    bit_field_decl.parent_format = ebm::StatementRef{};
                }

                if (field->field_type->bit_size) {
                    auto bit_size_val = varint(*field->field_type->bit_size);
                    if (!bit_size_val) {
                        return unexpect_error(std::move(bit_size_val.error()));
                    }
                    bit_field_decl.bit_size = *bit_size_val;
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
                if (field->ident) {
                    auto field_name_ref = add_identifier(field->ident->ident);
                    if (!field_name_ref) {
                        return unexpect_error(std::move(field_name_ref.error()));
                    }
                    field_decl.name = *field_name_ref;
                }
                else {
                    field_decl.name = ebm::IdentifierRef{};  // Anonymous field
                }
                auto type_ref = convert_type(field->field_type);
                if (!type_ref) {
                    return unexpect_error(std::move(type_ref.error()));
                }
                field_decl.field_type = *type_ref;
                // TODO: Handle parent_struct and is_state_variable
                body.field_decl(std::move(field_decl));
            }
        }
        else if (auto explicit_error_stmt = ast::as<ast::ExplicitError>(node)) {
            body.statement_kind = ebm::StatementOp::ERROR_REPORT;
            ebm::ErrorReport error_report;
            auto message_str_ref = add_string(explicit_error_stmt->message->value);
            if (!message_str_ref) {
                return unexpect_error(std::move(message_str_ref.error()));
            }
            error_report.message = *message_str_ref;
            for (auto& arg : explicit_error_stmt->base->arguments) {
                auto arg_expr_ref = convert_expr(arg);
                if (!arg_expr_ref) {
                    return unexpect_error(std::move(arg_expr_ref.error()));
                }
                append(error_report.arguments, *arg_expr_ref);
            }
            body.error_report(std::move(error_report));
        }
        else if (auto import_stmt = ast::as<ast::Import>(node)) {
            body.statement_kind = ebm::StatementOp::IMPORT_MODULE;
            auto module_name_ref = add_identifier(import_stmt->path);
            if (!module_name_ref) {
                return unexpect_error(std::move(module_name_ref.error()));
            }
            body.module_name(*module_name_ref);
        }
        else if (auto implicit_yield_stmt = ast::as<ast::ImplicitYield>(node)) {
            body.statement_kind = ebm::StatementOp::EXPRESSION;
            auto expr_ref = convert_expr(implicit_yield_stmt->expr);
            if (!expr_ref) {
                return unexpect_error(std::move(expr_ref.error()));
            }
            body.expression(*expr_ref);
        }
        else if (auto binary_expr = ast::as<ast::Binary>(node)) {
            if (binary_expr->op == ast::BinaryOp::assign) {
                body.statement_kind = ebm::StatementOp::ASSIGNMENT;
                auto target_ref = convert_expr(binary_expr->left);
                if (!target_ref) {
                    return unexpect_error(std::move(target_ref.error()));
                }
                body.target(*target_ref);
                auto value_ref = convert_expr(binary_expr->right);
                if (!value_ref) {
                    return unexpect_error(std::move(value_ref.error()));
                }
                body.value(*value_ref);
            }
            else if (binary_expr->op == ast::BinaryOp::define_assign || binary_expr->op == ast::BinaryOp::const_assign) {
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
            }
            else {
                body.statement_kind = ebm::StatementOp::EXPRESSION;
                auto expr_ref = convert_expr(ast::cast_to<ast::Expr>(node));
                if (!expr_ref) {
                    return unexpect_error(std::move(expr_ref.error()));
                }
                body.expression(*expr_ref);
            }
        }
        else {
            // Debug print to identify unhandled node types
            return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
        }
        return add_statement(new_id, std::move(body));
    }
}  // namespace ebmgen
