// This code is included within the visit_Expression_WRITE_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
CodeWriter w;
// Get the IOData statement object from the io_statement parameter
MAYBE(io_data_stmt_obj, this->module_.get_statement(io_statement));
if (io_data_stmt_obj.body.kind != ebm::StatementOp::WRITE_DATA) {  // Should be WRITE_DATA
    return unexpect_error("Expected WRITE_DATA statement for Expression_WRITE_DATA");
}
auto& io_data = *io_data_stmt_obj.body.write_data();

// Get data type string
MAYBE(data_type, this->module_.get_type(io_data.data_type));
MAYBE(data_type_str, visit_Type(*this, data_type));

// Get target expression object from the target_expr parameter
MAYBE(target_expr_obj, this->module_.get_expression(target_expr));
// Need to visit target_expr_obj to get its Python representation
// For now, just a placeholder
std::string target_expr_str = "/* target_expression_value */";

// Generate Python code for writing
// This is a placeholder. Actual implementation will depend on how binary writing is done in Python.
// Need to handle io_data.size.unit, io_data.size.size(), io_data.size.ref()
// Need to handle io_data.attribute (endianness, signedness)
w.writeln("write_data_to_stream(", target_expr_str, ", ", data_type_str, ", /* size_info */, /* attribute_info */)");

return w.out();