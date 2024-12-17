/*license*/
#pragma once
#include <file/file_view.h>
#include <core/ast/file.h>
#include <core/ast/json.h>
#include "common.hpp"

namespace rebgn {
    rebgn::expected<std::shared_ptr<brgen::ast::Node>> load_json(std::string_view input) {
        futils::file::View view;
        if (!view.open(input)) {
            return nullptr;
        }
        auto js = futils::json::parse<futils::json::JSON>(view, true);
        if (js.is_undef()) {
            return unexpect_error("cannot parse json file");
        }
        brgen::ast::AstFile file;
        if (!futils::json::convert_from_json(js, file)) {
            return unexpect_error("cannot convert json file");
        }
        if (!file.ast) {
            return unexpect_error("ast is not found");
        }
        brgen::ast::JSONConverter c;
        auto res = c.decode(*file.ast);
        if (!res) {
            return unexpect_error("cannot decode json file: {}", res.error().locations[0].msg);
        }
        if (!*res) {
            return unexpect_error("cannot decode json file");
        }
        return *res;
    }
}  // namespace rebgn