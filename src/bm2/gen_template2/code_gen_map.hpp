/*license*/
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "ast.hpp"
#include "flags.hpp"
#include "../../bm/binary_module.hpp"

namespace rebgn {

    // Function type for generating an AST Expression from an AbstractOp
    using AstGeneratorFunc = std::function<std::unique_ptr<Expression>(const rebgn::Code& code, Flags& flags)>;

    // Map to store AST generator functions
    inline std::map<rebgn::AbstractOp, AstGeneratorFunc> ast_generators;

    // Generator for IMMEDIATE_INT
    inline std::unique_ptr<Expression> generate_immediate_int(const rebgn::Code& code, Flags& flags) {
        return std::make_unique<Literal>(std::to_string(code.int_value()->value()));
    }

    // Generator for IMMEDIATE_STRING
    inline std::unique_ptr<Expression> generate_immediate_string(const rebgn::Code& code, Flags& flags) {
        // In a real scenario, you'd extract the string value from 'code'
        // For now, we'll use a placeholder string based on the ident value
        return std::make_unique<Literal>("\"string_from_ident_" + std::to_string(code.ident()->value()) + "\"");
    }

    // Generator for BINARY operations
    inline std::unique_ptr<Expression> generate_binary_op(const rebgn::Code& code, Flags& flags) {
        // Assuming code.bop returns BinaryOp, and code.left_ref and code.right_ref return Varint
        // For now, we'll use placeholder literals for left and right operands
        std::string op_str = to_string(code.bop().value());
        std::unique_ptr<Expression> left_expr = std::make_unique<Literal>("left_operand");
        std::unique_ptr<Expression> right_expr = std::make_unique<Literal>("right_operand");
        return std::make_unique<BinaryOpExpr>(std::move(left_expr), op_str, std::move(right_expr));
    }

    // Generator for UNARY operations
    inline std::unique_ptr<Expression> generate_unary_op(const rebgn::Code& code, Flags& flags) {
        // Assuming code.uop returns UnaryOp, and code.ref returns Varint
        // For now, we'll use a placeholder literal for the operand
        std::string op_str = to_string(code.uop().value());
        std::unique_ptr<Expression> expr = std::make_unique<Literal>("operand");
        return std::make_unique<UnaryOpExpr>(op_str, std::move(expr));
    }

    // Generator for IF statements
    inline std::unique_ptr<Statement> generate_if_statement(const rebgn::Code& code, Flags& flags) {
        // For now, use placeholder condition and blocks
        std::unique_ptr<Expression> condition = std::make_unique<Literal>("true");
        std::vector<std::unique_ptr<Statement>> then_statements;
        then_statements.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// then block")));
        std::unique_ptr<Statement> then_block = std::make_unique<Block>(std::move(then_statements));

        std::vector<std::unique_ptr<Statement>> else_statements;
        else_statements.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// else block")));
        std::unique_ptr<Statement> else_block = std::make_unique<Block>(std::move(else_statements));

        return std::make_unique<IfStatement>(std::move(condition), std::move(then_block), std::move(else_block));
    }

    // Generator for LOOP statements
    inline std::unique_ptr<Statement> generate_loop_statement(const rebgn::Code& code, Flags& flags) {
        std::unique_ptr<Expression> condition = nullptr;  // Default to infinite loop
        if (code.op == rebgn::AbstractOp::LOOP_CONDITION) {
            // For now, use a placeholder condition
            condition = std::make_unique<Literal>("loop_condition");
        }

        std::vector<std::unique_ptr<Statement>> loop_body_statements;
        loop_body_statements.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// loop body")));
        std::unique_ptr<Block> loop_body = std::make_unique<Block>(std::move(loop_body_statements));

        return std::make_unique<LoopStatement>(std::move(loop_body), std::move(condition));
    }

    // Generator for RETURN_TYPE
    inline std::unique_ptr<Statement> generate_return_statement(const rebgn::Code& code, Flags& flags) {
        // For now, return a simple return statement without an expression
        return std::make_unique<ReturnStatement>();
    }

    // Generator for DEFINE_VARIABLE
    inline std::unique_ptr<Statement> generate_variable_declaration(const rebgn::Code& code, Flags& flags) {
        // For now, declare an int variable with a placeholder name and no initializer
        return std::make_unique<VariableDecl>("int", "my_variable");
    }

    // Function to initialize the map (call once)
    inline void initialize_ast_generators() {
        ast_generators[rebgn::AbstractOp::IMMEDIATE_INT] = generate_immediate_int;
        ast_generators[rebgn::AbstractOp::IMMEDIATE_STRING] = generate_immediate_string;
        ast_generators[rebgn::AbstractOp::BINARY] = generate_binary_op;
        ast_generators[rebgn::AbstractOp::UNARY] = generate_unary_op;
        // Statements are handled separately for now
    }

    // Function to get the AST generator for a given AbstractOp
    inline AstGeneratorFunc get_ast_generator(rebgn::AbstractOp op) {
        auto it = ast_generators.find(op);
        if (it != ast_generators.end()) {
            return it->second;
        }
        return nullptr;  // Or throw an exception for unsupported ops
    }

}  // namespace rebgn