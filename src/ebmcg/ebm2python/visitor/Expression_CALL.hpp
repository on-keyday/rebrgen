// This code is included within the visit_Expression_CALL function.
// We can use variables like `this` (for Visitor) and `call_desc` directly.
CodeWriter w;

// Get the identifier name from call_desc.callee
MAYBE(callee_expr_obj, this->module_.get_expression(call_desc.callee));
if (callee_expr_obj.body.kind != ebm::ExpressionOp::IDENTIFIER) {
    return unexpect_error("Expected IDENTIFIER for callee in Expression_CALL");
}
MAYBE(callee_id_stmt, this->module_.get_statement(*callee_expr_obj.body.id()));
if (callee_id_stmt.body.kind != ebm::StatementOp::VARIABLE_DECL && callee_id_stmt.body.kind != ebm::StatementOp::FIELD_DECL) {
    return unexpect_error("Expected VARIABLE_DECL or FIELD_DECL for callee identifier statement");
}

ebm::IdentifierRef callee_identifier_ref;
if (callee_id_stmt.body.kind == ebm::StatementOp::VARIABLE_DECL) {
    callee_identifier_ref = callee_id_stmt.body.var_decl()->name;
}
else {  // FIELD_DECL
    callee_identifier_ref = callee_id_stmt.body.field_decl()->name;
}

std::string callee_name = this->module_.get_identifier_or(callee_identifier_ref, ebm::AnyRef{callee_identifier_ref.id}, "callee");

if (callee_name == "output.put") {
    if (call_desc.arguments.container.empty()) {
        return unexpect_error("output.put expects at least one argument");
    }
    MAYBE(arg_expr, this->module_.get_expression(call_desc.arguments.container[0]));
    MAYBE(arg_str, visit_Expression(*this, arg_expr));
    w.write("stream.read(1)");
    ;
}
else if (callee_name == "input.get") {
    // input.get(u8) should be translated to stream.read(1)
    w.write("stream.read(1)");
}
else {
    // Generic call handling
    MAYBE(callee_expr_obj_for_visit, this->module_.get_expression(call_desc.callee));
    MAYBE(callee_str, visit_Expression(*this, callee_expr_obj_for_visit));
    w.write(callee_str.value, "(");
    for (size_t i = 0; i < call_desc.arguments.container.size(); ++i) {
        MAYBE(arg_expr, this->module_.get_expression(call_desc.arguments.container[i]));
        MAYBE(arg_str, visit_Expression(*this, arg_expr));
        w.write(arg_str.value);
        if (i < call_desc.arguments.container.size() - 1) {
            w.write(", ");
        }
    }
    w.write(")");
}

return w.out();
