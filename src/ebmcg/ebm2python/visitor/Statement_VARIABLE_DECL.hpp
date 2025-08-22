// This code is included within the visit_Statement_VARIABLE_DECL function.
// We can use variables like `this` (for Visitor) and `var_decl` directly.
CodeWriter w;
auto name = this->module_.get_identifier_or(var_decl.name, ebm::AnyRef{item_id.id}, "var");
MAYBE(type, this->module_.get_type(var_decl.var_type));
MAYBE(type_str_val, visit_Type(*this, type));

w.write(name);

if (var_decl.initial_value.id.value() != 0) {
    MAYBE(initial_value_expr, this->module_.get_expression(var_decl.initial_value));
    MAYBE(initial_value_str, visit_Expression(*this, initial_value_expr));
    w.write(" = ", initial_value_str);
}

w.writeln();

return w.out();