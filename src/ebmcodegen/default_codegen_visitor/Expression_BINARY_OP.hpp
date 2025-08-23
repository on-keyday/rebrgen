// This code is included within the visit_Expression_BINARY_OP function.
// We can use variables like `this` (for Visitor) and `bop`, `left`, `right` directly.
CodeWriter w;

MAYBE(left_str, visit_Expression(*this, left));
MAYBE(right_str, visit_Expression(*this, right));

w.write("(", left_str, " ", to_string(bop), " ", right_str, ")");

return w.out();
