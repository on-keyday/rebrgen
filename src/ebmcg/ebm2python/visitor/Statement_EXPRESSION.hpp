// This code is included within the visit_Statement_EXPRESSION function.
// We can use variables like `this` (for Visitor) and function parameters directly.

// Get the Expression object from the ExpressionRef
MAYBE(expr_obj, this->module_.get_expression(expression));

// Visit the expression (call the free function visit_Expression)
MAYBE_VOID(res, visit_Expression(*this, expr_obj)); // Pass 'visitor' directly

return {};