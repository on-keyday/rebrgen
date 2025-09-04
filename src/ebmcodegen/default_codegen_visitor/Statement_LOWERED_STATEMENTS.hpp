if (!lowered_statements.container.size()) {
    return unexpect_error("No lowered statements found");
}
return visit_Statement(*this, lowered_statements.container[0].statement);