/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>

#include <wrap/cout.h>
#include "convert.hpp"
#include <file/file_stream.h>

#include <fnet/util/base64.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <tool/common/em_main.h>
#endif

namespace rebgn {
    rebgn::expected<std::shared_ptr<brgen::ast::Node>> load_json(std::string_view input);
}
struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::string_view cfg_output;
    std::vector<std::string_view> args;
    bool print_parsed = false;
    bool base64 = false;
    bool print_only_op = false;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&output, "o,output", "output file (if -, write to stdout)", "FILE");
        ctx.VarString<true>(&cfg_output, "c,cfg-output", "control flow graph output file", "FILE");
        ctx.VarBool(&print_parsed, "p,print-instructions", "print converted instructions");
        ctx.VarBool(&base64, "base64", "base64 encode output");
        ctx.VarBool(&print_only_op, "print-only-op", "print only op code(for debug)");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

namespace rebgn {
    void print_code(rebgn::Module& m) {
        for (auto& [metadata, id] : m.metadata_table) {
            cout << "metadata " << metadata << " " << id << '\n';
        }
        for (auto& [str, id] : m.string_table) {
            cout << "string \"" << futils::escape::escape_str<std::string>(str, futils::escape::EscapeFlag::hex) << "\" " << id << '\n';
        }
        for (auto& [id, num] : m.ident_table) {
            cout << "ident " << id->ident << " " << num;
            auto found = m.ident_index_table.find(num);
            if (found != m.ident_index_table.end()) {
                cout << " " << to_string(m.code[found->second].op);
            }
            cout << '\n';
        }

        auto print_ref = [&](rebgn::Varint ref, bool use_index = true) {
            if (ref.value() == 0) {
                cout << " (no ref)";
                return;
            }
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
                cout << " \"" << futils::escape::escape_str<std::string>(found->second, futils::escape::EscapeFlag::hex) << "\"";
                found_ident = true;
            }
            if (auto found = m.metadata_table_rev.find(ref.value()); found != m.metadata_table_rev.end()) {
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
        auto print_type = [&](const rebgn::StorageRef& s) {
            if (s.ref.value() == 0) {
                cout << "(no type)";
                return;
            }
            cout << "type " << s.ref.value();
            auto found = m.get_storage(s);
            if (!found) {
                cout << " (unknown storage)";
                return;
            }
            if (found->length.value() != found->storages.size()) {
                cout << " (length mismatch)";
            }
            cout << " (";
            for (auto& st : found->storages) {
                cout << " " << to_string(st.type);
                if (auto size = st.size()) {
                    if (st.type == rebgn::StorageType::ARRAY) {
                        cout << " length:" << size->value();
                    }
                    else if (st.type == rebgn::StorageType::INT || st.type == rebgn::StorageType::UINT ||
                             st.type == rebgn::StorageType::FLOAT) {
                        cout << " size:";
                        if (size->value() % 8 == 0) {
                            cout << " " << size->value() / 8 << "byte =";
                        }
                        cout << " " << size->value() << "bit";
                    }
                    else if (st.type == rebgn::StorageType::STRUCT_REF) {
                        cout << " size:";
                        if (size->value() == 0) {
                            cout << " (variable)";
                        }
                        else {
                            auto siz = size->value() - 1;
                            if (siz % 8 == 0) {
                                cout << siz / 8 << "byte =";
                            }
                            cout << " " << siz << "bit";
                        }
                    }
                    else if (st.type == rebgn::StorageType::VARIANT) {
                        cout << " alternative:" << size->value();
                    }
                }
                if (auto ref = st.ref()) {
                    print_ref(*ref);
                }
            }
            cout << " )";
        };
        for (auto& t : m.storage_key_table_rev) {
            print_type(StorageRef{.ref = rebgn::Varint(t.first)});
            cout << '\n';
        }
        std::string nest;
        for (auto& c : m.code) {
            switch (c.op) {
                case rebgn::AbstractOp::END_ENUM:
                case rebgn::AbstractOp::END_FORMAT:
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
                case rebgn::AbstractOp::END_STATE:
                case rebgn::AbstractOp::END_BIT_FIELD:
                case rebgn::AbstractOp::END_PROPERTY:
                case rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION:
                case rebgn::AbstractOp::END_DECODE_PACKED_OPERATION:
                case rebgn::AbstractOp::END_ENCODE_SUB_RANGE:
                case rebgn::AbstractOp::END_DECODE_SUB_RANGE:
                case rebgn::AbstractOp::END_FALLBACK:
                case rebgn::AbstractOp::END_COND_BLOCK:
                    nest.resize(nest.size() - 2);
                    break;
                default:
                    break;
            }

            cout << nest << to_string(c.op);
            if (auto uop = c.uop()) {
                cout << " " << to_string(*uop);
            }
            if (auto belong = c.belong()) {
                print_ref(*belong);
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
            if (auto m = c.merge_mode()) {
                cout << " " << to_string(*m);
            }
            if (auto m = c.packed_op_type()) {
                cout << " " << to_string(*m);
            }
            if (auto m = c.check_at()) {
                cout << " " << to_string(*m);
            }
            if (auto m = c.func_type()) {
                cout << " " << to_string(*m);
            }
            if (auto length = c.array_length()) {
                cout << " " << length->value() << " elements";
            }
            if (auto bit_size = c.bit_size()) {
                cout << " " << bit_size->value() << "bit";
            }
            if (auto bit_plus_one = c.bit_size_plus()) {
                if (bit_plus_one->value() == 0) {
                    cout << " (variable)";
                }
                else {
                    cout << " " << bit_plus_one->value() - 1 << "bit";
                }
            }

            if (auto s = c.type()) {
                cout << " ";
                print_type(*s);
            }
            if (auto s = c.from_type()) {
                cout << " ";
                print_type(*s);
            }
            if (auto s = c.cast_type()) {
                cout << " " << to_string(*s);
            }
            if (auto r = c.reserve_type()) {
                cout << " " << to_string(*r);
            }
            if (auto s = c.sub_range_type()) {
                cout << " " << to_string(*s);
            }
            if (auto e = c.endian()) {
                cout << " " << to_string(e->endian());
                cout << " " << (e->sign() ? "signed" : "unsigned");
                if (e->dynamic_ref.value() != 0) {
                    print_ref(e->dynamic_ref);
                }
            }
            if (auto m = c.metadata()) {
                print_ref(m->name);
                for (auto& v : m->refs) {
                    cout << " ";
                    print_ref(v);
                }
            }
            if (auto param = c.param()) {
                cout << " (";
                bool first = true;
                for (auto& p : param->refs) {
                    if (first) {
                        first = false;
                    }
                    else {
                        cout << ",";
                    }
                    print_ref(p);
                }
                cout << " )";
            }
            if (auto phi_param = c.phi_params()) {
                cout << " (";
                bool first = true;
                for (auto& [k, v] : phi_param->params) {
                    if (first) {
                        first = false;
                    }
                    else {
                        cout << ",";
                    }
                    print_ref(k);
                    cout << ":";
                    print_ref(v);
                }
                cout << " )";
            }
            if (auto enc = c.encode_flags()) {
                cout << " (";
                size_t i = 0;
                if (enc->has_seek()) {
                    if (i) {
                        cout << ",";
                    }
                    cout << "seek";
                    i++;
                }
                cout << " )";
            }
            if (auto dec = c.decode_flags()) {
                cout << " (";
                size_t i = 0;
                if (dec->has_eof()) {
                    cout << "eof";
                    i++;
                }
                if (dec->has_peek()) {
                    if (i) {
                        cout << ",";
                    }
                    cout << "peek";
                    i++;
                }
                if (dec->has_seek()) {
                    if (i) {
                        cout << ",";
                    }
                    cout << "seek";
                    i++;
                }
                if (dec->has_remain_bytes()) {
                    if (i) {
                        cout << ",";
                    }
                    cout << "remain_bytes";
                    i++;
                }
                cout << " )";
            }
            if (auto fb = c.fallback()) {
                print_ref(*fb);
            }
            cout << '\n';
            switch (c.op) {
                case rebgn::AbstractOp::DEFINE_ENUM:
                case rebgn::AbstractOp::DEFINE_FORMAT:
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
                case rebgn::AbstractOp::DEFINE_STATE:
                case rebgn::AbstractOp::DEFINE_BIT_FIELD:
                case rebgn::AbstractOp::DEFINE_PROPERTY:
                case rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION:
                case rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION:
                case rebgn::AbstractOp::BEGIN_ENCODE_SUB_RANGE:
                case rebgn::AbstractOp::BEGIN_DECODE_SUB_RANGE:
                case rebgn::AbstractOp::DEFINE_FALLBACK:
                case rebgn::AbstractOp::BEGIN_COND_BLOCK:
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
        cerr << res.error().error<std::string>() << '\n';
        return 1;
    }
    auto m = rebgn::convert(*res);
    if (!m) {
        cerr << m.error().error<std::string>() << '\n';
        return 1;
    }
    auto err = rebgn::transform(*m, *res);
    if (err) {
        cerr << err.error<std::string>() << '\n';
        return 1;
    }
    if (flags.print_parsed) {
        if (flags.print_only_op) {
            for (auto& c : m->code) {
                cout << to_string(c.op) << '\n';
            }
        }
        else {
            rebgn::print_code(*m);
        }
    }
    if (flags.cfg_output.size()) {
        auto file = futils::file::File::create(flags.cfg_output);
        if (!file) {
            cerr << file.error().error<std::string>() << '\n';
            return 1;
        }
        futils::file::FileStream<std::string> fs{*file};
        futils::binary::writer w{fs.get_direct_write_handler(), &fs};
        rebgn::write_cfg(w, *m);
    }
    if (flags.output.size()) {
        rebgn::Error err;
        futils::file::FileError fserr;

        auto get_fs_then = [&](auto&& then) {
            if (flags.output == "-") {
                futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
                futils::binary::writer w{fs.get_write_handler(), &fs};
                then(w, fs);
            }
            else {
                auto file = futils::file::File::create(flags.output);
                if (!file) {
                    err = file.error();
                    fserr = file.error();
                    return;
                }
                futils::file::FileStream<std::string> fs{*file};
                futils::binary::writer w{fs.get_write_handler(), &fs};
                then(w, fs);
            }
        };
        if (flags.base64) {
            std::string buffer;
            futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            err = rebgn::save(*m, w);
            if (err) {
                cerr << err.error<std::string>() << '\n';
                return 1;
            }
            std::string out;
            if (!futils::base64::encode(buffer, out)) {
                cerr << "base64 encode failed\n";
                return 1;
            }
            out.push_back('\n');  // for compatibility with text editors
            get_fs_then([&](auto& w, auto& fs) {
                w.write(out);
                fserr = fs.error;
            });
        }
        else {
            get_fs_then([&](auto& w, auto& fs) {
                err = rebgn::save(*m, w);
                fserr = fs.error;
            });
        }
        if (err) {
            if (fserr.err_code != 0) {
                cerr << fserr.error<std::string>() << '\n';
            }
            cerr << err.error<std::string>() << '\n';
            return 1;
        }
    }
    return 0;
}

int bmgen_main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { 
            if(err) {
                cerr << str;
            }
            else{
            cout << str;
         } },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}

#ifdef __EMSCRIPTEN__
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, bmgen_main);
}
#else
int main(int argc, char** argv) {
    return bmgen_main(argc, argv);
}
#endif
