// This code is included within the visit_Statement_STRUCT_DECL function.
// We can use variables like `visitor` and `struct_decl` directly.
CodeWriter w;
auto name = this->module_.get_identifier_or(struct_decl.name, item_id, "Struct");

w.writeln("class ", name, ":");
auto scope = w.indent_scope();

for (auto& field_ref : struct_decl.fields.container) {
    MAYBE(field, this->module_.get_statement(field_ref));
    MAYBE(res, visit_Statement(*this, field));
    w.write_unformatted(res.value);
}

// Visit encode_fn if it exists
if (struct_decl.encode_fn.id.value() != 0) {  // Corrected: Check value() of Varint id
    MAYBE(encode_fn_stmt, this->module_.get_statement(struct_decl.encode_fn));
    MAYBE(res, visit_Statement(*this, encode_fn_stmt));
    w.write_unformatted(res.value);
}

// Visit decode_fn if it exists
if (struct_decl.decode_fn.id.value() != 0) {  // Corrected: Check value() of Varint id
    MAYBE(decode_fn_stmt, this->module_.get_statement(struct_decl.decode_fn));
    MAYBE(res, visit_Statement(*this, decode_fn_stmt));
    w.write_unformatted(res.value);
}

w.writeln();  // Add a blank line for readability.

return w.out();