
CodeWriter w;
MAYBE(right_str, visit_Expression(*this, operand));

w.write("(", to_string(uop), " ", right_str.value, ")");

return w.out();
