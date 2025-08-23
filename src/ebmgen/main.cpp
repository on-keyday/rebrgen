#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "load_json.hpp"
#include "convert.hpp"
#include "debug_printer.hpp"   // Include the new header
#include <file/file_stream.h>  // Required for futils::file::FileStream
#include <binary/writer.h>     // Required for futils::binary::writer
#include <fstream>             // Required for std::ofstream
#include <sstream>             // Required for std::stringstream

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::string_view debug_output;  // New flag for debug output

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file (if -, write to stdout)", "FILE");
        ctx.VarString<true>(&debug_output, "d,debug-print", "debug output file", "FILE");  // Bind new flag
    }
};

auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    auto ast = ebmgen::load_json(flags.input, nullptr);
    if (!ast) {
        cerr << ast.error().error<std::string>() << '\n';
        return 1;
    }
    ebm::ExtendedBinaryModule ebm;
    auto err = ebmgen::convert_ast_to_ebm(ast->first, std::move(ast->second), ebm);
    if (err) {
        cerr << "Convert Error: " << err.error<std::string>() << '\n';
        return 1;
    }

    // Debug print if requested
    if (!flags.debug_output.empty()) {
        std::stringstream debug_ss;
        ebmgen::DebugPrinter printer(ebm, debug_ss);
        printer.print_module();

        std::ofstream debug_ofs(std::string(flags.debug_output));
        if (!debug_ofs.is_open()) {
            cerr << "Failed to open debug output file: " << flags.debug_output << '\n';
            return 1;
        }
        debug_ofs << debug_ss.str();
        debug_ofs.close();
    }

    // Serialize and write to output file
    futils::file::FileError fserr;
    auto file = futils::file::File::create(flags.output);
    if (!file) {
        cerr << "Failed to open output file: " << flags.output << ": " << file.error().error<std::string>() << '\n';
        return 1;
    }
    futils::file::FileStream<std::string> fs{*file};
    futils::binary::writer writer{fs.get_write_handler(), &fs};
    err = ebm.encode(writer);
    if (err) {
        cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
        return 1;
    }

    cout << "ebmgen finished successfully!\n"
         << "Output written to: " << flags.output << '\n';
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags,
        [](auto&& str, bool err) {
            if (err)
                cerr << str;
            else
                cout << str;
        },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}