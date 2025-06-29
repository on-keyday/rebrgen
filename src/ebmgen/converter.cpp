#include "converter.hpp"
#include <core/ast/traverse.h>
#include <core/ast/tool/ident.h>

namespace ebmgen {

    expected<ebm::TypeRef> Converter::convert_type(const std::shared_ptr<ast::Type>& type) {
        ebm::TypeBody body;
        if (auto int_type = ast::as<ast::IntType>(type)) {
            if (int_type->is_signed) {
                body.kind = ebm::TypeKind::INT;
            }
            else {
                body.kind = ebm::TypeKind::UINT;
            }
            if (!int_type->bit_size) {
                return unexpect_error("IntType must have a bit_size");
            }
            body.size(*int_type->bit_size);
        }
        else if (auto bool_type = ast::as<ast::BoolType>(type)) {
            body.kind = ebm::TypeKind::BOOL;
        }
        else if (auto float_type = ast::as<ast::FloatType>(type)) {
            body.kind = ebm::TypeKind::FLOAT;
            if (!float_type->bit_size) {
                return unexpect_error("FloatType must have a bit_size");
            }
            body.size(*float_type->bit_size);
        }
        else if (auto ident_type = ast::as<ast::IdentType>(type)) {
            if (auto locked = ident_type->base.lock()) {
                return convert_type(locked);
            }
            return unexpect_error("IdentType has no base type");
        }
        else if (auto array_type = ast::as<ast::ArrayType>(type)) {
            body.kind = ebm::TypeKind::ARRAY;
            auto element_type_ref = convert_type(array_type->element_type);
            if (!element_type_ref) {
                return unexpect_error(std::move(element_type_ref.error()));
            }
            body.element_type(*element_type_ref);
            if (array_type->length) {
                auto length_expr_ref = convert_expr(array_type->length);
                if (!length_expr_ref) {
                    return unexpect_error(std::move(length_expr_ref.error()));
                }
                // TODO: Convert ExpressionRef to Varint for length
                // For now, just setting a dummy Varint
                body.length(ebm::Varint{0});
            }
        }
        else if (auto int_literal_type = ast::as<ast::IntLiteralType>(type)) {
            body.kind = ebm::TypeKind::UINT;  // Assuming unsigned for now
            if (auto locked_literal = int_literal_type->base.lock()) {
                auto val = locked_literal->parse_as<std::uint64_t>();
                if (!val) {
                    return unexpect_error("Failed to parse IntLiteralType value");
                }
                // Determine bit size based on the value
                // This is a simplified approach; a more robust solution might involve analyzing the original brgen type
                if (*val <= 0xFF) {  // 8-bit
                    body.size(8);
                }
                else if (*val <= 0xFFFF) {  // 16-bit
                    body.size(16);
                }
                else if (*val <= 0xFFFFFFFF) {  // 32-bit
                    body.size(32);
                }
                else {  // 64-bit
                    body.size(64);
                }
            }
            else {
                return unexpect_error("IntLiteralType has no base literal");
            }
        }
        else if (auto str_literal_type = ast::as<ast::StrLiteralType>(type)) {
            body.kind = ebm::TypeKind::ARRAY;
            auto element_type_ref = convert_type(std::make_shared<ast::IntType>(str_literal_type->loc, 8, ast::Endian::unspec, false));
            if (!element_type_ref) {
                return unexpect_error(std::move(element_type_ref.error()));
            }
            body.element_type(*element_type_ref);
            if (str_literal_type->bit_size) {
                auto length = varint(*str_literal_type->bit_size / 8);
                if (!length) {
                    return unexpect_error(std::move(length.error()));
                }
                body.length(*length);
            }
        }
        else if (auto enum_type = ast::as<ast::EnumType>(type)) {
            body.kind = ebm::TypeKind::ENUM;
            if (auto locked_enum = enum_type->base.lock()) {
                auto name_ref = add_identifier(locked_enum->ident->ident);
                if (!name_ref) {
                    return unexpect_error(std::move(name_ref.error()));
                }
                body.name(*name_ref);
            }
            else {
                return unexpect_error("EnumType has no base enum");
            }
        }
        else if (auto struct_type = ast::as<ast::StructType>(type)) {
            body.kind = ebm::TypeKind::STRUCT;
            if (auto locked_base = struct_type->base.lock()) {
                if (auto format = ast::as<ast::Format>(locked_base)) {
                    auto name_ref = add_identifier(format->ident->ident);
                    if (!name_ref) {
                        return unexpect_error(std::move(name_ref.error()));
                    }
                    body.name(*name_ref);
                }
                else {
                    return unexpect_error("Unsupported base type for StructType");
                }
            }
            else {
                return unexpect_error("StructType has no base");
            }
        }
        else {
            return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
        }
        return add_type(std::move(body));
    }

    expected<ebm::BinaryOp> convert_binary_op(ast::BinaryOp op) {
        switch (op) {
            case ast::BinaryOp::add:
                return ebm::BinaryOp::add;
            case ast::BinaryOp::sub:
                return ebm::BinaryOp::sub;
            case ast::BinaryOp::mul:
                return ebm::BinaryOp::mul;
            case ast::BinaryOp::div:
                return ebm::BinaryOp::div;
            case ast::BinaryOp::mod:
                return ebm::BinaryOp::mod;
            case ast::BinaryOp::equal:
                return ebm::BinaryOp::equal;
            case ast::BinaryOp::not_equal:
                return ebm::BinaryOp::not_equal;
            case ast::BinaryOp::less:
                return ebm::BinaryOp::less;
            case ast::BinaryOp::less_or_eq:
                return ebm::BinaryOp::less_or_eq;
            case ast::BinaryOp::grater:
                return ebm::BinaryOp::grater;
            case ast::BinaryOp::grater_or_eq:
                return ebm::BinaryOp::grater_or_eq;
            case ast::BinaryOp::logical_and:
                return ebm::BinaryOp::logical_and;
            case ast::BinaryOp::logical_or:
                return ebm::BinaryOp::logical_or;
            case ast::BinaryOp::left_logical_shift:
                return ebm::BinaryOp::left_shift;
            case ast::BinaryOp::right_logical_shift:
                return ebm::BinaryOp::right_shift;
            case ast::BinaryOp::bit_and:
                return ebm::BinaryOp::bit_and;
            case ast::BinaryOp::bit_or:
                return ebm::BinaryOp::bit_or;
            case ast::BinaryOp::bit_xor:
                return ebm::BinaryOp::bit_xor;
            default:
                return unexpect_error("Unsupported binary operator");
        }
    }

    expected<ebm::UnaryOp> convert_unary_op(ast::UnaryOp op) {
        switch (op) {
            case ast::UnaryOp::not_:
                return ebm::UnaryOp::logical_not;
            case ast::UnaryOp::minus_sign:
                return ebm::UnaryOp::minus_sign;
            default:
                return unexpect_error("Unsupported unary operator");
        }
    }

    ebm::Identifier* Converter::get_identifier(ebm::IdentifierRef ref) {
        if (ref.id.value() == 0 || identifier_map.find(ref.id.value()) == identifier_map.end()) {
            return nullptr;
        }
        return &ebm.identifiers[identifier_map[ref.id.value()]];
    }

    ebm::StringLiteral* Converter::get_string(ebm::StringRef ref) {
        if (ref.id.value() == 0 || string_map.find(ref.id.value()) == string_map.end()) {
            return nullptr;
        }
        return &ebm.strings[string_map[ref.id.value()]];
    }

    ebm::Type* Converter::get_type(ebm::TypeRef ref) {
        if (ref.id.value() == 0 || type_map.find(ref.id.value()) == type_map.end()) {
            return nullptr;
        }
        return &ebm.types[type_map[ref.id.value()]];
    }

    ebm::Expression* Converter::get_expression(ebm::ExpressionRef ref) {
        if (ref.id.value() == 0 || expression_map.find(ref.id.value()) == expression_map.end()) {
            return nullptr;
        }
        return &ebm.expressions[expression_map[ref.id.value()]];
    }

    ebm::Statement* Converter::get_statement(ebm::StatementRef ref) {
        if (ref.id.value() == 0 || statement_map.find(ref.id.value()) == statement_map.end()) {
            return nullptr;
        }
        return &ebm.statements[statement_map[ref.id.value()]];
    }

    expected<ebm::CastType> Converter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = get_type(dest_ref);
        auto src = get_type(src_ref);

        if (!dest || !src) {
            return unexpect_error("Invalid type reference for cast");
        }

        if (dest->body.kind == ebm::TypeKind::INT || dest->body.kind == ebm::TypeKind::UINT) {
            if (src->body.kind == ebm::TypeKind::ENUM) {
                return ebm::CastType::ENUM_TO_INT;
            }
            if (src->body.kind == ebm::TypeKind::FLOAT) {
                return ebm::CastType::FLOAT_TO_INT_BIT;
            }
            if (src->body.kind == ebm::TypeKind::BOOL) {
                return ebm::CastType::BOOL_TO_INT;
            }
            // Handle int/uint size and signedness conversions
            if ((src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) && dest->body.size() && src->body.size()) {
                auto dest_size = *dest->body.size();
                auto src_size = *src->body.size();
                if (dest_size < src_size) {
                    return ebm::CastType::LARGE_INT_TO_SMALL_INT;
                }
                if (dest_size > src_size) {
                    return ebm::CastType::SMALL_INT_TO_LARGE_INT;
                }
                // Check signedness conversion
                if (dest->body.kind == ebm::TypeKind::UINT && src->body.kind == ebm::TypeKind::INT) {
                    return ebm::CastType::SIGNED_TO_UNSIGNED;
                }
                if (dest->body.kind == ebm::TypeKind::INT && src->body.kind == ebm::TypeKind::UINT) {
                    return ebm::CastType::UNSIGNED_TO_SIGNED;
                }
            }
        }
        else if (dest->body.kind == ebm::TypeKind::ENUM) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_ENUM;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::FLOAT) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_FLOAT_BIT;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::BOOL) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_BOOL;
            }
        }
        // TODO: Add more complex type conversions (ARRAY, VECTOR, STRUCT, RECURSIVE_STRUCT)

        return ebm::CastType::OTHER;
    }

    void Converter::convert_node(const std::shared_ptr<ast::Node>& node) {
        if (err) return;
        auto r = convert_statement(node);
        if (!r) {
            err = std::move(r.error());
        }
    }

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
                    if (auto field = ast::as<ast::Field>(element)) {
                        ebm::FieldDecl field_decl;
                        auto field_name_ref = add_identifier(field->ident->ident);
                        if (!field_name_ref) {
                            return unexpect_error(std::move(field_name_ref.error()));
                        }
                        field_decl.name = *field_name_ref;
                        auto type_ref = convert_type(field->field_type);
                        if (!type_ref) {
                            return unexpect_error(std::move(type_ref.error()));
                        }
                        field_decl.field_type = *type_ref;
                        ebm::StatementBody field_body;
                        field_body.statement_kind = ebm::StatementOp::FIELD_DECL;
                        field_body.field_decl(std::move(field_decl));
                        auto field_ref = add_statement(std::move(field_body));
                        if (!field_ref) {
                            return unexpect_error(std::move(field_ref.error()));
                        }
                        append(struct_decl.fields, *field_ref);
                    }
                    else {
                        return unexpect_error("Unsupported node type in format body: {}", node_type_to_string(element->node_type));
                    }
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
        // TODO: Implement conversion for different statement types
        return unexpect_error("Statement conversion not implemented yet: {}", node_type_to_string(node->node_type));
    }

    expected<ebm::ExpressionRef> Converter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        ebm::ExpressionBody body;
        auto type_ref = convert_type(node->expr_type);
        if (!type_ref) {
            return unexpect_error(std::move(type_ref.error()));
        }
        body.type = *type_ref;

        if (auto literal = ast::as<ast::IntLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_INT;
            auto value = literal->parse_as<std::uint64_t>();
            if (!value) {
                return unexpect_error("cannot parse int literal");
            }
            body.int_value(*value);
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::BoolLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_BOOL;
            body.bool_value(literal->value);
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::StrLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_STRING;
            auto str_ref = add_string(literal->value);
            if (!str_ref) {
                return unexpect_error(std::move(str_ref.error()));
            }
            body.string_value(*str_ref);
            return add_expr(std::move(body));
        }
        else if (auto literal = ast::as<ast::TypeLiteral>(node)) {
            body.op = ebm::ExpressionOp::LITERAL_TYPE;
            auto type_ref = convert_type(literal->type_literal);
            if (!type_ref) {
                return unexpect_error(std::move(type_ref.error()));
            }
            body.type_ref(*type_ref);
            return add_expr(std::move(body));
        }
        else if (auto ident = ast::as<ast::Ident>(node)) {
            body.op = ebm::ExpressionOp::IDENTIFIER;
            auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (!base) {
                return unexpect_error("Identifier {} not found", ident->ident);
            };
            auto id_ref = convert_statement(base->first->base.lock());
            if (!id_ref) {
                return unexpect_error(std::move(id_ref.error()));
            }
            body.id(*id_ref);
            return add_expr(std::move(body));
        }
        else if (auto binary = ast::as<ast::Binary>(node)) {
            body.op = ebm::ExpressionOp::BINARY_OP;
            auto left_ref = convert_expr(binary->left);
            if (!left_ref) {
                return unexpect_error(std::move(left_ref.error()));
            }
            auto right_ref = convert_expr(binary->right);
            if (!right_ref) {
                return unexpect_error(std::move(right_ref.error()));
            }
            body.left(*left_ref);
            body.right(*right_ref);
            auto bop = convert_binary_op(binary->op);
            if (!bop) {
                return unexpect_error(std::move(bop.error()));
            }
            body.bop(*bop);
            return add_expr(std::move(body));
        }
        else if (auto unary = ast::as<ast::Unary>(node)) {
            body.op = ebm::ExpressionOp::UNARY_OP;
            auto operand_ref = convert_expr(unary->expr);
            if (!operand_ref) {
                return unexpect_error(std::move(operand_ref.error()));
            }
            body.operand(*operand_ref);
            auto uop = convert_unary_op(unary->op);
            if (!uop) {
                return unexpect_error(std::move(uop.error()));
            }
            body.uop(*uop);
            return add_expr(std::move(body));
        }
        else if (auto index_expr = ast::as<ast::Index>(node)) {
            body.op = ebm::ExpressionOp::INDEX_ACCESS;
            auto base_ref = convert_expr(index_expr->expr);
            if (!base_ref) {
                return unexpect_error(std::move(base_ref.error()));
            }
            auto index_ref = convert_expr(index_expr->index);
            if (!index_ref) {
                return unexpect_error(std::move(index_ref.error()));
            }
            body.base(*base_ref);
            body.index(*index_ref);
            return add_expr(std::move(body));
        }
        else if (auto member_access = ast::as<ast::MemberAccess>(node)) {
            body.op = ebm::ExpressionOp::MEMBER_ACCESS;
            auto base_ref = convert_expr(member_access->target);
            if (!base_ref) {
                return unexpect_error(std::move(base_ref.error()));
            }
            auto member_ref = add_identifier(member_access->member->ident);
            if (!member_ref) {
                return unexpect_error(std::move(member_ref.error()));
            }
            body.base(*base_ref);
            body.member(*member_ref);
            return add_expr(std::move(body));
        }
        else if (auto cast_expr = ast::as<ast::Cast>(node)) {
            body.op = ebm::ExpressionOp::TYPE_CAST;
            auto target_type_ref = convert_type(cast_expr->expr_type);
            if (!target_type_ref) {
                return unexpect_error(std::move(target_type_ref.error()));
            }
            auto source_expr_ref = convert_expr(cast_expr->arguments[0]);
            if (!source_expr_ref) {
                return unexpect_error(std::move(source_expr_ref.error()));
            }
            body.target_type(*target_type_ref);
            auto source_expr_type_ref = convert_type(cast_expr->arguments[0]->expr_type);
            if (!source_expr_type_ref) {
                return unexpect_error(std::move(source_expr_type_ref.error()));
            }
            body.source_expr(*source_expr_ref);
            auto cast_kind = get_cast_type(*target_type_ref, *source_expr_type_ref);
            if (!cast_kind) {
                return unexpect_error(std::move(cast_kind.error()));
            }
            body.cast_kind(*cast_kind);
            return add_expr(std::move(body));
        }
        else {
            return unexpect_error("not implemented yet");
        }
    }

    Error Converter::set_lengths() {
        auto identifiers_len = varint(ebm.identifiers.size());
        if (!identifiers_len) {
            return identifiers_len.error();
        }
        ebm.identifiers_len = *identifiers_len;

        auto strings_len = varint(ebm.strings.size());
        if (!strings_len) {
            return strings_len.error();
        }
        ebm.strings_len = *strings_len;

        auto types_len = varint(ebm.types.size());
        if (!types_len) {
            return types_len.error();
        }
        ebm.types_len = *types_len;

        auto statements_len = varint(ebm.statements.size());
        if (!statements_len) {
            return statements_len.error();
        }
        ebm.statements_len = *statements_len;

        auto expressions_len = varint(ebm.expressions.size());
        if (!expressions_len) {
            return expressions_len.error();
        }
        ebm.expressions_len = *expressions_len;

        return Error();
    }

    Error Converter::convert(const std::shared_ptr<brgen::ast::Node>& ast_root) {
        root = ast_root;
        convert_node(ast_root);
        if (err) {
            return err;
        }
        return set_lengths();
    }

}  // namespace ebmgen
