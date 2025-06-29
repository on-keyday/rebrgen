#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "load_json.hpp"
#include "convert.hpp"
#include <file/file_stream.h> // Required for futils::file::FileStream
#include <binary/writer.h> // Required for futils::binary::writer

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file (if -, write to stdout)", "FILE");
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
    auto err = ebmgen::convert_ast_to_ebm(*ast, ebm);
    if (err) {
        cerr << err.error<std::string>() << '\n';
        return 1;
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

    cout << "ebmgen finished successfully!\n";
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, 
        [](auto&& str, bool err) { 
            if(err) cerr << str;
            else cout << str;
        },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        }
    );
}
