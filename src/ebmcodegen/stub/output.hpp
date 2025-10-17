/*license*/
#pragma once
#include <vector>
#include <string>
#include "core/lexer/token.h"

namespace ebmcodegen {
    struct LineMap {
        std::uint64_t line = 0;
        brgen::lexer::Loc loc;
    };
    struct Output {
        std::vector<std::string> struct_names;
        std::vector<LineMap> line_maps;
    };
}  // namespace ebmcodegen
