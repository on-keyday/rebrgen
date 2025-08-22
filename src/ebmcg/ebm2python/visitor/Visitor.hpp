// Helper function to convert EBM type to Python type string
    expected<std::string> type_to_python_str(const ebm::TypeRef& type_ref) const {
        // Handle null TypeRef (id.value() == 0)
        if (type_ref.id.value() == 0) {
            return "None";
        }

        const auto* type_ptr = module_.get_type(type_ref); // Get raw pointer
        if (type_ptr == nullptr) {
            return unexpect_error("Failed to get type for TypeRef ID: {}", type_ref.id.value());
        }
        const auto& type = *type_ptr; // Dereference to get the actual type object

        switch (type.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
                return "int";
            case ebm::TypeKind::FLOAT:
                return "float";
            case ebm::TypeKind::BOOL:
                return "bool";
            case ebm::TypeKind::STRUCT:
            case ebm::TypeKind::RECURSIVE_STRUCT: { // Handle RECURSIVE_STRUCT here as well
                // For structs, get the name of the struct
                MAYBE(struct_decl_stmt, module_.get_statement(*type.body.id()));
                auto struct_name = module_.get_identifier_or(struct_decl_stmt.body.struct_decl()->name, ebm::AnyRef{type.id.id}, "Struct");
                return struct_name;
            }
            case ebm::TypeKind::VOID:
                return "None";
            case ebm::TypeKind::META:
                return "Any";
            case ebm::TypeKind::ARRAY:
            case ebm::TypeKind::VECTOR: { // Handle both ARRAY and VECTOR here
                MAYBE(element_type_obj, module_.get_type(*type.body.element_type()));
                if ((element_type_obj.body.kind == ebm::TypeKind::UINT || element_type_obj.body.kind == ebm::TypeKind::INT) &&
                    element_type_obj.body.size() && *element_type_obj.body.size() == 8) {
                    return "bytes";
                } else {
                    MAYBE(element_type_str, type_to_python_str(*type.body.element_type())); // Recursive call
                    return "list[" + element_type_str + "]";
                }
            }
            case ebm::TypeKind::ENUM: {
                // For enums, get the name of the enum
                MAYBE(enum_decl_stmt, module_.get_statement(*type.body.id()));
                auto enum_name = module_.get_identifier_or(enum_decl_stmt.body.enum_decl()->name, ebm::AnyRef{type.id.id}, "Enum");
                return enum_name;
            }
            case ebm::TypeKind::ENCODER_RETURN:
            case ebm::TypeKind::DECODER_RETURN:
                return "bytes"; // Or some other appropriate type for return values
            case ebm::TypeKind::ENCODER_INPUT: // Add this case
            case ebm::TypeKind::DECODER_INPUT: // Add this case
                return "bytes"; // Map encoder/decoder input to bytes
            case ebm::TypeKind::FUNCTION: {
                // For functions, return a Callable type hint
                MAYBE(return_type_str, type_to_python_str(*type.body.return_type()));
                std::string params_str = ""; // Reverted: params_str is empty here
                return "Callable[[" + params_str + "], " + return_type_str + "]";
            }
            case ebm::TypeKind::VARIANT: {
                // For variants, return Union of member types
                std::string members_str = "";
                for (auto& member_type_ref : type.body.members()->container) {
                    MAYBE(member_str, type_to_python_str(member_type_ref));
                    if (!members_str.empty()) {
                        members_str += ", ";
                    }
                    members_str += member_str;
                }
                return "Union[" + members_str + "]";
            }
            case ebm::TypeKind::RANGE: {
                // For ranges, return a tuple or a custom Range type
                MAYBE(base_type_str, type_to_python_str(*type.body.base_type()));
                return "tuple[" + base_type_str + ", " + base_type_str + "]";
            }
            case ebm::TypeKind::OPTIONAL: {
                // For optional types, return Optional type hint
                MAYBE(inner_type_str, type_to_python_str(*type.body.inner_type()));
                return "Optional[" + inner_type_str + "]";
            }
            case ebm::TypeKind::PTR: {
                // For pointers, return Any or a specific type if context allows
                MAYBE(pointee_type_str, type_to_python_str(*type.body.pointee_type()));
                return "Any"; // Or "Pointer[" + pointee_type_str + "]" if a custom type is defined
            }
            default:
                return unexpect_error("Unhandled TypeKind: {}", to_string(type.body.kind)); // Return an error for unhandled types
        }
    }

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
                endian_prefix = "@"; // Native byte order
                break;
            case ebm::Endian::unspec:
                // Default to network byte order (big-endian) if unspecified, or raise an error
                endian_prefix = "!"; // Network byte order (big-endian)
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
                auto int_size = size.size()->value(); // Assuming Varint has a value() method

                if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                    if (int_size == 1) format_char = attribute.sign() ? "b" : "B";
                    else if (int_size == 2) format_char = attribute.sign() ? "h" : "H";
                    else if (int_size == 4) format_char = attribute.sign() ? "i" : "I";
                    else if (int_size == 8) format_char = attribute.sign() ? "q" : "Q";
                    else return unexpect_error("Unsupported integer size for struct format: {} bytes", int_size);
                } else if (size.unit == ebm::SizeUnit::BIT_FIXED) {
                    // Bit fields are not directly supported by struct module.
                    // This will require custom bit manipulation in Python.
                    // For now, return an error or a placeholder.
                    return unexpect_error("Bit field integer types not supported in struct format string generation.");
                } else {
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
                    if (float_size == 4) format_char = "f";
                    else if (float_size == 8) format_char = "d";
                    else return unexpect_error("Unsupported float size for struct format: {} bytes", float_size);
                } else {
                    return unexpect_error("Unsupported size unit for float type: {}", to_string(size.unit));
                }
                break;
            }
            case ebm::TypeKind::BOOL:
                format_char = "?"; // Boolean type
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
                } else {
                    return unexpect_error("Unsupported array/vector type for struct format string generation.");
                }
                break;
            }
            default:
                return unexpect_error("Unhandled TypeKind for struct format: {}", to_string(type.body.kind));
        }

        return endian_prefix + format_char;
    }