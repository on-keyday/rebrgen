CodeWriter w;

MAYBE(source_expr_str, visit_Expression(*this, source_expr));
MAYBE(target_type_str, visit_Type(*this, type));
w.write(target_type_str, "(", source_expr_str, ")");

return w.out();