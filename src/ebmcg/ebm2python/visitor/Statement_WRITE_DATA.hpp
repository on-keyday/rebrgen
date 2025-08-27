// This code is included within the visit_Statement_WRITE_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.
CodeWriter w;

// Get the IOData statement object from the io_statement parameter
auto& io_data = write_data;

// Get data type string
MAYBE(data_type, this->module_.get_type(io_data.data_type));
MAYBE(data_type_str, visit_Type(*this, data_type));

// Get target expression object from the target_expr parameter
MAYBE(target_expr_obj, this->module_.get_expression(io_data.target));
// Visit target_expr_obj to get its Python representation
MAYBE(target_expr_str, visit_Expression(*this, target_expr_obj));

// Generate Python code for writing
MAYBE(struct_format, this->type_to_struct_format(io_data.data_type, io_data.attribute, io_data.size));

std::string write_size_str;
if (io_data.size.unit == ebm::SizeUnit::BYTE_FIXED) {
    if (!io_data.size.size()) {
        return unexpect_error("Fixed byte size is missing for WRITE_DATA operation.");
    }
    write_size_str = std::to_string(io_data.size.size()->value());
}
else if (io_data.size.unit == ebm::SizeUnit::BIT_FIXED) {
    if (!io_data.size.size()) {
        return unexpect_error("Fixed bit size is missing for WRITE_DATA operation.");
    }
    // Convert bits to bytes, rounding up
    write_size_str = std::to_string((io_data.size.size()->value() + 7) / 8);
}
else if (io_data.size.unit == ebm::SizeUnit::DYNAMIC ||
         io_data.size.unit == ebm::SizeUnit::BIT_DYNAMIC ||
         io_data.size.unit == ebm::SizeUnit::BYTE_DYNAMIC ||
         io_data.size.unit == ebm::SizeUnit::ELEMENT_DYNAMIC) {
    if (!io_data.size.ref()) {
        return unexpect_error("Dynamic size expression reference is missing for WRITE_DATA operation.");
    }
    ebm::ExpressionRef expr_ref = *io_data.size.ref();
    MAYBE(expr_obj, this->module_.get_expression(expr_ref));
    MAYBE(dynamic_size_expr_str, visit_Expression(*this, expr_obj));
    write_size_str = dynamic_size_expr_str;
}
else {
    return unexpect_error("Unsupported size unit for WRITE_DATA operation: {}", to_string(io_data.size.unit));
}

w.writeln("stream.write(struct.pack(\"" + struct_format + ", ", target_expr_str, "))");

return w.out();
