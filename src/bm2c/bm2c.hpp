/*license*/
#pragma once
#include <binary/writer.h>
#include <bm/binary_module.hpp>
namespace bm2c {
    struct Flags {
    };
    void to_c(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);
}  // namespace bm2c
