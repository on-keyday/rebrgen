#pragma once

#include <memory>
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <error/error.h>

namespace ebmgen {
    using Error = ::futils::error::Error<std::allocator<char>, std::string>;
    // Function to convert brgen AST to ExtendedBinaryModule
    // This will be the main entry point for the conversion logic
    Error convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, ebm::ExtendedBinaryModule& ebm);

}  // namespace ebmgen
