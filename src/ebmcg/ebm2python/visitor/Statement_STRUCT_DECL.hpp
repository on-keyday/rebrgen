// This code is included within the visit_Statement_STRUCT_DECL function.
// We can use variables like `visitor` and `struct_decl` directly.

auto name = module_.get_identifier_or(struct_decl.name, ebm::AnyRef{item_id.id}, "Struct");

root.writeln("class ", name, ":");
auto scope = root.indent_scope();

for(auto& field_ref : struct_decl.fields.container) {
    MAYBE(field, module_.get_statement(field_ref));
    MAYBE_VOID(res, ebm2python::visit_Statement(*this, field));
}

root.writeln();  // Add a blank line for readability.