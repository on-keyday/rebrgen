CodeWriter* w;
auto writer_added = add_writer(&w);
MAYBE(r, visit_Expression(*this, expression));
w->write_unformatted(r.value);
auto result = std::move(w->out());
return result;