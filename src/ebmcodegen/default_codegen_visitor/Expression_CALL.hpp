// This code is included within the visit_Expression_CALL function.
// We can use variables like `this` (for Visitor) and `call_desc` directly.
CodeWriter w;

// Get the identifier name from call_desc.callee
MAYBE(callee, visit_Expression(*this, call_desc.callee));
w.write(callee.value, "(");
for (auto& arg : call_desc.arguments.container) {
    MAYBE(arg_str, visit_Expression(*this, arg));
    if (w.out().back() != '(') {
        w.write(",");
    }
    w.write(arg_str.value);
}
w.write(")");
return w.out();