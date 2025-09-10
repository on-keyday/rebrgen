auto writer_added = add_writer();
MAYBE(r, visit_Expression(*this, expression));
MAYBE(got, get_writer());
got.write_unformatted(r.value);
auto result = std::move(got.out());
return result;