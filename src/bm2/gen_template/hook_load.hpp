/*license*/
#pragma once
#include "flags.hpp"
#include <filesystem>
#include "../context.hpp"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <strutil/splits.h>

namespace rebgn {
    bool include_stack(Flags& flags, size_t& line, std::vector<std::filesystem::path>& stack, auto&& file_name, auto&& per_line, bool is_last) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string();
        if (flags.debug || flags.print_hooks) {
            futils::wrap::cerr_wrap() << "try hook via include: " << name << '\n';
        }
        for (auto& parents : stack) {
            std::error_code ec;
            if (std::filesystem::equivalent(parents, hook_file, ec)) {
                futils::wrap::cerr_wrap() << "circular include: " << name << '\n';
                return false;
            }
        }
        stack.push_back(hook_file);
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "loaded hook via include: " << name << '\n';
        for (auto i = 0; i < lines.size(); i++) {
            if (futils::strutil::starts_with(lines[i], "!@include ")) {
                auto split = futils::strutil::split(lines[i], " ", 2);
                if (split.size() == 2) {
                    include_stack(flags, line, stack, split[1], per_line, is_last && i == lines.size() - 1);
                }
                else {
                    futils::wrap::cerr_wrap() << "invalid include: " << lines[i] << '\n';
                }
                continue;
            }
            per_line(line, lines[i], is_last && i == lines.size() - 1);
            line++;
        }
        stack.pop_back();
        return true;
    }

    bool may_write_from_hook(Flags& flags, auto&& file_name, auto&& per_line) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string() + u8".txt";
        if (flags.debug) {
            futils::wrap::cerr_wrap() << "try hook: " << name << '\n';
        }
        else if (flags.print_hooks) {
            futils::wrap::cerr_wrap() << "hook: " << name << '\n';
        }
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "loaded hook: " << name << '\n';
        size_t line = 0;
        for (auto i = 0; i < lines.size(); i++) {
            if (futils::strutil::starts_with(lines[i], "!@include ")) {
                auto split = futils::strutil::split(lines[i], " ", 2);
                if (split.size() == 2) {
                    std::vector<std::filesystem::path> stack;
                    stack.reserve(10);
                    stack.push_back(hook_file);
                    include_stack(flags, line, stack, split[1], per_line, i == lines.size() - 1);
                }
                else {
                    futils::wrap::cerr_wrap() << "invalid include: " << lines[i] << '\n';
                }
                continue;
            }
            per_line(line, lines[i], i == lines.size() - 1);
            line++;
        }
        return true;
    }

    bool may_write_from_hook(Flags& flags, bm2::HookFile hook, auto&& per_line) {
        return may_write_from_hook(flags, to_string(hook), per_line);
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bm2::HookFileSub sub);

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub = bm2::HookFileSub::main);

    void with_hook_comment(bm2::TmpCodeWriter& w, Flags& flags, auto&& concat, futils::view::rvec line, size_t i, bool is_last) {
        if (i == 0) {
            w.writeln("// load hook: ", concat);
        }
        w.writeln(line);
        if (is_last) {
            w.writeln("// end hook: ", concat);
        }
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub, auto sub_sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}_{}", to_string(hook), op_name, to_string(sub), to_string(sub_sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec& line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, bm2::HookFileSub sub = bm2::HookFileSub::main);
    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code);

}  // namespace rebgn
