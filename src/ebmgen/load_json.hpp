#pragma once

#include <memory>
#include <string_view>
#include <functional>
#include <core/ast/ast.h>
#include "common.hpp"

namespace ebmgen {
    // Function to load brgen AST from JSON
    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json(std::string_view input, std::function<void(const char*)> timer_cb);
    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json_file(futils::view::rvec input, std::function<void(const char*)> timer_cb);
    expected<ebm::ExtendedBinaryModule> load_json_ebm(std::string_view input);
}  // namespace ebmgen
