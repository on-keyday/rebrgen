// This code is included within the visit_Statement_WRITE_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
if (!is_nil(write_data.lowered_statement.id)) {
    return visit_Statement(*this, write_data.lowered_statement.id);
}

CodeWriter w;

// Get the IOData statement object from the io_statement parameter
auto& io_data = write_data;

// Visit target_expr_obj to get its Python representation
MAYBE(target_expr_str, visit_Expression(*this, io_data.target));

// Generate Python code for writing
MAYBE(struct_format, this->type_to_struct_format(io_data.data_type, io_data.attribute, io_data.size));

w.writeln("stream.write(struct.pack(" + struct_format + ", ", target_expr_str.value, "))");

return w.out();
