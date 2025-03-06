/*license*/
#pragma once
#include <binary/writer.h>
#include <bm/binary_module.hpp>
namespace bm2python {
    struct Flags {
        bool is_async = false;
    };
    void to_python(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);
}  // namespace bm2python
