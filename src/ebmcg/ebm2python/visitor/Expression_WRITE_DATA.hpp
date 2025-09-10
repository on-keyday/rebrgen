// This code is included within the visit_Expression_WRITE_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
// This code is included within the visit_Expression_WRITE_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
MAYBE(statement_text, visit_Statement(*this, io_statement));
MAYBE(w, get_writer());
w.write_unformatted(statement_text.value);
