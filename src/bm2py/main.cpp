/*license*/
#include "bm2py.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2py::Flags bm2py_flags;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
        ctx.VarBool(&bm2py_flags.is_async, "async", "use async operation");
    }
};
DEFINE_ENTRY(Flags) {
    bm2py::to_py(w, bm,flags.bm2py_flags);
    return 0;
}
