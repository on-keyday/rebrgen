auto name = module_.get_identifier_or(item_id);

CodeWriter w;
MAYBE(val, visit_Expression(*this, enum_member_decl.value));

w.writeln(name, " = ", val.value);
return w.out();