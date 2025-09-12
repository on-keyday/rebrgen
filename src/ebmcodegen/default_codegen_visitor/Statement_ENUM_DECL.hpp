

auto name = module_.get_identifier_or(item_id);

CodeWriter w;

w.writeln("enum ", name, " ", begin_block);
{
    auto scope = w.indent_scope();
    MAYBE(block, visit_Block(*this, enum_decl.members));
    w.write_unformatted(block.value);
}
w.writeln(end_block);
return w.out();