/*license*/
#pragma once

#include <bm/binary_module.hpp>

namespace bm2rust {

    void to_rust(futils::binary::writer& w, const rebgn::BinaryModule& bm, bool enable_async);
}  // namespace bm2rust
