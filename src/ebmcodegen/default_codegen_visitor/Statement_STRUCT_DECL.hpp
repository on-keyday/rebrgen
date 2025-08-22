

auto name = module_.get_identifier_or(struct_decl.name, ebm::AnyRef{item_id.id});

CodeWriter w;

w.writeln("struct ", name, " {");
{
    auto scope = w.indent_scope();
    for (auto& field : struct_decl.fields.container) {
        MAYBE(field_ref, module_.get_statement(field));
        MAYBE(stmt, visit_Statement(*this, field_ref));
        w.write_unformatted(stmt);
    }
}
w.writeln("};");
