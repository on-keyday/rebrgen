/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>

#include <wrap/cout.h>
#include "convert.hpp"
namespace rebgn {
    rebgn::expected<std::shared_ptr<brgen::ast::Node>> load_json(std::string_view input);
}
struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::vector<std::string_view> args;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
    }
};
auto& cout = futils::wrap::cout_wrap();

void print_code(rebgn::Module& m) {
    std::string nest;
    for (auto& c : m.code) {
        switch (c.op) {
            case rebgn::AbstractOp::END_ENUM:
            case rebgn::AbstractOp::END_FORMAT:
            case rebgn::AbstractOp::END_FIELD:
            case rebgn::AbstractOp::END_ENUM_MEMBER:
            case rebgn::AbstractOp::END_UNION:
            case rebgn::AbstractOp::END_UNION_MEMBER:
                nest.resize(nest.size() - 2);
                break;
            default:
                break;
        }

        cout << nest << to_string(c.op);
        if (auto ident = c.ident()) {
            cout << " " << ident->value();
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
