

// Helper function to convert EBM type and attributes to Python struct format string
expected<std::string> type_to_struct_format(const ebm::TypeRef& type_ref, const ebm::IOAttribute& attribute, const ebm::Size& size) const {
    // Handle endianness prefix
    std::string endian_prefix;
    switch (attribute.endian()) {
        case ebm::Endian::little:
            endian_prefix = "<";
            break;
        case ebm::Endian::big:
            endian_prefix = ">";
            break;
        case ebm::Endian::native:
            endian_prefix = "@";  // Native byte order
            break;
        case ebm::Endian::unspec:
            // Default to network byte order (big-endian) if unspecified, or raise an error
            endian_prefix = "!";  // Network byte order (big-endian)
            break;
        case ebm::Endian::dynamic:
            // This case needs special handling in the generated Python code, not here.
            // For now, return an error or a placeholder.
            return unexpect_error("Dynamic endianness not supported in struct format string generation.");
    }

    const auto* type_ptr = module_.get_type(type_ref);
    if (type_ptr == nullptr) {
        return unexpect_error("Failed to get type for TypeRef ID: {}", type_ref.id.value());
    }
    const auto& type = *type_ptr;

    std::string format_char;
    switch (type.body.kind) {
        case ebm::TypeKind::INT:
        case ebm::TypeKind::UINT: {
            // Determine size and signedness
            if (!size.size()) {
                return unexpect_error("Integer type requires a fixed size for struct format.");
            }
            auto int_size = size.size()->value();  // Assuming Varint has a value() method

            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                if (int_size == 1)
                    format_char = attribute.sign() ? "b" : "B";
                else if (int_size == 2)
                    format_char = attribute.sign() ? "h" : "H";
                else if (int_size == 4)
                    format_char = attribute.sign() ? "i" : "I";
                else if (int_size == 8)
                    format_char = attribute.sign() ? "q" : "Q";
                else
                    return unexpect_error("Unsupported integer size for struct format: {} bytes", int_size);
            }
            else if (size.unit == ebm::SizeUnit::BIT_FIXED) {
                // Bit fields are not directly supported by struct module.
                // This will require custom bit manipulation in Python.
                // For now, return an error or a placeholder.
                return unexpect_error("Bit field integer types not supported in struct format string generation.");
            }
            else {
                return unexpect_error("Unsupported size unit for integer type: {}", to_string(size.unit));
            }
            break;
        }
        case ebm::TypeKind::FLOAT: {
            if (!size.size()) {
                return unexpect_error("Float type requires a fixed size for struct format.");
            }
            auto float_size = size.size()->value();
            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                if (float_size == 4)
                    format_char = "f";
                else if (float_size == 8)
                    format_char = "d";
                else
                    return unexpect_error("Unsupported float size for struct format: {} bytes", float_size);
            }
            else {
                return unexpect_error("Unsupported size unit for float type: {}", to_string(size.unit));
            }
            break;
        }
        case ebm::TypeKind::BOOL:
            format_char = "?";  // Boolean type
            break;
        case ebm::TypeKind::ARRAY:
        case ebm::TypeKind::VECTOR: {
            // For arrays/vectors of bytes, use 's' format.
            // Otherwise, struct module doesn't directly support arrays of arbitrary types.
            // This will require custom loop-based reading in Python.
            MAYBE(element_type_obj, module_.get_type(*type.body.element_type()));
            if ((element_type_obj.body.kind == ebm::TypeKind::UINT || element_type_obj.body.kind == ebm::TypeKind::INT) &&
                element_type_obj.body.size() && *element_type_obj.body.size() == 8 && size.unit == ebm::SizeUnit::BYTE_FIXED) {
                // This is a byte array, use 's' format with the total size
                if (!size.size()) {
                    return unexpect_error("Byte array type requires a fixed size for struct format.");
                }
                format_char = std::to_string(size.size()->value()) + "s";
            }
            else {
                return unexpect_error("Unsupported array/vector type for struct format string generation.");
            }
            break;
        }
        default:
            return unexpect_error("Unhandled TypeKind for struct format: {}", to_string(type.body.kind));
    }

    return endian_prefix + format_char;
}