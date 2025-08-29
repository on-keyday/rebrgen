/*license*/
#pragma once
#include "../converter.hpp"
#include "ebmgen/common.hpp"

namespace ebmgen {

    expected<void> transform(TransformContext& ctx, bool debug);
}  // namespace ebmgen
