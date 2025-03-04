﻿/*license*/
#include "bm2haskell.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2haskell::Flags bm2haskell_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
    }
};
DEFINE_ENTRY(Flags) {
    bm2haskell::to_haskell(w, bm,flags.bm2haskell_flags);
    return 0;
}
