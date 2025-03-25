/*license*/
#include "bm2c.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2c::Flags bm2c_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
    }
};
DEFINE_ENTRY(Flags) {
    bm2c::to_c(w, bm,flags.bm2c_flags,output);
    return 0;
}
