auto loop_ref = continue_.related_statement;
CodeWriter w;
while (!is_nil(loop_ref)) {
    MAYBE(loop_stmt, module_.get_statement(loop_ref));
    MAYBE(loop, loop_stmt.body.loop());
    if (auto iter = loop.increment()) {
        MAYBE(step, visit_Statement(*this, *iter));
        w.write_unformatted(step.value);
        break;
    }
    loop_ref = loop.lowered_statement.id;
}

w.writeln("continue");
return w.out();