// This code is included within the visit_Expression_LITERAL_INT64 function.
// We can use variables like `this` (for Visitor) and `int64_value` directly.
CodeWriter w;
w.write(std::to_string(int64_value));
return w.out();