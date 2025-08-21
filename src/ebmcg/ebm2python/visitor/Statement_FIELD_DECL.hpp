// This code is included within the visit_Statement_FIELD_DECL function.
// We can use variables like `visitor` and `field_decl` directly.

auto name = module_.get_identifier_or(field_decl.name, ebm::AnyRef{item_id.id}, "field");

// TODO: translate type
root.writeln(name, " = None");
