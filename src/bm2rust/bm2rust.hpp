/*license*/
#pragma once

#include <bm/binary_module.hpp>

namespace bm2rust {
    void to_rust(futils::binary::writer& w, const rebgn::BinaryModule& bm);
}  // namespace bm2rust
