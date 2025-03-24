/*license*/
#include "hook_load.hpp"

namespace rebgn {

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bm2::HookFileSub sub) {
        auto concat = std::format("{}{}", to_string(hook), to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, bm2::HookFileSub sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code) {
        return may_write_from_hook(flags, hook, [&](size_t i, futils::view::rvec& line, bool is_last) {
            if (i == 0) {
                w.writeln("// load hook: ", to_string(hook));
            }
            if (as_code) {
                if (!futils::strutil::contains(line, "\"") && !futils::strutil::contains(line, "\\")) {
                    w.writeln("w.writeln(\"", line, "\");");
                }
                else {
                    std::string escaped_line;
                    for (auto c : line) {
                        if (c == '"' || c == '\\') {
                            escaped_line.push_back('\\');
                        }
                        escaped_line.push_back(c);
                    }
                    w.writeln("w.writeln(\"", escaped_line, "\");");
                }
            }
            else {
                w.writeln(line);
            }
            if (is_last) {
                w.writeln("// end hook: ", to_string(hook));
            }
        });
    }
}  // namespace rebgn