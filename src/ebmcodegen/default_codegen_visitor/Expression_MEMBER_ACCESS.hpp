/*license*/
MAYBE(base_str, visit_Expression(*this, base));
MAYBE(ident, module_.get_identifier(member));

return std::format("{}.{}", base_str.value, ident.body.data);