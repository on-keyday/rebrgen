#include "core/ast/node/statement.h"
#include "../converter.hpp"
#include "core/ast/ast.h"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>
#include <memory>

namespace ebmgen {

    expected<std::pair<size_t, bool>> get_integral_size_and_sign(ConverterContext& ctx, ebm::TypeRef type) {
        for (;;) {
            auto type_ref = ctx.repository().get_type(type);
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
                    EBMA_CONVERT_STATEMENT(body, node->body);

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
                    make_init_visited(element_def);
                    EBMA_CONVERT_STATEMENT(inner_block_ref, node->body);
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
        return ref;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Assert>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        EBMA_CONVERT_EXPRESSION(cond, node->cond);
        MAYBE(body_, assert_statement_body(ctx, cond));
        body = std::move(body_);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Return>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::RETURN;
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
        body.kind = ebm::StatementOp::BREAK;
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Continue>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::CONTINUE;
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::If>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::IF_STATEMENT;
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
        MAYBE(loop_body, convert_loop_body(ast::cast_to<ast::Loop>(node)));
        body = std::move(loop_body);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Match>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::MATCH_STATEMENT;
        ebm::MatchStatement ebm_match_stmt;

        EBMA_CONVERT_EXPRESSION(target_ref, node->cond->expr);
        ebm_match_stmt.target = target_ref;
        ebm_match_stmt.is_exhaustive(node->struct_union_type->exhaustive);

        for (auto& branch : node->branch) {
            EBMA_CONVERT_STATEMENT(branch_ref, branch);
            append(ebm_match_stmt.branches, branch_ref);
        }
        body.match_statement(std::move(ebm_match_stmt));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::IndentBlock>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::BLOCK;
        ebm::Block block_body;
        const auto _scope = ctx.state().set_current_block(&block_body);
        for (auto& element : node->elements) {
            EBMA_CONVERT_STATEMENT(stmt_ref, element);
            append(block_body, stmt_ref);
        }
        body.block(std::move(block_body));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::MatchBranch>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::MATCH_BRANCH;
        ebm::MatchBranch ebm_branch;
        EBMA_CONVERT_EXPRESSION(cond_ref, node->cond->expr);
        ebm_branch.condition = cond_ref;

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
        body.kind = ebm::StatementOp::PROGRAM_DECL;

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

    expected<ebm::StructDecl> StatementConverter::convert_struct_decl(ebm::IdentifierRef name, const std::shared_ptr<ast::StructType>& node) {
        ebm::StructDecl struct_decl;
        struct_decl.name = name;
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
        body.kind = ebm::StatementOp::STRUCT_DECL;
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

        EBM_DEFINE_ANONYMOUS_VARIABLE(writer, encoder_input, {});
        EBM_DEFINE_ANONYMOUS_VARIABLE(reader, decoder_input, {});
        ctx.state().add_format_encode_decode(node, encode, enc_type, writer, decode, dec_type, reader);
        const auto _node = ctx.state().set_current_node(node);
        auto handle = [&](ebm::StatementRef fn_ref, std::shared_ptr<ast::Function> fn, ebm::StatementRef coder_input, GenerateType typ) -> expected<void> {
            const auto _mode = ctx.state().set_current_generate_type(typ);
            ebm::FunctionDecl derived_fn;
            if (fn) {
                MAYBE(decl, ctx.get_statement_converter().convert_function_decl(fn, typ, coder_input));
                derived_fn = std::move(decl);
            }
            else {
                derived_fn.parent_format = id;
                EBMA_ADD_IDENTIFIER(enc_name, typ == GenerateType::Encode ? "encode" : "decode");
                MAYBE(coder_return, get_coder_return(ctx, typ == GenerateType::Encode));
                derived_fn.name = enc_name;
                derived_fn.return_type = coder_return;
                append(derived_fn.params, coder_input);
                EBMA_CONVERT_STATEMENT(body, node->body);
                derived_fn.body = body;
            }
            ebm::StatementBody b;
            b.kind = ebm::StatementOp::FUNCTION_DECL;
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
        body.kind = ebm::StatementOp::ENUM_DECL;
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
        body.kind = ebm::StatementOp::ENUM_MEMBER_DECL;
        body.enum_member_decl(std::move(ebm_enum_member_decl));
        return {};
    }

    expected<ebm::FunctionDecl> StatementConverter::convert_function_decl(const std::shared_ptr<ast::Function>& node, GenerateType typ, ebm::StatementRef coder_input_ref) {
        ebm::FunctionDecl func_decl;
        if (auto parent = node->belong.lock()) {
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
            EBM_DEFINE_VARIABLE(param_ref, param_name_ref, param_type_ref, {}, true, false);
            append(func_decl.params, param_ref_def);
        }
        EBMA_CONVERT_STATEMENT(fn_body, node->body);
        func_decl.body = fn_body;
        return func_decl;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Function>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::FUNCTION_DECL;
        MAYBE(decl, convert_function_decl(node, GenerateType::Normal, {}));
        body.func_decl(std::move(decl));
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Metadata>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::METADATA;
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
        expected<void> result;
        ast::visit(node->statement, [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node->statement)>, std::shared_ptr>::template param_at<0>;
            result = convert_statement_impl(n, id, body);
        });
        return result;
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::State>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::STRUCT_DECL;
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

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Field>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        if (auto union_type = ast::as<ast::UnionType>(node->field_type)) {
            body.kind = ebm::StatementOp::PROPERTY_DECL;
            ebm::PropertyDecl prop_decl;
            EBMA_ADD_IDENTIFIER(field_name_ref, node->ident->ident);
            prop_decl.name = field_name_ref;

            EBMA_CONVERT_TYPE(property_type_ref, union_type->common_type);
            prop_decl.property_type = property_type_ref;

            if (auto parent_member = node->belong.lock()) {
                EBMA_CONVERT_STATEMENT(statement_ref, parent_member);
                prop_decl.parent_format = statement_ref;
            }

            prop_decl.merge_mode = ebm::MergeMode::COMMON_TYPE;

            body.property_decl(std::move(prop_decl));
        }
        else {
            if (!node->is_state_variable && ctx.state().get_current_generate_type() == GenerateType::Encode) {
                MAYBE(def_ref, ctx.state().is_visited(node, GenerateType::Normal));
                auto def = ctx.repository().get_statement(def_ref)->body.field_decl();
                EBM_IDENTIFIER(def_id, def_ref, def->field_type);
                MAYBE(body_, ctx.get_encoder_converter().encode_field_type(node->field_type, def_id, node));
                body = std::move(body_);
            }
            else if (!node->is_state_variable && ctx.state().get_current_generate_type() == GenerateType::Decode) {
                MAYBE(def_ref, ctx.state().is_visited(node, GenerateType::Normal));
                auto def = ctx.repository().get_statement(def_ref)->body.field_decl();
                EBM_IDENTIFIER(def_id, def_ref, def->field_type);
                MAYBE(body_, ctx.get_decoder_converter().decode_field_type(node->field_type, def_id, node));
                body = std::move(body_);
            }
            else {
                body.kind = ebm::StatementOp::FIELD_DECL;
                ebm::FieldDecl field_decl;
                if (node->ident) {
                    EBMA_ADD_IDENTIFIER(field_name_ref, node->ident->ident);
                    field_decl.name = field_name_ref;
                }
                EBMA_CONVERT_TYPE(type_ref, node->field_type);
                field_decl.field_type = type_ref;
                field_decl.is_state_variable(node->is_state_variable);
                if (auto locked = node->belong.lock()) {
                    MAYBE(parent_member_ref, ctx.state().is_visited(locked, GenerateType::Normal));
                    field_decl.parent_struct = parent_member_ref;
                }
                body.field_decl(std::move(field_decl));
            }
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ExplicitError>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::ERROR_REPORT;
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
        body.kind = ebm::StatementOp::IMPORT_MODULE;
        EBMA_ADD_IDENTIFIER(module_name_ref, node->path);
        body.module_name(module_name_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::ImplicitYield>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        body.kind = ebm::StatementOp::EXPRESSION;
        EBMA_CONVERT_EXPRESSION(expr_ref, node->expr);
        body.expression(expr_ref);
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Binary>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        auto assign_with_op = convert_assignment_binary_op(node->op);
        if (node->op == ast::BinaryOp::assign) {
            body.kind = ebm::StatementOp::ASSIGNMENT;
            EBMA_CONVERT_EXPRESSION(target_ref, node->left);
            body.target(target_ref);
            EBMA_CONVERT_EXPRESSION(value_ref, node->right);
            body.value(value_ref);
        }
        else if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            body.kind = ebm::StatementOp::VARIABLE_DECL;
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
            body.kind = ebm::StatementOp::ASSIGNMENT;
            EBMA_CONVERT_EXPRESSION(calc, ast::cast_to<ast::Expr>(node));
            EBMA_CONVERT_EXPRESSION(target_ref, node->left);
            body.target(target_ref);
            body.value(calc);
        }
        else {
            body.kind = ebm::StatementOp::EXPRESSION;
            EBMA_CONVERT_EXPRESSION(expr_ref, ast::cast_to<ast::Expr>(node));
            body.expression(expr_ref);
        }
        return {};
    }

    expected<void> StatementConverter::convert_statement_impl(const std::shared_ptr<ast::Node>& node, ebm::StatementRef id, ebm::StatementBody& body) {
        if (ast::as<ast::Expr>(node)) {
            body.kind = ebm::StatementOp::EXPRESSION;
            EBMA_CONVERT_EXPRESSION(expr_ref, ast::cast_to<ast::Expr>(node));
            body.expression(expr_ref);
            return {};
        }
        return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
    }

}  // namespace ebmgen
