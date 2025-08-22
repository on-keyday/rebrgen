/*license*/
CodeWriter w;
for (auto& element : block.container) {
    MAYBE(elem, module_.get_statement(element));
    MAYBE(result, visit_Statement(*this, elem));
    w.write_unformatted(result);
}
return w.out();