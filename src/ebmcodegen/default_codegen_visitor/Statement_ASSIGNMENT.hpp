/*license*/
CodeWriter w;
MAYBE(result_target, visit_Expression(*this, target));
MAYBE(result_value, visit_Expression(*this, value));

w.writeln(result_target.value, " = ", result_value.value);
return w.out();
