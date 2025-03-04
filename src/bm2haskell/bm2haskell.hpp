/*license*/
#pragma once
#include <binary/writer.h>
#include <bm/binary_module.hpp>
namespace bm2haskell {
    struct Flags {
    };
    void to_haskell(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);
}  // namespace bm2haskell
