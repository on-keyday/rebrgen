
MAYBE(base_str, visit_Expression(*this, base));
MAYBE(index_str, visit_Expression(*this, index));

return std::format("{}[{}]", base_str.value, index_str.value);