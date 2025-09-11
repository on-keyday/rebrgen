CodeWriter w;

if (!is_nil(value)) {
    auto add = add_writer();
    MAYBE(ret_val, visit_Expression(*this, value));
    w.writeln("return ", ret_val.value);
}
else {
    w.writeln("return");
}

return w.out();