/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/debug_printer.hpp>
#include <sstream>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <file/file_stream.h>
#include <json/stringer.h>
#include <set>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <tool/common/em_main.h>
#endif
#include "output.hpp"

namespace ebmcodegen {
    struct Flags : futils::cmdline::templ::HelpOption {
        std::string_view input;
        std::string_view output;
        bool dump_code = false;
        bool show_flags = false;
        // std::vector<std::string_view> args;
        // static constexpr auto arg_desc = "[option] args...";
        const char* program_name;
        const char* lang_name = "";
        const char* lsp_name = "";
        const char* webworker_name = "";

        bool debug_unimplemented = false;

        std::string_view dump_test_file;

        std::set<std::string_view> web_filtered;

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            ctx.VarString<true>(&input, "input,i", "input EBM file", "FILE");
            ctx.VarString<true>(&output, "output,o", "output source code file (currently not working and always output to stdout)", "FILE");
            ctx.VarBool(&show_flags, "show-flags", "show all flags (for debug and code generation)");
            ctx.VarBool(&dump_code, "dump-code", "dump code (for debug)");
            ctx.VarString<true>(&dump_test_file, "test-info", "dump test info file", "FILE");
            ctx.VarBool(&debug_unimplemented, "debug-unimplemented", "debug unimplemented node (for debug)");
            web_filtered = {"help", "input", "output", "show-flags", "dump-code", "test-info"};
        }
    };
    namespace internal {
        int load_file(auto& flags, auto& output, futils::cmdline::option::Context& ctx, auto&& then) {
            if (flags.show_flags) {
                futils::json::Stringer<> str;
                str.set_indent("  ");
                auto root_obj = str.object();
                root_obj("lang_name", flags.lang_name);
                root_obj("lsp_name", flags.lsp_name);
                root_obj("webworker_name", flags.webworker_name);
                root_obj("flags", [&](auto& s) {
                    auto fields = s.array();
                    for (auto& opt : ctx.options()) {
                        fields([&](auto& s) {
                            auto obj = s.object();
                            obj("name", opt->mainname);
                            obj("help", opt->help);
                            obj("argdesc", opt->argdesc);
                            obj("type", opt->type);
                            obj("web_filtered", flags.web_filtered.contains(opt->mainname));
                        });
                    }
                });
                root_obj.close();
                futils::wrap::cout_wrap() << str.out() << '\n';
                return 0;
            }
            if (flags.input.empty()) {
                futils::wrap::cerr_wrap() << flags.program_name << ": " << "no input file\n";
                return 1;
            }
            ebm::ExtendedBinaryModule ebm;
            auto& cout = futils::wrap::cout_wrap();
            auto& cerr = futils::wrap::cerr_wrap();
            futils::file::View view;
            if (auto res = view.open(flags.input); !res) {
                cerr << flags.program_name << ": " << res.error().template error<std::string>() << '\n';
                return 1;
            }
            if (!view.data()) {
                cerr << flags.program_name << ": " << "Empty file\n";
                return 1;
            }
            futils::binary::reader r{view};
            auto err = ebm.decode(r);
            if (err) {
                if (flags.dump_code) {
                    std::stringstream ss;
                    ebmgen::DebugPrinter printer(ebm, ss);
                    printer.print_module();
                    cout << ss.str();
                }
                cerr << flags.program_name << ": " << err.error<std::string>() << '\n';
                return 1;
            }
            if (!r.empty()) {
                if (flags.dump_code) {
                    std::stringstream ss;
                    ebmgen::DebugPrinter printer(ebm, ss);
                    printer.print_module();
                    cout << ss.str();
                }
                cerr << flags.program_name << ": " << "unexpected remaining data for input\n";
                return 1;
            }
            futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
            futils::binary::writer w{fs.get_direct_write_handler(), &fs};
            int ret = then(w, ebm, output);
            if (flags.dump_test_file.size()) {
                auto file = futils::file::File::create(flags.dump_test_file);
                if (!file) {
                    cerr << flags.program_name << ": " << file.error().template error<std::string>() << '\n';
                    return 1;
                }
                futils::file::FileStream<std::string> fs{*file};
                futils::binary::writer w{fs.get_direct_write_handler(), &fs};
                futils::json::Stringer str;
                auto obj = str.object();
                obj("line_map", [&](auto& s) { auto _ = s.array(); });  // for future use
                obj("structs", output.struct_names);
                obj.close();
                w.write(str.out());
            }
            return ret;
        }
    }  // namespace internal
}  // namespace ebmcodegen

#define DEFINE_ENTRY(FlagType, OutputType)                                                                                                                                                                                    \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, ebm::ExtendedBinaryModule& ebm, OutputType& output);                                                                          \
    int ebmcodegen_main(int argc, char** argv) {                                                                                                                                                                              \
        FlagType flags;                                                                                                                                                                                                       \
        OutputType output;                                                                                                                                                                                                    \
        flags.program_name = argv[0];                                                                                                                                                                                         \
        return futils::cmdline::templ::parse_or_err<std::string>(                                                                                                                                                             \
            argc, argv, flags, [&](auto&& str, bool err) {  if(err){ futils::wrap::cerr_wrap()<< flags.program_name << ": " <<str; } else { futils::wrap::cout_wrap() << str;} },                                                                                                                                                                 \
            [&](FlagType& flags, futils::cmdline::option::Context& ctx) { return ebmcodegen::internal::load_file(flags, output, ctx, [&](auto& w, auto& ebm, auto& output) { return Main(flags, ctx, w, ebm, output); }); }); \
    }                                                                                                                                                                                                                         \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, ebm::ExtendedBinaryModule& ebm, OutputType& output)

int ebmcodegen_main(int argc, char** argv);
#if defined(__EMSCRIPTEN__)
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, ebmcodegen_main);
}
#else
int main(int argc, char** argv) {
    return ebmcodegen_main(argc, argv);
}
#endif
