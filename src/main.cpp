/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>

#include <wrap/cout.h>
#include "convert.hpp"
#include <file/file_stream.h>
namespace rebgn {
    rebgn::expected<std::shared_ptr<brgen::ast::Node>> load_json(std::string_view input);
}
struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::vector<std::string_view> args;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file", "FILE");
    }
};
auto& cout = futils::wrap::cout_wrap();

void print_code(rebgn::Module& m) {
    for (auto& [str, id] : m.string_table) {
        cout << "string " << str << " " << id << '\n';
    }
    for (auto& [id, num] : m.ident_table) {
        cout << "ident " << id->ident << " " << num;
        auto found = m.ident_index_table.find(num);
        if (found != m.ident_index_table.end()) {
            cout << " " << to_string(m.code[found->second].op);
        }
        cout << '\n';
    }
    std::string nest;
    for (auto& c : m.code) {
        switch (c.op) {
            case rebgn::AbstractOp::END_ENUM:
            case rebgn::AbstractOp::END_FORMAT:
            case rebgn::AbstractOp::END_FIELD:
            case rebgn::AbstractOp::END_ENUM_MEMBER:
            case rebgn::AbstractOp::END_UNION:
            case rebgn::AbstractOp::END_UNION_MEMBER:
            case rebgn::AbstractOp::END_FUNCTION:
            case rebgn::AbstractOp::END_LOOP:
                nest.resize(nest.size() - 2);
                break;
            default:
                break;
        }

        cout << nest << to_string(c.op);
        if (auto ident = c.ident()) {
            cout << " " << ident->value();
        }
        if (auto ref = c.ref()) {
            cout << " " << ref->value();
        }
        if (auto bop = c.bop()) {
            cout << " " << to_string(*bop);
        }
        if (auto left_ref = c.left_ref()) {
            cout << " " << left_ref->value();
        }
        if (auto right_ref = c.right_ref()) {
            cout << " " << right_ref->value();
        }
        if (auto int_value = c.int_value()) {
            cout << " " << int_value->value();
        }
        if (auto int_value64 = c.int_value64()) {
            cout << " " << *int_value64;
        }
        if (auto s = c.storage()) {
            for (auto& st : s->storages) {
                cout << " " << to_string(st.type);
                if (auto size = st.size()) {
                    cout << " " << size->value();
                }
                if (auto ref = st.ref()) {
                    cout << " " << ref->value();
                }
            }
        }
        cout << '\n';
        switch (c.op) {
            case rebgn::AbstractOp::DEFINE_ENUM:
            case rebgn::AbstractOp::DEFINE_FORMAT:
            case rebgn::AbstractOp::DEFINE_FIELD:
            case rebgn::AbstractOp::DEFINE_ENUM_MEMBER:
            case rebgn::AbstractOp::DEFINE_UNION:
            case rebgn::AbstractOp::DEFINE_UNION_MEMBER:
            case rebgn::AbstractOp::DEFINE_FUNCTION:
            case rebgn::AbstractOp::LOOP_CONDITION:
            case rebgn::AbstractOp::LOOP_INFINITE:
                nest += "  ";
                break;
            default:
                break;
        }
    }
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    auto res = rebgn::load_json(flags.input);
    if (!res) {
        cout << res.error().error<std::string>() << '\n';
        return 1;
    }
    auto m = rebgn::convert(*res);
    if (!m) {
        cout << m.error().error<std::string>() << '\n';
        return 1;
    }
    print_code(*m);
    if (flags.output.size()) {
        auto file = futils::file::File::create(flags.output);
        if (!file) {
            cout << file.error().error<std::string>() << '\n';
            return 1;
        }
        futils::file::FileStream<std::string> fs{*file};
        futils::binary::writer w{fs.get_direct_write_handler(), &fs};
        auto err = rebgn::save(*m, w);
        if (err) {
            if (fs.error.err_code != 0) {
                cout << fs.error.error<std::string>() << '\n';
            }
            cout << err.error<std::string>() << '\n';
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
