// This code is included within the visit_Statement_VARIABLE_DECL function.
// We can use variables like `this` (for Visitor) and `var_decl` directly.
CodeWriter w;
auto name = this->module_.get_identifier_or(item_id);
MAYBE(type_str_val, visit_Type(*this, var_decl.var_type));
std::optional<Result> initial_value;

if (!is_nil(var_decl.initial_value)) {
    auto add = add_writer();
    MAYBE(initial_value_, visit_Expression(*this, var_decl.initial_value));
    MAYBE(got, get_writer());
    w.write_unformatted(got.out());
    initial_value = initial_value_;
}

w.write(name);
if (initial_value) {
    w.write(" = ", initial_value->value);
}

w.writeln();

return w.out();