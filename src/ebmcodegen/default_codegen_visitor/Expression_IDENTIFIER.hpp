/*license*/
MAYBE(stmt, module_.get_statement(id));
ebm::IdentifierRef ref;
stmt.body.visit([&](auto&& visitor, std::string_view name, auto&& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, ebm::IdentifierRef>) {
        // if (name == "name") {
        ref = value;
        //}
    }
    else
        VISITOR_RECURSE(visitor, name, value)
});

return module_.get_identifier_or(ref, ebm::AnyRef{stmt.id.id});
