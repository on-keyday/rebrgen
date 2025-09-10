// This code is included within the visit_Expression_READ_DATA function.
// We can use variables like `this` (for Visitor) and function parameters directly.

// Get the IOData statement object from the io_statement parameter
auto& io_data = read_data;

// Get data type string
MAYBE(data_type, this->module_.get_type(io_data.data_type));
MAYBE(data_type_str, visit_Type(*this, data_type));

// Get target statement object from the target_stmt parameter
MAYBE(target_stmt_obj, this->module_.get_statement(target_stmt));
ebm::IdentifierRef target_identifier_ref;

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
}
else if (io_data.size.unit == ebm::SizeUnit::BIT_FIXED) {
    if (!io_data.size.size()) {
        return unexpect_error("Fixed bit size is missing for READ_DATA operation.");
    }
    // Convert bits to bytes, rounding up
    read_size_str = std::to_string((io_data.size.size()->value() + 7) / 8);
}
else {
    // For dynamic sizes, we'll need to evaluate the expression first.
    // For now, return an error or a placeholder.
    return unexpect_error("Unsupported size unit for READ_DATA operation: {}", to_string(io_data.size.unit));
}

return "struct.unpack(" + struct_format + ", stream.read(" + read_size_str + "))[0]";