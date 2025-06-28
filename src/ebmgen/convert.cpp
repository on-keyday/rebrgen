#include "converter.hpp"

namespace ebmgen {

    Error convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, ebm::ExtendedBinaryModule& ebm) {
        Converter converter(ebm);
        return converter.convert(ast_root);
    }

}  // namespace ebmgen
