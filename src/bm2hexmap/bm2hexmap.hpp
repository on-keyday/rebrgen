/*license*/
#pragma once
#include <binary/writer.h>
#include <bm/binary_module.hpp>
namespace bm2hexmap {
    struct Flags {
        futils::view::rvec input_binary;  // input binary file
        std::string start_format_name;
    };
    void to_hexmap(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);
}  // namespace bm2hexmap
