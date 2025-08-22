/*license*/
CodeWriter w;
MAYBE(result_target, visit_Expression(*this, target));
MAYBE(result_value, visit_Expression(*this, value));

w.write(result_target, " = ", result_value);
return w.out();
