/*license*/
// Code generated by gen_template, DO NOT EDIT.
#include "bm2cpp3.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2cpp3::Flags bm2cpp3_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
    }
};
DEFINE_ENTRY(Flags,bm2cpp3::Output) {
    bm2cpp3::to_cpp3(w, bm,flags.bm2cpp3_flags,output);
    return 0;
}
