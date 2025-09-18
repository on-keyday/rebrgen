/*license*/
#include "transform.hpp"
#include "../common.hpp"
#include "control_flow_graph.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include <testutil/timer.h>
#include "../convert//helper.hpp"

namespace ebmgen {

    expected<void> derive_property_setter_getter(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_stmts = tctx.statement_repository().get_all();
        for (auto& s : all_stmts) {
            if (auto prop = s.body.property_decl()) {
                if (prop->merge_mode == ebm::MergeMode::STRICT_TYPE) {
                    ebm::FunctionDecl getter, setter;
                    getter.name = prop->name;
                    setter.name = prop->name;
                    getter.parent_format = prop->parent_format;
                    setter.parent_format = prop->parent_format;
                    {
                        ebm::TypeBody ptr_type;
                        ptr_type.kind = ebm::TypeKind::PTR;
                        ptr_type.pointee_type(prop->property_type);
                        EBMA_ADD_TYPE(ret_type, std::move(ptr_type));
                        getter.return_type = ret_type;
                    }
                    EBM_DEFINE_ANONYMOUS_VARIABLE(arg, prop->property_type, {});
                    {
                        ebm::TypeBody set_return;
                        set_return.kind = ebm::TypeKind::PROPERTY_SETTER_RETURN;
                        EBMA_ADD_TYPE(ret_type, std::move(set_return));
                        setter.return_type = ret_type;
                        append(setter.params, arg_def);
                    }
                    ebm::Block getter_block;
                    for (auto& m : prop->members.container) {
                        MAYBE(stmt, ctx.repository().get_statement(m));
                        MAYBE(member, stmt.body.property_member_decl());
                        ebm::IfStatement if_stmt;
                    }
                }
            }
        }
        return {};
    }

    expected<CFGList> transform(TransformContext& ctx, bool debug) {
        // internal CFG used optimization
        {
            CFGContext cfg_ctx{ctx};
            MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
            MAYBE_VOID(bit_io_read, lowered_dynamic_bit_io(cfg_ctx, false));
            MAYBE_VOID(bit_io_write, lowered_dynamic_bit_io(cfg_ctx, true));
        }
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
        if (!debug) {
            MAYBE_VOID(remove_unused, remove_unused_object(ctx));
            ctx.recalculate_id_index_map();
        }
        // final cfg view
        CFGContext cfg_ctx{ctx};
        MAYBE(cfg, analyze_control_flow_graph(cfg_ctx));
        return cfg;
    }
}  // namespace ebmgen
