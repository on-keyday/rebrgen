/*license*/
#pragma once
#include "../converter.hpp"
#include "control_flow_graph.hpp"
#include "ebmgen/common.hpp"

namespace ebmgen {

    expected<CFGList> transform(TransformContext& ctx, bool debug);

    ebm::Block* get_block(ebm::StatementBody& body);
    expected<void> vectorized_io(TransformContext& tctx, bool write);
    expected<void> remove_unused_object(TransformContext& ctx);
    expected<void> lowered_dynamic_bit_io(CFGContext& tctx, bool write);
    expected<void> merge_bit_field(TransformContext& tctx);
}  // namespace ebmgen
