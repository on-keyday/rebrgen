#pragma once

#include <memory>
#include <string_view>
#include <functional>
#include <core/ast/ast.h>
#include "common.hpp"

namespace ebmgen {
    // Function to load brgen AST from JSON
    expected<std::shared_ptr<brgen::ast::Node>> load_json(std::string_view input, std::function<void(const char*)> timer_cb);
}  // namespace ebmgen
