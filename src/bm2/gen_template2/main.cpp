#include <iostream>
#include "flags.hpp"
#include "ast.hpp"
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>

namespace rebgn {
    bool write_code_template(futils::wrap::UtfOut& w, Flags& flags) {
        if (flags.cmd_options.lang_name.empty()) {
            std::cerr << "error: lang option is required" << std::endl;
            return false;
        }

        AstRenderer renderer;

        // Test 1: Simple Literal
        w << "// Test 1: Simple Literal\n";
        std::unique_ptr<Expression> literal_expr = lit("42");
        w << renderer.render(literal_expr);
        w << "\n\n";

        // Test 2: Binary Operation (10 + 20)
        w << "// Test 2: Binary Operation\n";
        std::unique_ptr<Expression> binary_expr = bin_op(lit("10"), "+", lit("20"));
        w << renderer.render(binary_expr);
        w << "\n\n";

        // Test 3: Variable Declaration (int x = 5;)
        w << "// Test 3: Variable Declaration\n";
        std::unique_ptr<Statement> var_decl_stmt = var_decl("int", "x", lit("5"));
        w << renderer.render(var_decl_stmt);
        w << "\n\n";

        // Test 4: If Statement (if (true) { ... } else { ... })
        w << "// Test 4: If Statement\n";
        std::vector<std::unique_ptr<Statement>> then_stmts;
        then_stmts.push_back(expr_stmt(lit("// then block")));
        std::unique_ptr<Block> then_block = block(std::move(then_stmts));

        std::vector<std::unique_ptr<Statement>> else_stmts;
        else_stmts.push_back(expr_stmt(lit("// else block")));
        std::unique_ptr<Block> else_block = block(std::move(else_stmts));

        std::unique_ptr<Statement> if_stmt_node = if_stmt(
            lit("true"), std::move(then_block), std::move(else_block));
        w << renderer.render(if_stmt_node);
        w << "\n\n";

        // Test 5: Loop Statement (while (true) { ... })
        w << "// Test 5: Loop Statement\n";
        std::vector<std::unique_ptr<Statement>> loop_body_stmts;
        loop_body_stmts.push_back(expr_stmt(lit("// loop body")));
        std::unique_ptr<Block> loop_body = block(std::move(loop_body_stmts));
        std::unique_ptr<Statement> loop_stmt_node = loop_stmt(std::move(loop_body));
        w << renderer.render(loop_stmt_node);
        w << "\n\n";

        // Test 6: Function Call (my_function(arg1, arg2))
        w << "// Test 6: Function Call\n";
        std::vector<std::unique_ptr<Expression>> func_args;
        func_args.push_back(lit("arg1"));
        func_args.push_back(lit("arg2"));
        std::unique_ptr<Expression> func_call_expr = func_call(
            "my_function", std::move(func_args));
        w << renderer.render(func_call_expr);
        w << "\n\n";

        // Test 7: Index Expression (array[index])
        w << "// Test 7: Index Expression\n";
        std::unique_ptr<Expression> index_expr_node = idx_expr(
            lit("my_array"), lit("0"));
        w << renderer.render(index_expr_node);
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
    _set_error_mode(_OUT_TO_STDERR);
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}