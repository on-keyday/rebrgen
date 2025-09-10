// This code is included within the visit_Expression_READ_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.

// Get the IOData statement object from the io_statement parameter
if (read_data.lowered_statement.id.id.value() != 0) {
    return visit_Statement(*this, read_data.lowered_statement.id);
}
auto& io_data = read_data;
// Visit target_expr_obj to get its Python representation
MAYBE(target_expr_str, visit_Expression(*this, io_data.target));

// Generate Python code for writing
MAYBE(struct_format, this->type_to_struct_format(io_data.data_type, io_data.attribute, io_data.size));

MAYBE(read_size_str, get_size_str(*this, io_data.size));

return target_expr_str.value + " = struct.unpack(" + struct_format + ", stream.read(" + read_size_str + "))[0]\n";