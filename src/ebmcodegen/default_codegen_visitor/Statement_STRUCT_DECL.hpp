

auto name = module_.get_identifier_or(struct_decl.name, item_id);

CodeWriter w;

w.writeln("struct ", name, " {");
{
    auto scope = w.indent_scope();
    MAYBE(block, visit_Block(*this, struct_decl.fields));
    w.write_unformatted(block);
}
w.writeln("};");
return w.out();