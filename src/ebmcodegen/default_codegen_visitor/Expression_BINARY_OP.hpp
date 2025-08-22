// This code is included within the visit_Expression_BINARY_OP function.
// We can use variables like `this` (for Visitor) and `bop`, `left`, `right` directly.
CodeWriter w;

MAYBE(left_str, visit_Expression(*this, left));
MAYBE(right_str, visit_Expression(*this, right));

// Debug prints
futils::wrap::cerr_wrap() << "DEBUG: BINARY_OP left_str: " << left_str << "\n";
futils::wrap::cerr_wrap() << "DEBUG: BINARY_OP right_str: " << right_str << "\n";
futils::wrap::cerr_wrap() << "DEBUG: BINARY_OP operator: " << to_string(bop) << "\n";

// New debug print for left operand's ExpressionOp
if (auto op = this->module_.get_expression_op(left)) {
    futils::wrap::cerr_wrap() << "DEBUG: BINARY_OP left_op_kind: " << to_string(*op) << "\n";
}
else {
    futils::wrap::cerr_wrap() << "DEBUG: BINARY_OP left_op_kind: (unknown)\n";
}

w.write("(", left_str, " ", to_string(bop), " ", right_str, ")");

return w.out();
