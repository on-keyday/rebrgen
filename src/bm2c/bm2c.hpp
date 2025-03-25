/*license*/
#pragma once
#include <binary/writer.h>
#include <bm/binary_module.hpp>
#include <bm2/output.hpp>
namespace bm2c {
    struct Flags {
    };
    void to_c(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags,bm2::Output& output);
}  // namespace bm2c
