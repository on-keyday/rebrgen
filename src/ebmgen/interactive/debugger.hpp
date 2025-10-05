/*license*/
#pragma once

#include "ebmgen/mapping.hpp"

namespace ebmgen {
    void interactive_debugger(MappingTable& table);
    using ObjectResult = std::vector<ebm::AnyRef>;
    expected<ObjectResult> run_query(MappingTable& table, std::string_view input);
}  // namespace ebmgen
