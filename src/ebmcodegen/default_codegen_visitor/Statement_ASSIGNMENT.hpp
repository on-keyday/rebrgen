/*license*/
CodeWriter w;
MAYBE(expr_target, module_.get_expression(target));
MAYBE(result_target, visit_Expression(*this, expr_target));
MAYBE(expr_value, module_.get_expression(value));
MAYBE(result_value, visit_Expression(*this, expr_value));

w.write(result_target, " = ", result_value);
return w.out();
