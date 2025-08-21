

auto name = module_.get_identifier_or(struct_decl.name, ebm::AnyRef{item_id.id});

root.writeln("struct ", name, " {");
{
    auto scope = root.indent_scope();
    for (auto& field : struct_decl.fields.container) {
        MAYBE(field_ref, module_.get_statement(field));
        MAYBE_VOID(ok, visit_Statement(*this, field_ref));
    }
}
root.writeln("};");
