/*license*/
#pragma once
#include "convert.hpp"

namespace rebgn {
    namespace ast = brgen::ast;
    expected<Module> convert(std::shared_ptr<ast::Node>& node) {
        Module mod;
    }
}  // namespace rebgn