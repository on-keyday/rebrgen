#include "converter.hpp"
#include "transform/transform.hpp"
#include "convert.hpp"

namespace ebmgen {

    Error convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, std::vector<std::string>&& file_names, ebm::ExtendedBinaryModule& ebm, Option opt) {
        ConverterContext converter;
        auto s = converter.convert_statement(ast_root);
        if (!s) {
            return s.error();
        }
        TransformContext transform_ctx(converter);
        auto t = transform(transform_ctx, opt.not_remove_unused);
        if (!t) {
            return t.error();
        }
        auto fin = converter.repository().finalize(ebm, std::move(file_names));
        if (!fin) {
            return fin.error();
        }
        return none;
    }

}  // namespace ebmgen
