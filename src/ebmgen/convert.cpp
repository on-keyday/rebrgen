#include "converter.hpp"

namespace ebmgen {

    Error convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, ebm::ExtendedBinaryModule& ebm) {
        ConverterContext converter;
        auto s = converter.convert_statement(ast_root);
        if (!s) {
            return s.error();
        }
        auto fin = converter.finalize(ebm);
        if (!fin) {
            return fin.error();
        }
        return none;
    }

}  // namespace ebmgen
