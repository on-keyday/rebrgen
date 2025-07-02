#include "debug_printer.hpp"

#include <iostream>
#include <iomanip>

namespace ebmgen {

    void DebugPrinter::print_module() const {
        os_ << "ExtendedBinaryModule:" << std::endl;
        indent_level_ = 1;
        indent();
        os_ << "Version: " << static_cast<int>(module_.version) << std::endl;
        indent();
        os_ << "Max ID: " << module_.max_id.id.value() << std::endl;

        indent();
        os_ << "Identifiers (" << module_.identifiers.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& ident : module_.identifiers) {
            print_identifier(ident);
        }
        indent_level_--;

        indent();
        os_ << "String Literals (" << module_.strings.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& str_lit : module_.strings) {
            print_string_literal(str_lit);
        }
        indent_level_--;

        indent();
        os_ << "Types (" << module_.types.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& type : module_.types) {
            print_type(type);
        }
        indent_level_--;

        indent();
        os_ << "Statements (" << module_.statements.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& stmt : module_.statements) {
            print_statement(stmt);
        }
        indent_level_--;

        indent();
        os_ << "Expressions (" << module_.expressions.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& expr : module_.expressions) {
            print_expression(expr);
        }
        indent_level_--;

        indent();
        os_ << "Debug Info:" << std::endl;
        indent_level_++;
        print_debug_info(module_.debug_info);
        indent_level_--;
        indent_level_ = 0;
    }

    const ebm::Identifier* DebugPrinter::get_identifier(const ebm::IdentifierRef& ref) const {
        for (const auto& ident : module_.identifiers) {
            if (ident.id.id.value() == ref.id.value()) {
                return &ident;
            }
        }
        return nullptr;
    }

    const ebm::StringLiteral* DebugPrinter::get_string_literal(const ebm::StringRef& ref) const {
        for (const auto& str_lit : module_.strings) {
            if (str_lit.id.id.value() == ref.id.value()) {
                return &str_lit;
            }
        }
        return nullptr;
    }

    const ebm::Type* DebugPrinter::get_type(const ebm::TypeRef& ref) const {
        for (const auto& type : module_.types) {
            if (type.id.id.value() == ref.id.value()) {
                return &type;
            }
        }
        return nullptr;
    }

    const ebm::Statement* DebugPrinter::get_statement(const ebm::StatementRef& ref) const {
        for (const auto& stmt : module_.statements) {
            if (stmt.id.id.value() == ref.id.value()) {
                return &stmt;
            }
        }
        return nullptr;
    }

    const ebm::Expression* DebugPrinter::get_expression(const ebm::ExpressionRef& ref) const {
        for (const auto& expr : module_.expressions) {
            if (expr.id.id.value() == ref.id.value()) {
                return &expr;
            }
        }
        return nullptr;
    }

    void DebugPrinter::print_identifier(const ebm::Identifier& ident) const {
        indent();
        os_ << "ID: " << ident.id.id.value() << ", Name: \"" << ident.name.data << "\"" << std::endl;
    }

    void DebugPrinter::print_string_literal(const ebm::StringLiteral& str_lit) const {
        indent();
        os_ << "ID: " << str_lit.id.id.value() << ", Value: \"" << str_lit.value.data << "\"" << std::endl;
    }

    void DebugPrinter::print_type(const ebm::Type& type) const {
        indent();
        os_ << "Type ID: " << type.id.id.value() << std::endl;
        indent_level_++;
        print_type_body(type.body);
        indent_level_--;
    }

    void DebugPrinter::print_type_body(const ebm::TypeBody& body) const {
        indent();
        os_ << "Kind: " << ebm::to_string(body.kind) << std::endl;
        indent_level_++;
        switch (body.kind) {
            case ebm::TypeKind::INT: {
                if (auto size = body.size()) {
                    indent();
                    os_ << "Size: " << static_cast<int>(*size) << std::endl;
                }
                break;
            }
            case ebm::TypeKind::UINT: {
                if (auto size = body.size()) {
                    indent();
                    os_ << "Size: " << static_cast<int>(*size) << std::endl;
                }
                break;
            }
            case ebm::TypeKind::FLOAT: {
                if (auto size = body.size()) {
                    indent();
                    os_ << "Size: " << static_cast<int>(*size) << std::endl;
                }
                break;
            }
            case ebm::TypeKind::STRUCT: {
                if (auto id = body.id()) {
                    indent();
                    os_ << "Struct ID: " << id->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                if (auto id = body.id()) {
                    indent();
                    os_ << "Recursive Struct ID: " << id->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::BOOL:
            case ebm::TypeKind::VOID:
            case ebm::TypeKind::META: {
                // No additional fields
                break;
            }
            case ebm::TypeKind::ENUM: {
                if (auto id = body.id()) {
                    indent();
                    os_ << "Enum ID: " << id->id.value() << std::endl;
                }
                if (auto base_type = body.base_type()) {
                    indent();
                    os_ << "Base Type ID: " << base_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::ARRAY: {
                if (auto element_type = body.element_type()) {
                    indent();
                    os_ << "Element Type ID: " << element_type->id.value() << std::endl;
                }
                if (auto length = body.length()) {
                    indent();
                    os_ << "Length: " << length->value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::VECTOR: {
                if (auto element_type = body.element_type()) {
                    indent();
                    os_ << "Element Type ID: " << element_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::VARIANT: {
                if (auto common_type = body.common_type()) {
                    indent();
                    os_ << "Common Type ID: " << common_type->id.value() << std::endl;
                }
                if (auto members = body.members()) {
                    indent();
                    os_ << "Members:" << std::endl;
                    indent_level_++;
                    for (const auto& member_ref : members->container) {
                        indent();
                        os_ << "- Member Type ID: " << member_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::TypeKind::RANGE: {
                if (auto base_type = body.base_type()) {
                    indent();
                    os_ << "Base Type ID: " << base_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::CODER_RETURN: {
                if (auto coder_type = body.coder_type()) {
                    indent();
                    os_ << "Coder Type ID: " << coder_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::PROPERTY_SETTER_RETURN: {
                if (auto property_type = body.property_type()) {
                    indent();
                    os_ << "Property Type ID: " << property_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::OPTIONAL: {
                if (auto inner_type = body.inner_type()) {
                    indent();
                    os_ << "Inner Type ID: " << inner_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::TypeKind::PTR: {
                if (auto pointee_type = body.pointee_type()) {
                    indent();
                    os_ << "Pointee Type ID: " << pointee_type->id.value() << std::endl;
                }
                break;
            }
            default: {
                indent();
                os_ << "(Unhandled TypeKind)" << std::endl;
                break;
            }
        }
        indent_level_--;
    }

    void DebugPrinter::print_statement(const ebm::Statement& stmt) const {
        indent();
        os_ << "Statement ID: " << stmt.id.id.value() << std::endl;
        indent_level_++;
        print_statement_body(stmt.body);
        indent_level_--;
    }

    void DebugPrinter::print_statement_body(const ebm::StatementBody& body) const {
        indent();
        os_ << "Kind: " << ebm::to_string(body.statement_kind) << std::endl;
        indent_level_++;
        switch (body.statement_kind) {
            case ebm::StatementOp::ASSIGNMENT: {
                if (auto target = body.target()) {
                    indent();
                    os_ << "Target Expr ID: " << target->id.value() << std::endl;
                }
                if (auto value = body.value()) {
                    indent();
                    os_ << "Value Expr ID: " << value->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::RETURN: {
                if (auto value = body.value()) {
                    indent();
                    os_ << "Return Value Expr ID: " << value->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::ASSERT: {
                if (auto condition = body.condition()) {
                    indent();
                    os_ << "Condition Expr ID: " << condition->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::NEW_OBJECT: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto object_type = body.object_type()) {
                    indent();
                    os_ << "Object Type ID: " << object_type->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::READ_DATA: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto data_type = body.data_type()) {
                    indent();
                    os_ << "Data Type ID: " << data_type->id.value() << std::endl;
                }
                if (auto endian = body.endian()) {
                    indent();
                    os_ << "Endian: " << ebm::to_string(endian->endian()) << ", Signed: " << (endian->sign() ? "true" : "false") << std::endl;
                }
                if (auto bit_size = body.bit_size()) {
                    indent();
                    os_ << "Bit Size: " << bit_size->value() << std::endl;
                }
                if (auto fallback_stmt = body.fallback_stmt()) {
                    indent();
                    os_ << "Fallback Statements (" << fallback_stmt->container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& fb_stmt : fallback_stmt->container) {
                        indent();
                        os_ << "Fallback Type: " << ebm::to_string(fb_stmt.fallback_type) << std::endl;
                        indent_level_++;
                        for (const auto& stmt_ref : fb_stmt.block.container) {
                            indent();
                            os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                        }
                        indent_level_--;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::WRITE_DATA: {
                if (auto source_expr = body.source_expr()) {
                    indent();
                    os_ << "Source Expr ID: " << source_expr->id.value() << std::endl;
                }
                if (auto data_type = body.data_type()) {
                    indent();
                    os_ << "Data Type ID: " << data_type->id.value() << std::endl;
                }
                if (auto endian = body.endian()) {
                    indent();
                    os_ << "Endian: " << ebm::to_string(endian->endian()) << ", Signed: " << (endian->sign() ? "true" : "false") << std::endl;
                }
                if (auto bit_size = body.bit_size()) {
                    indent();
                    os_ << "Bit Size: " << bit_size->value() << std::endl;
                }
                if (auto fallback_stmt = body.fallback_stmt()) {
                    indent();
                    os_ << "Fallback Statements (" << fallback_stmt->container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& fb_stmt : fallback_stmt->container) {
                        indent();
                        os_ << "Fallback Type: " << ebm::to_string(fb_stmt.fallback_type) << std::endl;
                        indent_level_++;
                        for (const auto& stmt_ref : fb_stmt.block.container) {
                            indent();
                            os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                        }
                        indent_level_--;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::SEEK_STREAM: {
                if (auto offset = body.offset()) {
                    indent();
                    os_ << "Offset Expr ID: " << offset->id.value() << std::endl;
                }
                if (auto stream_type = body.stream_type()) {
                    indent();
                    os_ << "Stream Type: " << ebm::to_string(*stream_type) << std::endl;
                }
                break;
            }
            case ebm::StatementOp::GET_STREAM_OFFSET: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto stream_type = body.stream_type()) {
                    indent();
                    os_ << "Stream Type: " << ebm::to_string(*stream_type) << std::endl;
                }
                break;
            }
            case ebm::StatementOp::GET_REMAINING_BYTES: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto stream_type = body.stream_type()) {
                    indent();
                    os_ << "Stream Type: " << ebm::to_string(*stream_type) << std::endl;
                }
                break;
            }
            case ebm::StatementOp::CAN_READ_STREAM: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto stream_type = body.stream_type()) {
                    indent();
                    os_ << "Stream Type: " << ebm::to_string(*stream_type) << std::endl;
                }
                if (auto num_bytes = body.num_bytes()) {
                    indent();
                    os_ << "Num Bytes Expr ID: " << num_bytes->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::IF_STATEMENT: {
                if (auto if_stmt = body.if_statement()) {
                    indent();
                    os_ << "Condition Expr ID: " << if_stmt->condition.id.value() << std::endl;
                    indent();
                    os_ << "Then Block (" << if_stmt->then_block.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : if_stmt->then_block.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                    indent();
                    os_ << "Else Block (" << if_stmt->else_block.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : if_stmt->else_block.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::LOOP_STATEMENT: {
                if (auto loop_stmt = body.loop()) {
                    indent();
                    os_ << "Loop Type: " << ebm::to_string(loop_stmt->loop_type) << std::endl;
                    if (loop_stmt->loop_type == ebm::LoopType::WHILE) {
                        if (auto condition = loop_stmt->condition()) {
                            indent();
                            os_ << "Condition Expr ID: " << condition->id.value() << std::endl;
                        }
                    }
                    else if (loop_stmt->loop_type == ebm::LoopType::FOR_EACH) {
                        if (auto item_var = loop_stmt->item_var()) {
                            indent();
                            os_ << "Item Var ID: " << item_var->id.value() << std::endl;
                        }
                        if (auto collection = loop_stmt->collection()) {
                            indent();
                            os_ << "Collection Expr ID: " << collection->id.value() << std::endl;
                        }
                    }
                    indent();
                    os_ << "Body Block (" << loop_stmt->body.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : loop_stmt->body.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::MATCH_STATEMENT: {
                if (auto match_stmt = body.match_statement()) {
                    indent();
                    os_ << "Target Expr ID: " << match_stmt->target.id.value() << std::endl;
                    indent();
                    os_ << "Is Exhaustive: " << (match_stmt->is_exhaustive() ? "true" : "false") << std::endl;
                    indent();
                    os_ << "Branches (" << match_stmt->branches.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : match_stmt->branches.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::MATCH_BRANCH: {
                if (auto match_branch = body.match_branch()) {
                    indent();
                    os_ << "Condition Expr ID: " << match_branch->condition.id.value() << std::endl;
                    indent();
                    os_ << "Body Block (" << match_branch->body.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : match_branch->body.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::BREAK: {
                if (auto break_stmt = body.break_()) {
                    indent();
                    os_ << "Related Statement ID: " << break_stmt->related_statement.id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::CONTINUE: {
                if (auto continue_stmt = body.continue_()) {
                    indent();
                    os_ << "Related Statement ID: " << continue_stmt->related_statement.id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::FUNCTION_DECL: {
                if (auto func_decl = body.func_decl()) {
                    indent();
                    os_ << "Name ID: " << func_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Return Type ID: " << func_decl->return_type.id.value() << std::endl;
                    indent();
                    os_ << "Params Block (" << func_decl->params.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : func_decl->params.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                    if (func_decl->parent_format.id.value() != 0) {
                        indent();
                        os_ << "Parent Format ID: " << func_decl->parent_format.id.value() << std::endl;
                    }
                }
                break;
            }
            case ebm::StatementOp::VARIABLE_DECL: {
                if (auto var_decl = body.var_decl()) {
                    indent();
                    os_ << "Name ID: " << var_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Var Type ID: " << var_decl->var_type.id.value() << std::endl;
                    if (var_decl->initial_value.id.value() != 0) {
                        indent();
                        os_ << "Initial Value Expr ID: " << var_decl->initial_value.id.value() << std::endl;
                    }
                    indent();
                    os_ << "Is Constant: " << (var_decl->is_constant() ? "true" : "false") << std::endl;
                }
                break;
            }
            case ebm::StatementOp::FIELD_DECL: {
                if (auto field_decl = body.field_decl()) {
                    indent();
                    os_ << "Name ID: " << field_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Field Type ID: " << field_decl->field_type.id.value() << std::endl;
                    indent();
                    os_ << "Parent Struct ID: " << field_decl->parent_struct.id.value() << std::endl;
                    indent();
                    os_ << "Is State Variable: " << (field_decl->is_state_variable() ? "true" : "false") << std::endl;
                }
                break;
            }
            case ebm::StatementOp::ENUM_DECL: {
                if (auto enum_decl = body.enum_decl()) {
                    indent();
                    os_ << "Name ID: " << enum_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Base Type ID: " << enum_decl->base_type.id.value() << std::endl;
                    indent();
                    os_ << "Members Block (" << enum_decl->members.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : enum_decl->members.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::ENUM_MEMBER_DECL: {
                if (auto enum_member_decl = body.enum_member_decl()) {
                    indent();
                    os_ << "Name ID: " << enum_member_decl->name.id.value() << std::endl;
                    if (enum_member_decl->value.id.value() != 0) {
                        indent();
                        os_ << "Value Expr ID: " << enum_member_decl->value.id.value() << std::endl;
                    }
                    if (enum_member_decl->string_repr.id.value() != 0) {
                        indent();
                        os_ << "String Repr ID: " << enum_member_decl->string_repr.id.value() << std::endl;
                    }
                }
                break;
            }
            case ebm::StatementOp::STRUCT_DECL: {
                if (auto struct_decl = body.struct_decl()) {
                    indent();
                    os_ << "Name ID: " << struct_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Fields Block (" << struct_decl->fields.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : struct_decl->fields.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                    indent();
                    os_ << "Is Recursive: " << (struct_decl->is_recursive() ? "true" : "false") << std::endl;
                }
                break;
            }
            case ebm::StatementOp::UNION_DECL: {
                if (auto union_decl = body.union_decl()) {
                    indent();
                    os_ << "Name ID: " << union_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Parent Field ID: " << union_decl->parent_field.id.value() << std::endl;
                    indent();
                    os_ << "Members Block (" << union_decl->members.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : union_decl->members.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::UNION_MEMBER_DECL: {
                if (auto union_member_decl = body.union_member_decl()) {
                    indent();
                    os_ << "Name ID: " << union_member_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Field Type ID: " << union_member_decl->field_type.id.value() << std::endl;
                    indent();
                    os_ << "Is State Variable: " << (union_member_decl->is_state_variable() ? "true" : "false") << std::endl;
                    indent();
                    os_ << "Parent Union ID: " << union_member_decl->parent_union.id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::PROGRAM_DECL: {
                if (auto body_block = body.body()) {
                    indent();
                    os_ << "Body Block (" << body_block->container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : body_block->container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::STATE_DECL: {
                if (auto state_decl = body.state_decl()) {
                    indent();
                    os_ << "Name ID: " << state_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Body Block (" << state_decl->body.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& stmt_ref : state_decl->body.container) {
                        indent();
                        os_ << "- Statement Ref ID: " << stmt_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::BIT_FIELD_DECL: {
                if (auto bit_field_decl = body.bit_field_decl()) {
                    indent();
                    os_ << "Name ID: " << bit_field_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Parent Format ID: " << bit_field_decl->parent_format.id.value() << std::endl;
                    indent();
                    os_ << "Bit Size: " << bit_field_decl->bit_size.value() << std::endl;
                    indent();
                    os_ << "Packed Op Type: " << ebm::to_string(bit_field_decl->packed_op_type) << std::endl;
                }
                break;
            }
            case ebm::StatementOp::PROPERTY_DECL: {
                if (auto property_decl = body.property_decl()) {
                    indent();
                    os_ << "Name ID: " << property_decl->name.id.value() << std::endl;
                    indent();
                    os_ << "Parent Format ID: " << property_decl->parent_format.id.value() << std::endl;
                    indent();
                    os_ << "Property Type ID: " << property_decl->property_type.id.value() << std::endl;
                    indent();
                    os_ << "Merge Mode: " << ebm::to_string(property_decl->merge_mode) << std::endl;
                }
                break;
            }
            case ebm::StatementOp::METADATA: {
                if (auto metadata = body.metadata()) {
                    indent();
                    os_ << "Name ID: " << metadata->name.id.value() << std::endl;
                    indent();
                    os_ << "Values (" << metadata->values.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& expr_ref : metadata->values.container) {
                        indent();
                        os_ << "- Expression Ref ID: " << expr_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::IMPORT_MODULE: {
                if (auto module_name = body.module_name()) {
                    indent();
                    os_ << "Module Name ID: " << module_name->id.value() << std::endl;
                }
                if (auto alias = body.alias()) {
                    indent();
                    os_ << "Alias ID: " << alias->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::EXPRESSION: {
                if (auto expr = body.expression()) {
                    indent();
                    os_ << "Expression ID: " << expr->id.value() << std::endl;
                }
                break;
            }
            case ebm::StatementOp::PHI_NODE: {
                if (auto target_var = body.target_var()) {
                    indent();
                    os_ << "Target Var Expr ID: " << target_var->id.value() << std::endl;
                }
                if (auto params = body.params()) {
                    indent();
                    os_ << "Params (" << params->size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& param : *params) {
                        indent();
                        os_ << "- Condition Expr ID: " << param.condition.id.value() << ", Value Expr ID: " << param.value.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::StatementOp::ERROR_REPORT: {
                if (auto error_report = body.error_report()) {
                    indent();
                    os_ << "Message String ID: " << error_report->message.id.value() << std::endl;
                    indent();
                    os_ << "Arguments (" << error_report->arguments.container.size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& expr_ref : error_report->arguments.container) {
                        indent();
                        os_ << "- Expression Ref ID: " << expr_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            default: {
                indent();
                os_ << "(Unhandled StatementOp)" << std::endl;
                break;
            }
        }
        indent_level_--;
    }

    void DebugPrinter::print_expression(const ebm::Expression& expr) const {
        indent();
        os_ << "Expression ID: " << expr.id.id.value() << std::endl;
        indent_level_++;
        print_expression_body(expr.body);
        indent_level_--;
    }

    void DebugPrinter::print_expression_body(const ebm::ExpressionBody& body) const {
        indent();
        os_ << "Type ID: " << body.type.id.value() << std::endl;
        indent();
        os_ << "Op: " << ebm::to_string(body.op) << std::endl;
        indent_level_++;
        switch (body.op) {
            case ebm::ExpressionOp::LITERAL_INT: {
                if (auto val = body.int_value()) {
                    indent();
                    os_ << "Value: " << *val << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::LITERAL_BOOL: {
                if (auto val = body.bool_value()) {
                    indent();
                    os_ << "Value: " << (*val ? "true" : "false") << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::LITERAL_STRING: {
                if (auto ref = body.string_value()) {
                    indent();
                    os_ << "String Ref ID: " << ref->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::LITERAL_TYPE: {
                if (auto ref = body.type_ref()) {
                    indent();
                    os_ << "Type Ref ID: " << ref->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::IDENTIFIER: {
                if (auto ref = body.id()) {
                    indent();
                    os_ << "Identifier Ref ID: " << ref->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::BINARY_OP: {
                if (auto bop = body.bop()) {
                    indent();
                    os_ << "Binary Op: " << ebm::to_string(*bop) << std::endl;
                }
                if (auto left = body.left()) {
                    indent();
                    os_ << "Left Expr ID: " << left->id.value() << std::endl;
                }
                if (auto right = body.right()) {
                    indent();
                    os_ << "Right Expr ID: " << right->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::UNARY_OP: {
                if (auto uop = body.uop()) {
                    indent();
                    os_ << "Unary Op: " << ebm::to_string(*uop) << std::endl;
                }
                if (auto operand = body.operand()) {
                    indent();
                    os_ << "Operand Expr ID: " << operand->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::CALL: {
                if (auto callee = body.callee()) {
                    indent();
                    os_ << "Callee Expr ID: " << callee->id.value() << std::endl;
                }
                if (auto args = body.arguments()) {
                    indent();
                    os_ << "Arguments (" << args->size() << ") :" << std::endl;
                    indent_level_++;
                    for (const auto& arg_ref : *args) {
                        indent();
                        os_ << "- Argument Expr ID: " << arg_ref.id.value() << std::endl;
                    }
                    indent_level_--;
                }
                break;
            }
            case ebm::ExpressionOp::INDEX_ACCESS: {
                if (auto base = body.base()) {
                    indent();
                    os_ << "Base Expr ID: " << base->id.value() << std::endl;
                }
                if (auto index = body.index()) {
                    indent();
                    os_ << "Index Expr ID: " << index->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::MEMBER_ACCESS: {
                if (auto base = body.base()) {
                    indent();
                    os_ << "Base Expr ID: " << base->id.value() << std::endl;
                }
                if (auto member = body.member()) {
                    indent();
                    os_ << "Member ID: " << member->id.value() << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::TYPE_CAST: {
                if (auto target_type = body.target_type()) {
                    indent();
                    os_ << "Target Type ID: " << target_type->id.value() << std::endl;
                }
                if (auto source_expr = body.source_expr()) {
                    indent();
                    os_ << "Source Expr ID: " << source_expr->id.value() << std::endl;
                }
                if (auto cast_kind = body.cast_kind()) {
                    indent();
                    os_ << "Cast Kind: " << ebm::to_string(*cast_kind) << std::endl;
                }
                break;
            }
            case ebm::ExpressionOp::RANGE: {
                if (auto start = body.start()) {
                    indent();
                    os_ << "Start Expr ID: " << start->id.value() << std::endl;
                }
                if (auto end = body.end()) {
                    indent();
                    os_ << "End Expr ID: " << end->id.value() << std::endl;
                }
                break;
            }
            default: {
                indent();
                os_ << "(Unhandled ExpressionOp)" << std::endl;
                break;
            }
        }
        indent_level_--;
    }

    void DebugPrinter::print_debug_info(const ebm::DebugInfo& debug_info) const {
        indent();
        os_ << "Files (" << debug_info.files.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& file : debug_info.files) {
            indent();
            os_ << "- " << file.data << std::endl;
        }
        indent_level_--;

        indent();
        os_ << "Locations (" << debug_info.locs.size() << ") :" << std::endl;
        indent_level_++;
        for (const auto& loc : debug_info.locs) {
            indent();
            os_ << "- Ident ID: " << loc.ident.id.value() << ", File ID: " << loc.file_id.value()
                << ", Line: " << loc.line.value() << ", Column: " << loc.column.value()
                << ", Start: " << loc.start.value() << ", End: " << loc.end.value() << std::endl;
        }
        indent_level_--;
    }

}  // namespace ebmgen
