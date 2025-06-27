#include <iostream>
#include "flags.hpp"
#include "ast.hpp"
#include "code_gen_map.hpp"
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "../../bm/binary_module.hpp"

namespace rebgn {
    bool write_code_template(futils::wrap::UtfOut& w, Flags& flags) {
        if (flags.cmd_options.lang_name.empty()) {
            std::cerr << "error: lang option is required" << std::endl;
            return false;
        }

        initialize_ast_generators();

        AstRenderer renderer;
        std::unique_ptr<Expression> generated_expr;
        std::unique_ptr<Statement> generated_stmt;

        // Test with IMMEDIATE_INT
        rebgn::Code dummy_code_int; 
        dummy_code_int.op = rebgn::AbstractOp::IMMEDIATE_INT; // Set op before setting int_value
        dummy_code_int.int_value(rebgn::Varint(123)); // Assign a dummy Varint value
        generated_expr = get_ast_generator(rebgn::AbstractOp::IMMEDIATE_INT)(dummy_code_int, flags);
        w << "// Immediate Int Test\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with IMMEDIATE_STRING
        rebgn::Code dummy_code_string; 
        dummy_code_string.op = rebgn::AbstractOp::IMMEDIATE_STRING; // Set op before setting ident
        dummy_code_string.ident(rebgn::Varint(456)); // Assign a dummy Varint value for ident
        generated_expr = get_ast_generator(rebgn::AbstractOp::IMMEDIATE_STRING)(dummy_code_string, flags);
        w << "// Immediate String Test\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with BINARY operation (e.g., ADD)
        rebgn::Code dummy_code_binary;
        dummy_code_binary.op = rebgn::AbstractOp::BINARY; // Set op before setting bop
        dummy_code_binary.bop(rebgn::BinaryOp::add); // Assign a dummy BinaryOp
        generated_expr = get_ast_generator(rebgn::AbstractOp::BINARY)(dummy_code_binary, flags);
        w << "// Binary Operation Test (ADD)\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with UNARY operation (e.g., MINUS_SIGN)
        rebgn::Code dummy_code_unary;
        dummy_code_unary.op = rebgn::AbstractOp::UNARY; // Set op before setting uop
        dummy_code_unary.uop(rebgn::UnaryOp::minus_sign); // Assign a dummy UnaryOp
        generated_expr = get_ast_generator(rebgn::AbstractOp::UNARY)(dummy_code_unary, flags);
        w << "// Unary Operation Test (MINUS_SIGN)\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with IF-ELSE statement
        rebgn::Code dummy_code_if;
        dummy_code_if.op = rebgn::AbstractOp::IF; // Set op
        generated_stmt = generate_if_statement(dummy_code_if, flags);
        w << "// IF-ELSE Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with IF-ELIF-ELSE structure
        w << "// IF-ELIF-ELSE Statement Test\n";
        std::unique_ptr<Expression> if_cond = std::make_unique<Literal>("condition1");
        std::vector<std::unique_ptr<Statement>> if_then_stmts;
        if_then_stmts.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// if block")));
        std::unique_ptr<Statement> if_then_block = std::make_unique<Block>(std::move(if_then_stmts));

        std::unique_ptr<Expression> elif_cond = std::make_unique<Literal>("condition2");
        std::vector<std::unique_ptr<Statement>> elif_then_stmts;
        elif_then_stmts.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// elif block")));
        std::unique_ptr<Statement> elif_then_block = std::make_unique<Block>(std::move(elif_then_stmts));

        std::vector<std::unique_ptr<Statement>> else_stmts;
        else_stmts.push_back(std::make_unique<ExpressionStatement>(std::make_unique<Literal>("// else block")));
        std::unique_ptr<Statement> else_block_final = std::make_unique<Block>(std::move(else_stmts));

        // Constructing the nested IfStatement for ELIF
        std::unique_ptr<Statement> elif_stmt = std::make_unique<IfStatement>(
            std::move(elif_cond),
            std::move(elif_then_block),
            std::move(else_block_final) // The final else block for the chain
        );

        // The main IfStatement
        std::unique_ptr<Statement> if_elif_else_stmt = std::make_unique<IfStatement>(
            std::move(if_cond),
            std::move(if_then_block),
            std::move(elif_stmt) // The else block of the first if is the elif statement
        );

        w << renderer.render(if_elif_else_stmt);
        w << "\n\n";

        // Test with LOOP_INFINITE
        rebgn::Code dummy_code_loop_infinite;
        dummy_code_loop_infinite.op = rebgn::AbstractOp::LOOP_INFINITE;
        generated_stmt = generate_loop_statement(dummy_code_loop_infinite, flags);
        w << "// Infinite Loop Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with LOOP_CONDITION
        rebgn::Code dummy_code_loop_condition;
        dummy_code_loop_condition.op = rebgn::AbstractOp::LOOP_CONDITION;
        generated_stmt = generate_loop_statement(dummy_code_loop_condition, flags);
        w << "// Conditional Loop Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with RETURN_TYPE
        rebgn::Code dummy_code_return;
        dummy_code_return.op = rebgn::AbstractOp::RETURN_TYPE;
        generated_stmt = generate_return_statement(dummy_code_return, flags);
        w << "// Return Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with DEFINE_VARIABLE
        rebgn::Code dummy_code_define_variable;
        dummy_code_define_variable.op = rebgn::AbstractOp::DEFINE_VARIABLE;
        generated_stmt = generate_variable_declaration(dummy_code_define_variable, flags);
        w << "// Variable Declaration Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        return true;
    }
}  // namespace rebgn

auto& cout = futils::wrap::cout_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (!rebgn::write_code_template(cout, flags)) {
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}