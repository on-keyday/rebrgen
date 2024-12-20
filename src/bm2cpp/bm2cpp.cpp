/*license*/
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
        for (size_t i = 0; i < bm.code.size(); i++) {
            auto& code = bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_FORMAT: {
                    auto& ident = ctx.ident_table[code.ident().value().value()];
                    ctx.cw.writeln("struct ", ident, " {");
                    break;
                }
                case rebgn::AbstractOp::END_FORMAT: {
                    ctx.cw.writeln("};");
                    break;
                }
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }
}  // namespace bm2cpp