/*license*/
for (auto& element : block.container) {
    auto elem = module_.get_statement(element);
    if (!elem) {
        return unexpect_error("Unexpected null pointer for Statement: {}", element.id.value());
    }
    MAYBE_VOID(result, visit_Statement(*this, *elem));
}