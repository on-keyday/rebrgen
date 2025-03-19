/*license*/
#include "bm2python.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2python::Flags bm2python_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
        ctx.VarBool(&bm2python_flags.use_async, "async", "use async operation");
    }
};
DEFINE_ENTRY(Flags) {
    bm2python::to_python(w, bm,flags.bm2python_flags);
    return 0;
}
