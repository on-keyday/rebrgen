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
    std::string_view cfg_output;
    std::vector<std::string_view> args;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file", "FILE");
        ctx.VarString<true>(&cfg_output, "c,cfg-output", "control flow graph output file", "FILE");
    }
};
auto& cout = futils::wrap::cout_wrap();

namespace rebgn {
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
                case rebgn::AbstractOp::END_CASE:
                case rebgn::AbstractOp::END_MATCH:
                case rebgn::AbstractOp::ELSE:
                case rebgn::AbstractOp::ELIF:
                case rebgn::AbstractOp::END_IF:
                case rebgn::AbstractOp::END_PROGRAM:
                    nest.resize(nest.size() - 2);
                    break;
                default:
                    break;
            }
            auto print_ref = [&](rebgn::Varint ref, bool use_index = true) {
                bool found_ident = false;
                if (use_index) {
                    if (auto found = m.ident_index_table.find(ref.value()); found != m.ident_index_table.end()) {
                        std::string_view s = to_string(m.code[found->second].op);
                        if (s.starts_with("DEFINE_")) {
                            s = s.substr(7);
                        }
                        cout << " " << s;
                        found_ident = true;
                    }
                }
                if (auto found = m.ident_table_rev.find(ref.value()); found != m.ident_table_rev.end()) {
                    cout << " " << found->second->ident;
                    found_ident = true;
                }
                if (auto found = m.string_table_rev.find(ref.value()); found != m.string_table_rev.end()) {
                    cout << " " << found->second;
                    found_ident = true;
                }
                if (found_ident) {
                    cout << "(" << ref.value() << ")";
                }
                else {
                    cout << " " << ref.value();
                }
            };
            cout << nest << to_string(c.op);
            if (auto uop = c.uop()) {
                cout << " " << to_string(*uop);
            }
            if (auto ident = c.ident()) {
                print_ref(*ident, false);
            }
            if (auto ref = c.ref()) {
                print_ref(*ref);
            }
            if (auto left_ref = c.left_ref()) {
                print_ref(*left_ref);
            }
            if (auto bop = c.bop()) {
                cout << " " << to_string(*bop);
            }
            if (auto right_ref = c.right_ref()) {
                print_ref(*right_ref);
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
                        print_ref(*ref);
                    }
                }
            }
            if (auto m = c.metadata()) {
                print_ref(m->name);
                for (auto& v : m->expr_refs) {
                    cout << " ";
                    print_ref(v);
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
                case rebgn::AbstractOp::MATCH:
                case rebgn::AbstractOp::EXHAUSTIVE_MATCH:
                case rebgn::AbstractOp::CASE:
                case rebgn::AbstractOp::DEFAULT_CASE:
                case rebgn::AbstractOp::ELSE:
                case rebgn::AbstractOp::ELIF:
                case rebgn::AbstractOp::IF:
                case rebgn::AbstractOp::DEFINE_PROGRAM:
                    nest += "  ";
                    break;
                default:
                    break;
            }
        }
    }
}  // namespace rebgn

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
    auto err = rebgn::optimize(*m, *res);
    if (err) {
        cout << err.error<std::string>() << '\n';
        return 1;
    }
    rebgn::print_code(*m);
    if (flags.cfg_output.size()) {
        auto file = futils::file::File::create(flags.cfg_output);
        if (!file) {
            cout << file.error().error<std::string>() << '\n';
            return 1;
        }
        futils::file::FileStream<std::string> fs{*file};
        futils::binary::writer w{fs.get_direct_write_handler(), &fs};
        rebgn::write_cfg(w, *m);
    }
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
