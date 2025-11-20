/*license*/
#pragma once

#include "structs.hpp"
namespace ebmcodegen {
    void generate_class_based(std::string_view ns_name, CodeWriter& header, CodeWriter& source, std::map<std::string_view, Struct>& struct_map, bool is_codegen);
}