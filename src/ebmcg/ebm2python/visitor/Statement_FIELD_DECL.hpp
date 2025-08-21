// This code is included within the visit_Statement_FIELD_DECL function.
// We can use variables like `visitor` and `field_decl` directly.

auto name = module_.get_identifier_or(field_decl.name, ebm::AnyRef{item_id.id}, "field");
MAYBE(type_str_val, type_to_python_str(field_decl.field_type)); // Correctly extract the string value

root.writeln(name, ": ", type_str_val, " = None"); // Use the extracted string value