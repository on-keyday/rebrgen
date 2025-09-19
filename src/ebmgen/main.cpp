#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "core/byte.h"
#include <wrap/iostream.h>
#include "ebmgen/json_printer.hpp"
#include "ebmgen/mapping.hpp"
#include "fnet/util/base64.h"
#include "json/stringer.h"
#include "load_json.hpp"
#include "convert.hpp"
#include "debug_printer.hpp"  // Include the new header
#include "transform/control_flow_graph.hpp"
#include <file/file_stream.h>  // Required for futils::file::FileStream
#include <binary/writer.h>     // Required for futils::binary::writer
#include <fstream>             // Required for std::ofstream
#include <sstream>             // Required for std::stringstream

enum class DebugOutputFormat {
    Text,
    JSON,
};

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::string_view debug_output;  // New flag for debug output
    DebugOutputFormat format = DebugOutputFormat::Text;
    std::string_view cfg_output;
    bool base64 = false;
    bool verbose = false;
    bool debug = false;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file (if -, write to stdout)", "FILE");
        ctx.VarString<true>(&debug_output, "d,debug-print", "debug output file", "FILE");
        ctx.VarMap(&format, "debug-format", "debug output format (default: text)", "{text,json}", std::map<std::string, DebugOutputFormat>{{"text", DebugOutputFormat::Text}, {"json", DebugOutputFormat::JSON}});
        ctx.VarString<true>(&cfg_output, "c,cfg-output", "control flow graph output file", "FILE");
        ctx.VarBool(&base64, "base64", "output as base64 encoding (for web playground)");
        ctx.VarBool(&verbose, "v,verbose", "verbose output (for debug)");
        ctx.VarBool(&debug, "g,debug", "enable debug transformations (do not remove unused items)");
    }
};

auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    ebmgen::verbose_error = flags.verbose;
    auto ast = ebmgen::load_json(flags.input, nullptr);
    if (!ast) {
        cerr << ast.error().error<std::string>() << '\n';
        return 1;
    }
    ebm::ExtendedBinaryModule ebm;
    auto output = ebmgen::convert_ast_to_ebm(ast->first, std::move(ast->second), ebm, {.not_remove_unused = flags.debug});
    if (!output) {
        cerr << "Convert Error: " << output.error().error<std::string>() << '\n';
        return 1;
    }

    std::optional<ebmgen::MappingTable> table;
    if (!flags.debug_output.empty() || !flags.cfg_output.empty()) {
        table.emplace(ebm);
    }

    // Debug print if requested
    if (!flags.debug_output.empty()) {
        auto save_to_file = [&](auto&& out) {
            std::ofstream debug_ofs(std::string(flags.debug_output));
            if (!debug_ofs.is_open()) {
                cerr << "Failed to open debug output file: " << flags.debug_output << '\n';
                return 1;
            }
            debug_ofs << out;
            debug_ofs.close();
            return 0;
        };
        if (flags.format == DebugOutputFormat::Text) {
            std::stringstream debug_ss;
            ebmgen::DebugPrinter printer(*table, debug_ss);

            printer.print_module();
            auto ret = save_to_file(debug_ss.str());
            if (ret != 0) {
                return ret;
            }
        }
        else if (flags.format == DebugOutputFormat::JSON) {
            ebmgen::JSONPrinter p(*table);
            futils::json::Stringer<> s;
            p.print_module(s);
            auto ret = save_to_file(s.out());
            if (ret != 0) {
                return ret;
            }
        }
        else {
            cerr << "Unsupported format\n";
            return 1;
        }
    }

    // also generates control flow graph
    if (!flags.cfg_output.empty()) {
        std::ofstream debug_ofs(std::string(flags.cfg_output));
        if (!debug_ofs.is_open()) {
            cerr << "Failed to open control flow graph file: " << flags.cfg_output << '\n';
            return 1;
        }
        futils::binary::writer w{&futils::wrap::iostream_adapter<futils::byte>::out, &debug_ofs};
        ebmgen::write_cfg(w, output->control_flow_graph, *table);
    }

    auto write_ebm = [&](futils::binary::writer& w) {
        if (flags.base64) {
            std::string buffer;
            futils::binary::writer temp_w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            auto err = ebm.encode(temp_w);
            if (err) {
                cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
                return 1;
            }
            std::string output;
            if (!futils::base64::encode(buffer, output)) {
                cerr << "Failed to encode EBM to base64: MAYBE BUG\n";
                return 1;
            }
            if (!w.write(output)) {
                cerr << "Failed to write base64 output\n";
                return 1;
            }
        }
        else {
            auto err = ebm.encode(w);
            if (err) {
                cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
                return 1;
            }
        }
        return 0;
    };

    if (flags.output == "-") {
        futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
        futils::binary::writer writer{fs.get_write_handler(), &fs};
        if (write_ebm(writer)) {
            return 1;
        }
    }
    else if (!flags.output.empty()) {
        // Serialize and write to output file
        auto file = futils::file::File::create(flags.output);
        if (!file) {
            cerr << "Failed to open output file: " << flags.output << ": " << file.error().error<std::string>() << '\n';
            return 1;
        }

        futils::file::FileStream<std::string> fs{*file};
        futils::binary::writer writer{fs.get_write_handler(), &fs};
        if (write_ebm(writer)) {
            return 1;
        }

        if (flags.verbose) {
            cerr << "ebmgen finished successfully!\n"
                 << "Output written to: " << flags.output << '\n';
        }
    }
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