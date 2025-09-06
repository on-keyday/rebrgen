/*license*/
#pragma once
#include "../converter.hpp"
#include "control_flow_graph.hpp"
#include "ebmgen/common.hpp"

namespace ebmgen {

    expected<CFGList> transform(TransformContext& ctx, bool debug);
}  // namespace ebmgen
