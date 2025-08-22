// This code is included within the visit_Expression_READ_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.

// Get the IOData statement object from the io_statement parameter
MAYBE(io_data_stmt_obj, this->module_.get_statement(io_statement));
if (io_data_stmt_obj.body.kind != ebm::StatementOp::READ_DATA) { // Should be READ_DATA
    return unexpect_error("Expected READ_DATA statement for Expression_READ_DATA");
}
auto& io_data = *io_data_stmt_obj.body.read_data();

// Get data type string
MAYBE(data_type_str, this->type_to_python_str(io_data.data_type));

// Get target statement object from the target_stmt parameter
MAYBE(target_stmt_obj, this->module_.get_statement(target_stmt));
if (target_stmt_obj.body.kind != ebm::StatementOp::ASSIGNMENT) { // Should be ASSIGNMENT
    return unexpect_error("Expected ASSIGNMENT statement for target_stmt in Expression_READ_DATA");
}

// Resolve the target of the assignment to get the identifier name
MAYBE(target_expr_obj, this->module_.get_expression(*target_stmt_obj.body.target()));
if (target_expr_obj.body.kind != ebm::ExpressionOp::IDENTIFIER) {
    return unexpect_error("Expected IDENTIFIER expression for assignment target");
}
MAYBE(var_decl_stmt_obj, this->module_.get_statement(*target_expr_obj.body.id()));
if (var_decl_stmt_obj.body.kind != ebm::StatementOp::VARIABLE_DECL && var_decl_stmt_obj.body.kind != ebm::StatementOp::FIELD_DECL) {
    return unexpect_error("Expected VARIABLE_DECL or FIELD_DECL for identifier statement");
}

ebm::IdentifierRef target_identifier_ref;
if (var_decl_stmt_obj.body.kind == ebm::StatementOp::VARIABLE_DECL) {
    target_identifier_ref = var_decl_stmt_obj.body.var_decl()->name;
} else { // FIELD_DECL
    target_identifier_ref = var_decl_stmt_obj.body.field_decl()->name;
}

std::string target_var_name = this->module_.get_identifier_or(target_identifier_ref, ebm::AnyRef{target_identifier_ref.id}, "target_var");

// Generate Python code for reading
// Use the new type_to_struct_format helper
MAYBE(struct_format, this->type_to_struct_format(io_data.data_type, io_data.attribute, io_data.size));

// Determine the size in bytes for reading from stream
std::string read_size_str;
if (io_data.size.unit == ebm::SizeUnit::BYTE_FIXED) {
    if (!io_data.size.size()) {
        return unexpect_error("Fixed byte size is missing for READ_DATA operation.");
    }
    read_size_str = std::to_string(io_data.size.size()->value());
} else if (io_data.size.unit == ebm::SizeUnit::BIT_FIXED) {
    if (!io_data.size.size()) {
        return unexpect_error("Fixed bit size is missing for READ_DATA operation.");
    }
    // Convert bits to bytes, rounding up
    read_size_str = std::to_string((io_data.size.size()->value() + 7) / 8);
} else {
    // For dynamic sizes, we'll need to evaluate the expression first.
    // For now, return an error or a placeholder.
    return unexpect_error("Unsupported size unit for READ_DATA operation: {}", to_string(io_data.size.unit));
}

// Generate the Python code
this->root.writeln("import struct"); // Add import for struct module
this->root.writeln(target_var_name, " = struct.unpack(\", struct_format, \", stream.read(", read_size_str, "))[0]");

return {};
