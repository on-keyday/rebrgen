/*license*/
#pragma once
#include "binary_module.hpp"
#include <unordered_map>
#include <helper/expected.h>
#include <error/error.h>
#include <core/ast/ast.h>
namespace rebgn {
    struct Module {
        std::unordered_map<std::string, std::uint64_t> string_table;
        std::vector<Code> code;
    };

    template <typename T>
    using expected = futils::helper::either::expected<T, futils::error::Error<>>;

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node);

}  // namespace rebgn
