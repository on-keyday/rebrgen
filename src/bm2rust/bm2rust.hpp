/*license*/
#pragma once

#include <bm/binary_module.hpp>

namespace bm2rust {
    struct Flags {
        bool use_async = false;
        bool use_copy_on_write_vec = false;
    };
    void to_rust(futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);
}  // namespace bm2rust
