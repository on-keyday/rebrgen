/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <bm/binary_module.hpp>
#include <file/file_view.h>
#include "bm2rust.hpp"
#include <file/file_stream.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <tool/common/em_main.h>
#endif

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::vector<std::string_view> args;
    bm2rust::Flags bm2rust_flags;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file", "FILE");
        ctx.VarBool(&bm2rust_flags.enable_async, "async", "enable async");
        ctx.VarBool(&bm2rust_flags.use_copy_on_write, "copy-on-write", "use copy-on-write vectors");
    }
};
auto& cout = futils::wrap::cout_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    rebgn::BinaryModule bm;
    futils::file::View view;
    if (auto res = view.open(flags.input); !res) {
        cout << res.error().error<std::string>() << '\n';
        return 1;
    }
    if (!view.data()) {
        cout << "Empty file\n";
        return 1;
    }
    futils::binary::reader r{view};
    auto err = bm.decode(r);
    if (err) {
        cout << err.error<std::string>() << '\n';
        return 1;
    }
    futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
    futils::binary::writer w{fs.get_direct_write_handler(), &fs};
    bm2rust::to_rust(w, bm, flags.bm2rust_flags);

    return 0;
}

int rust_main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}

#if defined(__EMSCRIPTEN__)
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, rust_main);
}
#else
int main(int argc, char** argv) {
    return rust_main(argc, argv);
}
#endif