#include "converter.hpp"
#include "transform/transform.hpp"
#include "convert.hpp"

namespace ebmgen {

    expected<Output> convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, std::vector<std::string>&& file_names, ebm::ExtendedBinaryModule& ebm, Option opt) {
        ConverterContext converter;
        MAYBE(s, converter.convert_statement(ast_root));
        TransformContext transform_ctx(converter);
        MAYBE(t, transform(transform_ctx, opt.not_remove_unused));
        MAYBE_VOID(f, converter.repository().finalize(ebm, std::move(file_names)));
        return Output{
            .control_flow_graph = std::move(t),
        };
    }

}  // namespace ebmgen
