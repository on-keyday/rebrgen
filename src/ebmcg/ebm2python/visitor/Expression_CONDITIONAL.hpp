MAYBE(cond_str, visit_Expression(*this, condition));
MAYBE(then_str, visit_Expression(*this, then));
MAYBE(else_str, visit_Expression(*this, else_));

return std::format("({} if {} else {})", then_str.value, cond_str.value, else_str.value);
