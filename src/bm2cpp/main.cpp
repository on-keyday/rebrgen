/*license*/
#include "bm2cpp.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
};

DEFINE_ENTRY(Flags) {
    bm2cpp::to_cpp(w, bm);
    return 0;
}
