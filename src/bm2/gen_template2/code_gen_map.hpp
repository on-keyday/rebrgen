/*license*/
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "ast.hpp"
#include "flags.hpp"
#include "../../bm/binary_module.hpp"
#include "../../bm2/context.hpp"

namespace rebgn {

// Function type for generating an AST Expression from an AbstractOp
using AstGeneratorFunc = std::function<std::unique_ptr<Expression>(const bm2::Context& ctx, size_t code_idx, Flags& flags)>;

// Map to store AST generator functions
inline std::map<rebgn::AbstractOp, AstGeneratorFunc> ast_generators;

// Forward declarations for recursive calls
std::unique_ptr<Expression> generate_expression_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<Statement> generate_statement_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags);

// Helper to find the end of an IF block (and its ELIF/ELSE branches)
// Returns the index of the END_IF, or the next statement after the ELSE block
inline size_t find_end_of_if_block(const bm2::Context& ctx, size_t start_idx) {
    size_t nested_if_count = 0;
    for (size_t i = start_idx + 1; i < ctx.bm.code.size(); ++i) {
        const auto& current_code = ctx.bm.code[i];
        if (current_code.op == rebgn::AbstractOp::IF) {
            nested_if_count++;
        } else if (current_code.op == rebgn::AbstractOp::END_IF) {
            if (nested_if_count == 0) {
                return i; // Found the matching END_IF
            } else {
                nested_if_count--;
            }
        }
    }
    return ctx.bm.code.size(); // Should not happen in a well-formed AST
}

// Generator for IMMEDIATE_INT
inline std::unique_ptr<rebgn::Expression> generate_immediate_int(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit(std::to_string(code.int_value()->value()));
}

// Generator for IMMEDIATE_STRING
inline std::unique_ptr<rebgn::Expression> generate_immediate_string(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit("\"" + ctx.ident_table.at(code.ident()->value()) + "\"");
}

// Generator for IMMEDIATE_TRUE
inline std::unique_ptr<rebgn::Expression> generate_immediate_true(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("true");
}

// Generator for IMMEDIATE_FALSE
inline std::unique_ptr<rebgn::Expression> generate_immediate_false(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("false");
}

// Generator for IMMEDIATE_INT64
inline std::unique_ptr<rebgn::Expression> generate_immediate_int64(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit(std::to_string(code.int_value64().value()) + "LL");
}

// Generator for IMMEDIATE_CHAR
inline std::unique_ptr<rebgn::Expression> generate_immediate_char(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit("'" + std::string(1, static_cast<char>(code.int_value()->value())) + "'");
}

// Generator for IMMEDIATE_TYPE
inline std::unique_ptr<rebgn::Expression> generate_immediate_type(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    // Assuming StorageRef points to an identifier that represents the type name
    return lit(ctx.ident_table.at(code.type().value().ref.value()));
}

// Generator for EMPTY_PTR
inline std::unique_ptr<rebgn::Expression> generate_empty_ptr(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("nullptr");
}

// Generator for EMPTY_OPTIONAL
inline std::unique_ptr<rebgn::Expression> generate_empty_optional(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("std::nullopt");
}

// Generator for PHI, DECLARE_VARIABLE, DEFINE_VARIABLE_REF, BEGIN_COND_BLOCK
inline std::unique_ptr<rebgn::Expression> generate_ref_evaluation(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
}

// Generator for ASSIGN
inline std::unique_ptr<rebgn::Expression> generate_assign_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return bin_op(std::move(left_expr), "=", std::move(right_expr));
}

// Generator for ACCESS
inline std::unique_ptr<rebgn::Expression> generate_access_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::string right_ident = ctx.ident_table.at(code.right_ref()->value());

    // Check if the left_expr is an IMMEDIATE_TYPE (representing an enum)
    // This is a simplification; a more robust solution might involve type checking the AST node
    bool is_enum_member = (ctx.bm.code[ctx.ident_index_table.at(code.left_ref()->value())].op == rebgn::AbstractOp::IMMEDIATE_TYPE);

    if (is_enum_member) {
        // Enum member access (e.g., EnumType::EnumValue)
        return bin_op(std::move(left_expr), "::", lit(right_ident));
    } else {
        // Normal member access (e.g., object.member)
        return bin_op(std::move(left_expr), ".", lit(right_ident));
    }
}

// Generator for INDEX
inline std::unique_ptr<rebgn::Expression> generate_index_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return idx_expr(std::move(left_expr), std::move(right_expr));
}

// Generator for ARRAY_SIZE
inline std::unique_ptr<rebgn::Expression> generate_array_size_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> target_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    return member_func_call(std::move(target_expr), "size");
}

// Generator for BINARY operations
inline std::unique_ptr<rebgn::Expression> generate_binary_op(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string op_str = to_string(code.bop().value());
    std::cerr << "DEBUG: BINARY op_str: " << op_str << std::endl;
    std::cerr << "DEBUG: BINARY left_ref: " << code.left_ref()->value() << std::endl;
    std::cerr << "DEBUG: BINARY right_ref: " << code.right_ref()->value() << std::endl;
    size_t left_idx = ctx.ident_index_table.at(code.left_ref()->value());
    size_t right_idx = ctx.ident_index_table.at(code.right_ref()->value());
    std::cerr << "DEBUG: BINARY resolved left_idx: " << left_idx << std::endl;
    std::cerr << "DEBUG: BINARY resolved right_idx: " << right_idx << std::endl;
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, left_idx, flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, right_idx, flags);
    return bin_op(std::move(left_expr), op_str, std::move(right_expr));
}

// Generator for UNARY operations
inline std::unique_ptr<rebgn::Expression> generate_unary_op(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string op_str = to_string(code.uop().value());
    std::cerr << "DEBUG: UNARY op_str: " << op_str << std::endl;
    std::cerr << "DEBUG: UNARY ref: " << code.ref()->value() << std::endl;
    size_t ref_idx = ctx.ident_index_table.at(code.ref()->value());
    std::cerr << "DEBUG: UNARY resolved ref_idx: " << ref_idx << std::endl;
    std::unique_ptr<rebgn::Expression> expr = generate_expression_from_code(ctx, ref_idx, flags);
    return un_op(op_str, std::move(expr));
}

// Generator for IF statements
inline std::unique_ptr<rebgn::Statement> generate_if_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> condition = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);

    std::vector<std::unique_ptr<rebgn::Statement>> then_statements;
    size_t current_idx = code_idx + 1;
    size_t end_of_if = find_end_of_if_block(ctx, code_idx);

    // Parse then block
    while (current_idx < end_of_if && ctx.bm.code[current_idx].op != rebgn::AbstractOp::ELIF && ctx.bm.code[current_idx].op != rebgn::AbstractOp::ELSE) {
        then_statements.push_back(generate_statement_from_code(ctx, current_idx, flags));
        current_idx++;
    }
    std::unique_ptr<rebgn::Statement> then_block = block(std::move(then_statements));

    std::unique_ptr<rebgn::Statement> else_block = nullptr;

    // Handle ELIF and ELSE
    if (current_idx < end_of_if) {
        if (ctx.bm.code[current_idx].op == rebgn::AbstractOp::ELIF) {
            // ELIF is essentially an IF statement in the else branch
            else_block = generate_if_statement(ctx, current_idx, flags); // Recursively call for ELIF
        } else if (ctx.bm.code[current_idx].op == rebgn::AbstractOp::ELSE) {
            std::vector<std::unique_ptr<rebgn::Statement>> else_statements;
            current_idx++; // Move past ELSE op
            while (current_idx < end_of_if) {
                else_statements.push_back(generate_statement_from_code(ctx, current_idx, flags));
                current_idx++;
            }
            else_block = block(std::move(else_statements));
        }
    }

    return if_stmt(std::move(condition), std::move(then_block), std::move(else_block));
}

// Generator for LOOP statements
inline std::unique_ptr<rebgn::Statement> generate_loop_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> condition = nullptr; // Default to infinite loop
    if (code.op == rebgn::AbstractOp::LOOP_CONDITION) {
        condition = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }

    // For now, loop body is a placeholder
    std::vector<std::unique_ptr<rebgn::Statement>> loop_body_statements;
    loop_body_statements.push_back(expr_stmt(lit("// loop body")));
    std::unique_ptr<rebgn::Block> loop_body = block(std::move(loop_body_statements));

    return loop_stmt(std::move(loop_body), std::move(condition));
}

// Generator for RETURN_TYPE
inline std::unique_ptr<rebgn::Statement> generate_return_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> expr = nullptr;
    // Assuming RETURN_TYPE might have an associated expression via 'ref'
    if (code.ref()) { // Check if ref exists
        expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }
    return ret_stmt(std::move(expr));
}

// Generator for DEFINE_VARIABLE
inline std::unique_ptr<rebgn::Statement> generate_variable_declaration(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    // For now, declare an int variable with a placeholder name and no initializer
    // In a real scenario, you'd extract type and name from 'code'
    std::string var_name = "my_variable";
    if (code.ident()) {
        var_name = ctx.ident_table.at(code.ident()->value());
    }
    std::unique_ptr<rebgn::Expression> initializer = nullptr;
    if (code.ref()) {
        initializer = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }
    return var_decl("int", var_name, std::move(initializer));
}

// Central dispatch for expressions
std::unique_ptr<rebgn::Expression> generate_expression_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::cerr << "DEBUG: generate_expression_from_code processing op: " << to_string(code.op) << " at index: " << code_idx << std::endl;
    switch (code.op) {
        case rebgn::AbstractOp::IMMEDIATE_INT:
            return generate_immediate_int(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_STRING:
            return generate_immediate_string(ctx, code_idx, flags);
        case rebgn::AbstractOp::BINARY:
            return generate_binary_op(ctx, code_idx, flags);
        case rebgn::AbstractOp::UNARY:
            return generate_unary_op(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_TRUE:
            return generate_immediate_true(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_FALSE:
            return generate_immediate_false(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_INT64:
            return generate_immediate_int64(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_CHAR:
            return generate_immediate_char(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_TYPE:
            return generate_immediate_type(ctx, code_idx, flags);
        case rebgn::AbstractOp::EMPTY_PTR:
            return generate_empty_ptr(ctx, code_idx, flags);
        case rebgn::AbstractOp::EMPTY_OPTIONAL:
            return generate_empty_optional(ctx, code_idx, flags);
        case rebgn::AbstractOp::PHI:
        case rebgn::AbstractOp::DECLARE_VARIABLE:
        case rebgn::AbstractOp::DEFINE_VARIABLE_REF:
        case rebgn::AbstractOp::BEGIN_COND_BLOCK:
            return generate_ref_evaluation(ctx, code_idx, flags);
        case rebgn::AbstractOp::ASSIGN:
            return generate_assign_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::ACCESS:
            return generate_access_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::INDEX:
            return generate_index_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::ARRAY_SIZE:
            return generate_array_size_expression(ctx, code_idx, flags);
        // Add more expression types here
        default:
            return expr_stmt(lit("// Unimplemented Expression: " + std::string(to_string(code.op))));
    }
}

// Central dispatch for statements
std::unique_ptr<rebgn::Statement> generate_statement_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    switch (code.op) {
        case rebgn::AbstractOp::IF:
        case rebgn::AbstractOp::ELIF:
        case rebgn::AbstractOp::ELSE:
            return generate_if_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::LOOP_INFINITE:
        case rebgn::AbstractOp::LOOP_CONDITION:
            return generate_loop_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::RETURN_TYPE:
            return generate_return_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_VARIABLE:
            return generate_variable_declaration(ctx, code_idx, flags);
        // Add more statement types here
        default:
            return expr_stmt(lit("// Unimplemented Statement: " + std::string(to_string(code.op))));
    }
}

// Function to initialize the map (call once)
inline void initialize_ast_generators() {
    // Expression generators are handled by generate_expression_from_code
    // Statement generators are handled by generate_statement_from_code
}

// Function to get the AST generator for a given AbstractOp
// This function is no longer directly used for dispatching, but kept for compatibility if needed
inline AstGeneratorFunc get_ast_generator(rebgn::AbstractOp op) {
    // This function's role has been largely replaced by generate_expression_from_code
    // and generate_statement_from_code
    return nullptr; 
}

} // namespace rebgn