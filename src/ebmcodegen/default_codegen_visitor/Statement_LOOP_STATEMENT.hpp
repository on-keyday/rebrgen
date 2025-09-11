/*license*/
if (!is_nil(loop.lowered_statement.id)) {
    return visit_Statement(*this, loop.lowered_statement.id);
}
CodeWriter w;
if (auto init = loop.init(); init && init->id.value() != 0) {
    MAYBE(init_s, visit_Statement(*this, *init));
    w.write_unformatted(init_s.value);
}
std::string cond;
if (auto c = loop.condition()) {
    MAYBE(expr, visit_Expression(*this, c->cond));
    cond = std::move(expr.value);
}
else {
    ebm::Expression bool_true;
    bool_true.body.kind = ebm::ExpressionOp::LITERAL_BOOL;
    bool_true.body.bool_value(1);
    MAYBE(expr, visit_Expression(*this, bool_true));
    cond = std::move(expr.value);
}

if (use_brace_for_condition) {
    w.writeln("while (", tidy_condition_brace(std::move(cond)), ") {");
}
else {
    w.writeln("while ", tidy_condition_brace(std::move(cond)), " ", begin_block);
}
{
    auto body_indent = w.indent_scope();
    MAYBE(body, visit_Statement(*this, loop.body));
    w.write_unformatted(body.value);
    if (auto iter = loop.increment()) {
        MAYBE(step, visit_Statement(*this, *iter));
        w.write_unformatted(step.value);
    }
}
w.writeln(end_block);

return w.out();