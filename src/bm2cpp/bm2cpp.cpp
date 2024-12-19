/*license*/
#pragma once
#include "bm2cpp.hpp"
#include <code/code_writer.h>
#include <unordered_map>

namespace bm2cpp {
    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;

        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm)
            : cw(w), bm(bm) {
        }
    };

    void to_cpp(futils::binary::writer& w, const rebgn::BinaryModule& bm) {
        Context ctx(w, bm);
        for (auto& sr : bm.strings.refs) {
            ctx.string_table[sr.code.value()] = sr.string.data;
        }
        for (auto& ir : bm.identifiers.refs) {
            ctx.ident_table[ir.code.value()] = ir.string.data;
        }
        std::vector<futils::helper::DynDefer> indents;
        for (auto& code : bm.code) {
            switch (code.op) {
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }
}  // namespace bm2cpp