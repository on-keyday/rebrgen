// This code is included within the visit_Statement_FUNCTION_DECL function.
// We can use variables like `visitor` and `func_decl` directly.
CodeWriter w;
auto func_name = this->module_.get_identifier_or(func_decl.name, ebm::AnyRef{item_id.id}, "function");
MAYBE(return_type_str, visit_Type(*this, func_decl.return_type));

std::string params_str = "";
if (func_decl.parent_format.id.value() != 0) {
    params_str += "self";
}
if (func_decl.params.container.empty() == false) {
    for (auto& param_stmt_ref : func_decl.params.container) {
        MAYBE(param_stmt, this->module_.get_statement(param_stmt_ref));
        if (param_stmt.body.kind == ebm::StatementOp::VARIABLE_DECL) {
            auto& var_decl = *param_stmt.body.var_decl();
            auto param_name = this->module_.get_identifier_or(var_decl.name, ebm::AnyRef{param_stmt.id.id}, "param");
            MAYBE(param_type_str, visit_Type(*this, var_decl.var_type));  // Correctly extract the string value
            if (!params_str.empty()) {
                params_str += ", ";
            }
            params_str += param_name + ": " + param_type_str;
        }
        else {
            return unexpect_error("Function parameter is not a VARIABLE_DECL: {}", to_string(param_stmt.body.kind));
        }
    }
}

w.writeln("def ", func_name, "(", params_str, ") -> ", return_type_str, ":");
auto scope = w.indent_scope();
MAYBE(res, visit_Statement(*this, func_decl.body));
if (res.empty()) {
    w.writeln("pass");  // If the body is empty, we just pass
}
else {
    w.write_unformatted(res);
}

return w.out();