/*license*/
#pragma once

#include "cmdline/option/optcontext.h"
#include "json/stringer.h"
namespace ebmcodegen {
    auto flag_description_json(futils::cmdline::option::Context& ctx, const char* lang_name, const char* ui_lang_name, const char* lsp_name, const char* webworker_name, const char* file_ext_name, auto&& web_filtered) {
        futils::json::Stringer<> str;
        str.set_indent("  ");
        auto root_obj = str.object();
        root_obj("lang_name", lang_name);
        root_obj("ui_lang_name", ui_lang_name);
        root_obj("lsp_name", lsp_name);
        root_obj("webworker_name", webworker_name);
        root_obj("file_ext_name", file_ext_name);
        root_obj("flags", [&](auto& s) {
            auto fields = s.array();
            for (auto& opt : ctx.options()) {
                fields([&](auto& s) {
                    auto obj = s.object();
                    obj("name", opt->mainname);
                    obj("help", opt->help);
                    obj("argdesc", opt->argdesc);
                    obj("type", opt->type);
                    obj("web_filtered", web_filtered.contains(opt->mainname));
                });
            }
        });
        root_obj.close();
        return str.out();
    }
}  // namespace ebmcodegen
