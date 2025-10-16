/*license*/
#pragma once
#include <vector>
#include <string>
#include "ebm/extended_binary_module.hpp"

namespace ebmcodegen {
    struct LineMap {
        std::uint64_t line = 0;
        ebm::Loc loc;
    };
    struct Output {
        std::vector<std::string> struct_names;
        std::vector<LineMap> line_maps;
    };
}  // namespace ebmcodegen
