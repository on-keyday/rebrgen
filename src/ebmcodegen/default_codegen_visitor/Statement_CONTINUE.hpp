MAYBE(loop_stmt, module_.get_statement(continue_.related_statement));
MAYBE(loop, loop_stmt.body.loop());
CodeWriter w;
if (auto iter = loop.increment()) {
    MAYBE(step, visit_Statement(*this, *iter));
    w.write_unformatted(step.value);
}
w.writeln("continue");
return w.out();