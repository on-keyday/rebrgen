/*license*/
#include <wrap/cin.h>
#include <wrap/cout.h>
#include <cstddef>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include "comb2/composite/number.h"
#include "comb2/composite/range.h"
#include "comb2/composite/string.h"
#include "comb2/internal/test.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/debug_printer.hpp"
#include "ebmgen/mapping.hpp"
#include "error/error.h"
#include "escape/escape.h"
#include "number/parse.h"
#include "number/prefix.h"
#include <strutil/splits.h>
#include <comb2/composite/cmdline.h>
#include <console/ansiesc.h>
#include <comb2/tree/simple_node.h>

namespace ebmgen {
    namespace query {
        enum class NodeType {
            Root,
            Operator,
            Ident,
            Number,
            StringLit,
            BinaryOp,
            UnaryOp,
            ObjectType,
            Object,
        };
        using namespace futils::comb2::ops;
        constexpr auto space = futils::comb2::composite::space | futils::comb2::composite::tab | futils::comb2::composite::eol;
        constexpr auto spaces = *(space);
        constexpr auto compare_op = str(NodeType::Operator, lit(">=") | lit("<=") | lit("==") | lit("!=") | lit(">") | lit("<"));
        constexpr auto and_op = str(NodeType::Operator, lit("and") | lit("&&"));
        constexpr auto or_op = str(NodeType::Operator, lit("or") | lit("||"));
        constexpr auto not_op = str(NodeType::Operator, lit("not") | lit("!"));
        constexpr auto number = str(NodeType::Number, futils::comb2::composite::hex_integer | futils::comb2::composite::dec_integer);
        constexpr auto ident = str(NodeType::Ident, futils::comb2::composite::c_ident&*(lit(".") & +futils::comb2::composite::c_ident));
        constexpr auto string_lit = str(NodeType::StringLit, futils::comb2::composite::c_str);
        constexpr auto expr_recurse = method_proxy(expr);
        constexpr auto compare_recurse = method_proxy(compare);
        constexpr auto and_recurse = method_proxy(and_expr);
        constexpr auto or_recurse = method_proxy(or_expr);
        constexpr auto primary = spaces & +(string_lit | number | ident | ('('_l & expr_recurse & +')'_l)) & spaces;
        constexpr auto unary_op = group(NodeType::UnaryOp, (not_op & +primary) | +primary);
        constexpr auto compare = group(NodeType::BinaryOp, unary_op & -(compare_op & +compare_recurse));
        constexpr auto and_expr = group(NodeType::BinaryOp, compare & -(and_op & +and_recurse));
        constexpr auto or_expr = group(NodeType::BinaryOp, and_expr & -(or_op & +or_recurse));
        constexpr auto expr = or_expr;
        constexpr auto object_type = str(NodeType::ObjectType, lit("Identifier") | lit("String") | lit("Type") | lit("Statement") | lit("Expression"));
        constexpr auto object = group(NodeType::Object, object_type& spaces & +lit("{") & spaces & +expr_recurse & spaces & +lit("}"));
        constexpr auto full_expr = spaces & ~(object & spaces) & spaces & eos;
        struct Recurse {
            decltype(expr) expr;
            decltype(compare) compare;
            decltype(and_expr) and_expr;
            decltype(or_expr) or_expr;
        };
        constexpr Recurse recurse{expr, compare, and_expr, or_expr};
        constexpr auto check = []() {
            auto seq = futils::make_ref_seq("Identifier { id == 123 } Expression { id >= 0 and id < 100 } Statement { bop == \"add\"}");
            return full_expr(seq, futils::comb2::test::TestContext<>{}, recurse) == futils::comb2::Status::match;
        };

        static_assert(check(), "Expression parser static assert");
    }  // namespace query
    struct Debugger {
        futils::wrap::UtfOut& cout;
        futils::wrap::UtfIn& cin;
        MappingTable& table;

        std::string line;
        std::vector<std::string_view> args;

        void print() {
            std::stringstream print_buf;
            DebugPrinter printer{table, print_buf};
            if (args.size() < 2) {
                cout << "Usage: print <identifier>\n";
                return;
            }
            auto ident = args[1];
            std::uint64_t id;
            if (!futils::number::parse_integer(ident, id)) {
                cout << "Invalid identifier: " << ident << "\n";
                return;
            }
            auto obj = table.get_object(ebm::AnyRef{id});
            std::visit(
                [&](auto&& obj) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(obj)>, std::monostate>) {
                        cout << "Identifier not found: " << ident << "\n";
                    }
                    else {
                        printer.print_object(*obj);
                    }
                },
                obj);
            cout << print_buf.str();
        }

        void header() {
            auto& raw_module = table.module();
            cout << "Header Information:\n";
            cout << " Version: " << raw_module.version << "\n";
            cout << " Max ID: " << get_id(raw_module.max_id) << "\n";
            cout << " Identifiers: " << raw_module.identifiers.size() << "\n";
            cout << " String Literals: " << raw_module.strings.size() << "\n";
            cout << " Types: " << raw_module.types.size() << "\n";
            cout << " Statements: " << raw_module.statements.size() << "\n";
            cout << " Expressions: " << raw_module.expressions.size() << "\n";
            cout << " Aliases: " << raw_module.aliases.size() << "\n";
            cout << " Total Objects: " << (raw_module.identifiers.size() + raw_module.strings.size() + raw_module.types.size() + raw_module.statements.size() + raw_module.expressions.size() + raw_module.aliases.size()) << "\n";
            cout << " Source File:\n";
            for (auto& s : raw_module.debug_info.files) {
                cout << "  " << s.data << "\n";
            }
            cout << " Debug Locations: " << raw_module.debug_info.locs.size() << "\n";
            cout << "----\n";
        }

        using EvalValue = std::variant<bool, std::uint64_t, std::string>;

        struct EvalContext {
            std::vector<EvalValue> stack;
            std::map<std::string, EvalValue> variables;
        };

        using EvalFunc = std::function<bool(EvalContext&)>;

        struct Object {
            std::string type;
            std::unordered_set<std::string> related_identifiers;
            EvalFunc evaluator;
        };
#define CHECK_AND_TAKE(var)                 \
    if (ctx.stack.empty()) {                \
        return false;                       \
    }                                       \
    auto var = std::move(ctx.stack.back()); \
    ctx.stack.pop_back()

#define CHECK_AND_TAKE_AS(var, T)              \
    CHECK_AND_TAKE(var##__);                   \
    if (!std::holds_alternative<T>(var##__)) { \
        return false;                          \
    }                                          \
    auto var = std::get<T>(var##__)

        struct Evaluator {
            std::vector<Object> objects;
        };
        using NodeType = query::NodeType;
        using Node = futils::comb2::tree::node::GenericNode<NodeType>;

        expected<EvalFunc> eval_expr(Object& object, const std::shared_ptr<Node>& node) {
            using futils::comb2::tree::node::as_group, futils::comb2::tree::node::as_tok;
            if (auto g = as_group<NodeType>(node)) {
                if (g->tag == NodeType::BinaryOp) {
                    if (g->children.size() == 1) {
                        return eval_expr(object, g->children[0]);
                    }
                    else if (g->children.size() == 3) {
                        MAYBE(left, eval_expr(object, g->children[0]));
                        auto op = as_tok<NodeType>(g->children[1]);
                        MAYBE(right, eval_expr(object, g->children[2]));
                        if (op && op->tag == NodeType::Operator) {
                            if (op->token == "and" || op->token == "&&") {
                                return [left = std::move(left), right = std::move(right)](EvalContext& ctx) {
                                    if (!left(ctx)) {
                                        return false;
                                    }
                                    CHECK_AND_TAKE_AS(left_value, bool);
                                    if (!left_value) {
                                        ctx.stack.push_back(EvalValue{false});
                                        return true;
                                    }
                                    return right(ctx);
                                };
                            }
                            else if (op->token == "or" || op->token == "||") {
                                return [left = std::move(left), right = std::move(right)](EvalContext& ctx) {
                                    if (!left(ctx)) {
                                        return false;
                                    }
                                    CHECK_AND_TAKE_AS(left_value, bool);
                                    if (left_value) {
                                        ctx.stack.push_back(EvalValue{true});
                                        return true;
                                    }
                                    return right(ctx);
                                };
                            }
                            else if (op->token == "==" || op->token == "!=" || op->token == ">" || op->token == ">=" || op->token == "<" || op->token == "<=") {
                                return [left = std::move(left), right = std::move(right), op = op->token](EvalContext& ctx) {
                                    if (!left(ctx)) {
                                        return false;
                                    }
                                    CHECK_AND_TAKE(left_value);
                                    if (!right(ctx)) {
                                        return false;
                                    }
                                    CHECK_AND_TAKE(right_value);
                                    bool result = false;
                                    if (std::holds_alternative<bool>(left_value) && std::holds_alternative<bool>(right_value)) {
                                        auto l = std::get<bool>(left_value);
                                        auto r = std::get<bool>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else {
                                            return false;
                                        }
                                    }
                                    else if (std::holds_alternative<std::string>(left_value) && std::holds_alternative<std::string>(right_value)) {
                                        auto l = std::get<std::string>(left_value);
                                        auto r = std::get<std::string>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else {
                                            return false;
                                        }
                                    }
                                    else if (std::holds_alternative<std::uint64_t>(left_value) && std::holds_alternative<std::uint64_t>(right_value)) {
                                        auto l = std::get<std::uint64_t>(left_value);
                                        auto r = std::get<std::uint64_t>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else if (op == ">") {
                                            result = (l > r);
                                        }
                                        else if (op == ">=") {
                                            result = (l >= r);
                                        }
                                        else if (op == "<") {
                                            result = (l < r);
                                        }
                                        else if (op == "<=") {
                                            result = (l <= r);
                                        }
                                    }
                                    else {
                                        return false;
                                    }
                                    ctx.stack.push_back(EvalValue{result});
                                    return true;
                                };
                            }
                            else {
                                return unexpect_error("Unsupported binary operator: {}", op->token);
                            }
                        }
                        return unexpect_error("Unsupported binary operation: {}", op ? op->token : "(null)");
                    }
                    else {
                        return unexpect_error("Invalid binary operation node with {} children", g->children.size());
                    }
                }
                else if (g->tag == NodeType::UnaryOp) {
                    if (g->children.size() == 1) {
                        return eval_expr(object, g->children[0]);
                    }
                    else if (g->children.size() == 2) {
                        auto op = as_tok<NodeType>(g->children[0]);
                        MAYBE(operand, eval_expr(object, g->children[1]));
                        if (op && op->tag == NodeType::Operator) {
                            if (op->token == "not" || op->token == "!") {
                                return [operand = std::move(operand)](EvalContext& ctx) {
                                    if (!operand(ctx)) {
                                        return false;
                                    }
                                    CHECK_AND_TAKE_AS(value, bool);
                                    ctx.stack.push_back(EvalValue{!value});
                                    return true;
                                };
                            }
                            else {
                                return unexpect_error("Unsupported unary operator: {}", op->token);
                            }
                        }
                        return unexpect_error("Unsupported unary operation: {}", op ? op->token : "(null)");
                    }
                    else {
                        return unexpect_error("Invalid unary operation node with {} children", g->children.size());
                    }
                }
                else {
                    return unexpect_error("Unsupported expression group node");
                }
            }
            else if (auto t = as_tok<NodeType>(node)) {
                if (t->tag == NodeType::Number) {
                    std::uint64_t value;
                    if (!futils::number::prefix_integer(t->token, value)) {
                        return unexpect_error("Invalid number: {}", t->token);
                    }
                    return [value](EvalContext& ctx) {
                        ctx.stack.push_back(EvalValue{value});
                        return true;
                    };
                }
                else if (t->tag == NodeType::Ident) {
                    object.related_identifiers.insert(t->token);
                    return [token = t->token](EvalContext& ctx) {
                        if (auto found = ctx.variables.find(token); found != ctx.variables.end()) {
                            ctx.stack.push_back(found->second);
                            return true;
                        }
                        return false;
                    };
                }
                else if (t->tag == NodeType::StringLit) {
                    std::string value;
                    auto removed_quotes = t->token;
                    removed_quotes.erase(0, 1);
                    removed_quotes.pop_back();
                    if (!futils::escape::unescape_str(removed_quotes, value)) {
                        return unexpect_error("Invalid string literal: {}", t->token);
                    }
                    return [value = std::move(value)](EvalContext& ctx) {
                        ctx.stack.push_back(EvalValue{value});
                        return true;
                    };
                }
            }
            return unexpect_error("Unsupported expression node");
        }

        expected<void> eval_object(Evaluator& eval, const std::shared_ptr<Node>& node) {
            using futils::comb2::tree::node::as_group, futils::comb2::tree::node::as_tok;
            if (auto g = as_group<NodeType>(node)) {
                if (g->tag == NodeType::Object) {
                    if (g->children.size() == 2) {
                        auto type_tok = as_tok<NodeType>(g->children[0]);
                        if (!type_tok || type_tok->tag != NodeType::ObjectType) {
                            return unexpect_error("Invalid object type token");
                        }
                        auto expr_node = g->children[1];
                        Object obj;
                        MAYBE(fn, eval_expr(obj, expr_node));
                        obj.evaluator = std::move(fn);
                        obj.type = type_tok->token;
                        eval.objects.push_back(std::move(obj));
                        return {};
                    }
                    else {
                        return unexpect_error("Invalid object node with {} children", g->children.size());
                    }
                }
                if (g->tag == NodeType::Root) {
                    for (auto& child : g->children) {
                        MAYBE_VOID(v, eval_object(eval, child));
                    }
                    return {};
                }
            }
            return unexpect_error("Unsupported object node");
        }

        bool setup_evaluator(Evaluator& eval) {
            auto input = futils::make_ref_seq(std::string_view(line));
            input.rptr = args[0].size();
            futils::comb2::tree::BranchTable table;
            auto res = query::full_expr(input, table, query::recurse);
            if (res != futils::comb2::Status::match || !input.eos()) {
                cout << "Failed to parse query expression at position " << input.rptr << ": " << line.substr(input.rptr) << "\n";
                return false;
            }
            auto collected = futils::comb2::tree::node::collect<NodeType>(table.root_branch);
            auto result = eval_object(eval, collected);
            if (!result) {
                cout << "Error in query expression: " << result.error().error() << "\n";
                return false;
            }
            return true;
        }

        void query() {
            if (args.size() < 2) {
                cout << "Usage: query <conditions...>\n";
                return;
            }
            Evaluator eval;
            if (!setup_evaluator(eval)) {
                return;
            }
            auto for_each = [&](Object& obj, auto& objects) {
                std::vector<std::string> idents;
                auto qualified_name = [&](const char* name) -> std::string {
                    if (idents.size()) {
                        return idents.back() + "." + name;
                    }
                    return name;
                };
                for (auto& t : objects) {
                    EvalContext ctx;
                    t.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, ebm::Varint>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = value.value();
                            }
                        }
                        else if constexpr (AnyRef<T>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = get_id(value);
                            }
                        }
                        else if constexpr (std::is_same_v<T, bool>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = value;
                            }
                        }
                        else if constexpr (std::is_integral_v<T>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = std::uint64_t(value);
                            }
                        }
                        else if constexpr (std::is_same_v<T, ebm::String>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = value.data;
                            }
                        }
                        else if constexpr (is_container<decltype(value)>) {
                            for (size_t i = 0; i < value.container.size(); i++) {
                                std::string elem_name = std::format("{}[{}]", name, i);
                                visitor(visitor, elem_name.c_str(), value.container[i]);
                            }
                        }
                        else if constexpr (ebmgen::has_visit<decltype(value), decltype(visitor)>) {
                            idents.push_back(qualified_name(name));
                            value.visit(visitor);
                            idents.pop_back();
                        }
                        else if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>>) {
                            if (value) {
                                visitor(visitor, name, *value);
                            }
                        }
                        else if constexpr (std::is_enum_v<T>) {
                            if (obj.related_identifiers.contains(qualified_name(name))) {
                                ctx.variables[qualified_name(name)] = to_string(value, true);
                            }
                        }
                        else {
                            static_assert(std::is_same_v<T, void>, "unsupported type in query");
                        }
                    });
                    if (obj.evaluator(ctx)) {
                        if (ctx.stack.size() == 1 && std::holds_alternative<bool>(ctx.stack.back()) && std::get<bool>(ctx.stack.back())) {
                            std::stringstream print_buf;
                            DebugPrinter printer{table, print_buf};
                            printer.print_object(t);
                            cout << print_buf.str();
                            cout << "----\n";
                        }
                    }
                }
            };
            for (auto& obj : eval.objects) {
                if (obj.type == "Identifier") {
                    for_each(obj, table.module().identifiers);
                }
                else if (obj.type == "String") {
                    for_each(obj, table.module().strings);
                }
                else if (obj.type == "Type") {
                    for_each(obj, table.module().types);
                }
                else if (obj.type == "Statement") {
                    for_each(obj, table.module().statements);
                }
                else if (obj.type == "Expression") {
                    for_each(obj, table.module().expressions);
                }
            }
        }

        void print_help() {
            cout << "Commands:\n";
            cout << "  help               Show this help message\n";
            cout << "  exit, quit        Exit the debugger\n";
            cout << "  print <id>       Print the object with the given identifier\n";
            cout << "  p <id>           Alias for print\n";
            cout << "  pr <id>          Alias for print\n";
            cout << "  header           Print module header information\n";
            cout << "  query <expr>     Query objects matching the expression\n";
            cout << "  clear            Clear the console\n";
        }

        void start() {
            cout << "Interactive Debugger\n";
            cout << "Type 'exit' to quit\n";
            cout << "Type 'help' for commands\n";
            while (true) {
                cout << "ebmgen> ";
                line.clear();
                cin >> line;
                std::set<std::string> temporary_buffer;
                args = futils::comb2::cmdline::command_line<std::vector<std::string_view>>(std::string_view(line), [&](auto& buf) {
                    buf = buf.substr(1, buf.size() - 2);
                    if (buf.contains("\\")) {
                        auto tmp_buf = futils::escape::unescape_str<std::string>(buf);
                        buf = *temporary_buffer.insert(std::move(tmp_buf)).first;
                    }
                });
                if (args.empty()) {
                    continue;
                }
                auto command = args[0];
                if (command == "exit" || command == "quit") {
                    break;
                }
                else if (command == "help") {
                    print_help();
                }
                else if (command == "print" || command == "p" || command == "pr") {
                    print();
                }
                else if (command == "header") {
                    header();
                }
                else if (command == "clear") {
                    cout << futils::console::escape::cursor(futils::console::escape::CursorMove::clear, 2).c_str();
                    cout << futils::console::escape::cursor(futils::console::escape::CursorMove::home, 1).c_str();
                }
                else if (command == "query") {
                    query();
                }
                else {
                    cout << "Unknown command: " << command << ", type 'help' for commands\n";
                }
            }
        }
    };

    void interactive_debugger(MappingTable& table) {
        Debugger debugger{futils::wrap::cout_wrap(), futils::wrap::cin_wrap(), table};
        debugger.start();
    }
}  // namespace ebmgen
