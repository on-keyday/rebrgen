// src/ebmcg/ebm2python/visitor/Visitor.hpp

// Helper function to convert EBM type to Python type string
expected<std::string> type_to_python_str(const ebm::TypeRef& type_ref) {
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
