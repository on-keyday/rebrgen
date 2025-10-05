/*license*/
#pragma once

#include "ebmgen/mapping.hpp"
#include <unordered_set>
namespace ebmgen {
    void interactive_debugger(MappingTable& table);
    using ObjectSet = std::unordered_set<ebm::AnyRef>;
    expected<ObjectSet> run_query(MappingTable& table, std::string_view input);
}  // namespace ebmgen
