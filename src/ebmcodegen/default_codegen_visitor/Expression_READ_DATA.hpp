// This code is included within the visit_Expression_READ_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
std::string ident = module_.get_identifier_or(target_stmt);
MAYBE(define_variable, visit_Statement(*this, target_stmt));
MAYBE(io_logic, visit_Statement(*this, io_statement));
MAYBE(w, get_writer());
w.write_unformatted(define_variable.value);
w.write_unformatted(io_logic.value);
return ident;