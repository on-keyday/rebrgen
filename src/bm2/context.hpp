/*license*/
#pragma once
#include <string>
#include <code/code_writer.h>
#include <binary/writer.h>
#include <unordered_map>
#include <bm/binary_module.hpp>

namespace bm2 {
    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct BitFieldState {
        std::uint64_t ident;
        std::uint64_t bit_size;
        rebgn::PackedOpType op;
        rebgn::EndianExpr endian;
    };

    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;
        std::unordered_map<std::uint64_t, std::uint64_t> ident_index_table;
        std::unordered_map<std::uint64_t, rebgn::Range> ident_range_table;
        std::unordered_map<std::uint64_t, std::string> metadata_table;
        std::unordered_map<std::uint64_t, rebgn::Storages> storage_table;
        std::string ptr_type;
        rebgn::BMContext bm_ctx;
        std::vector<futils::helper::DynDefer> on_functions;
        std::vector<std::string> this_as;
        std::vector<BitFieldState> bit_field_ident;

        std::vector<std::string> current_r;
        std::vector<std::string> current_w;

        bool on_assign = false;

        std::string root_r = "r";
        std::string root_w = "w";
        std::string root_this = "(*this)";

        std::string r() {
            if (current_r.empty()) {
                return root_r;
            }
            return current_r.back();
        }

        std::string w() {
            if (current_w.empty()) {
                return root_w;
            }
            return current_w.back();
        }

        std::string this_() {
            if (this_as.empty()) {
                return root_this;
            }
            return this_as.back();
        }

        const rebgn::Code& ref(rebgn::Varint ident) {
            return bm.code[ident_index_table[ident.value()]];
        }

        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm,
                std::string_view root_r, std::string_view root_w, std::string_view root_this)
            : cw(w), bm(bm), root_r(root_r), root_w(root_w), root_this(root_this) {
        }
    };

}  // namespace bm2