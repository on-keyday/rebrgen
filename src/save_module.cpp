/*license*/
#include "convert.hpp"

namespace rebgn {
    Error save(Module& m, futils::binary::writer& w) {
        BinaryModule bm;
        auto add_refs = [&](StringRefs& refs, std::uint64_t id, std::string_view str) {
            StringRef sr;
            auto idv = varint(id);
            if (!idv) {
                return idv.error();
            }
            sr.code = idv.value();
            auto length = varint(str.length());
            if (!length) {
                return length.error();
            }
            sr.string.length = length.value();
            sr.string.data = str;
            refs.refs.push_back(std::move(sr));
            return none;
        };
        for (auto& [str, id] : m.string_table) {
            auto err = add_refs(bm.strings, id, str);
            if (err) {
                return err;
            }
        }
        auto length = varint(bm.strings.refs.size());
        if (!length) {
            return length.error();
        }
        bm.strings.refs_length = length.value();
        for (auto& [ident, id] : m.ident_table) {
            auto err = add_refs(bm.identifiers, id, ident->ident);
            if (err) {
                return err;
            }
        }
        length = varint(bm.identifiers.refs.size());
        if (!length) {
            return length.error();
        }
        bm.identifiers.refs_length = length.value();
        for (auto& [ident, index] : m.ident_index_table) {
            auto id = varint(ident);
            if (!id) {
                return id.error();
            }
            auto idx = varint(index);
            if (!idx) {
                return idx.error();
            }
            IdentIndex ii;
            ii.ident = id.value();
            ii.index = idx.value();
            bm.ident_indexes.refs.push_back(std::move(ii));
        }
        length = varint(bm.ident_indexes.refs.size());
        if (!length) {
            return length.error();
        }
        bm.ident_indexes.refs_length = length.value();
        auto code_length = varint(m.code.size());
        if (!code_length) {
            return code_length.error();
        }
        bm.code_length = code_length.value();
        bm.code = m.code;
        return bm.encode(w);
    }
}  // namespace rebgn
