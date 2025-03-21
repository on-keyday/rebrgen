/*license*/
#include "bm2go.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2go::Flags bm2go_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
    }
};
DEFINE_ENTRY(Flags) {
    bm2go::to_go(w, bm,flags.bm2go_flags);
    return 0;
}
