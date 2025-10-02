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
        size_t current_index = all_stmts.size();
        for (size_t i = 0; i < current_index; i++) {
            auto& s = all_stmts[i];
            if (auto prop = s.body.property_decl()) {
                if (prop->merge_mode == ebm::MergeMode::STRICT_TYPE) {
                    auto copy = *prop;  // avoid effect of memory relocation of adding object
                    prop = &copy;
                    ebm::FunctionDecl getter, setter;
                    getter.name = prop->name;
                    setter.name = prop->name;
                    getter.parent_format = prop->parent_format;
                    setter.parent_format = prop->parent_format;
                    // getter return value
                    {
                        ebm::TypeBody ptr_type;
                        ptr_type.kind = ebm::TypeKind::PTR;
                        ptr_type.pointee_type(prop->property_type);
                        EBMA_ADD_TYPE(ret_type, std::move(ptr_type));
                        getter.return_type = ret_type;
                    }
                    // setter return value and arguments
                    EBM_DEFINE_ANONYMOUS_VARIABLE(arg, prop->property_type, {});
                    {
                        ebm::TypeBody set_return;
                        set_return.kind = ebm::TypeKind::PROPERTY_SETTER_RETURN;
                        EBMA_ADD_TYPE(ret_type, std::move(set_return));
                        setter.return_type = ret_type;
                        append(setter.params, arg_def);
                    }
                    // getter match
                    {
                        ebm::MatchStatement m;
                        m.target = *prop->cond();
                        EBM_DEFAULT_VALUE(default_, getter.return_type);  // nullptr
                        EBM_RETURN(default_return, default_);
                        for (auto& b : prop->members.container) {
                            MAYBE(stmt, ctx.repository().get_statement(b));
                            MAYBE(member, stmt.body.property_member_decl());
                            ebm::MatchBranch br;
                            br.condition = make_condition(member.condition);
                            if (is_nil(member.field)) {
                                br.body = default_return;
                            }
                            else {
                                EBM_IDENTIFIER(id, member.field, prop->property_type);
                                EBM_ADDRESSOF(addr, getter.return_type, id);
                                EBM_RETURN(ret, addr);
                                br.body = ret;
                            }
                            ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_BRANCH};
                            body.match_branch(std::move(br));
                            EBMA_ADD_STATEMENT(s, std::move(body));
                            append(m.branches, s);
                        }
                        MAYBE_VOID(getter_lowered, ctx.get_statement_converter().derive_match_lowered_if(m, false));
                        ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_STATEMENT};
                        body.match_statement(std::move(m));
                        EBMA_ADD_STATEMENT(match, std::move(body));
                        ebm::Block getter_block;
                        append(getter_block, match);
                        append(getter_block, default_return);
                        EBM_BLOCK(gblock, std::move(getter_block));
                        getter.body = gblock;
                    }
                    // setter match
                    {
                        ebm::MatchStatement m;
                        m.target = *prop->cond();
                        EBM_SETTER_STATUS(default_status, setter.return_type, ebm::SetterStatus::FAILED);
                        EBM_RETURN(default_return, default_status);
                        for (auto& b : prop->members.container) {
                            MAYBE(stmt, ctx.repository().get_statement(b));
                            MAYBE(member, stmt.body.property_member_decl());
                            ebm::MatchBranch br;
                            br.condition = make_condition(member.condition);
                            if (is_nil(member.field)) {
                                br.body = default_return;
                            }
                            else {
                                EBM_IDENTIFIER(id, member.field, prop->property_type);
                                EBM_ASSIGNMENT(assign, id, arg);
                                ebm::Block block;
                                append(block, assign);
                                EBM_SETTER_STATUS(success_status, setter.return_type, ebm::SetterStatus::SUCCESS);
                                EBM_RETURN(success_return, success_status);
                                append(block, success_return);
                                EBM_BLOCK(b, std::move(block));
                                br.body = b;
                            }
                            ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_BRANCH};
                            body.match_branch(std::move(br));
                            EBMA_ADD_STATEMENT(s, std::move(body));
                            append(m.branches, s);
                        }
                        MAYBE_VOID(setter_lowered, ctx.get_statement_converter().derive_match_lowered_if(m, false));
                        ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_STATEMENT};
                        body.match_statement(std::move(m));
                        EBMA_ADD_STATEMENT(match, std::move(body));
                        ebm::Block setter_block;
                        append(setter_block, match);
                        append(setter_block, default_return);
                        EBM_BLOCK(sblock, std::move(setter_block));
                        setter.body = sblock;
                    }
                    ebm::StatementBody func{.kind = ebm::StatementKind::FUNCTION_DECL};
                    func.func_decl(std::move(getter));
                    EBMA_ADD_STATEMENT(getter_ref, std::move(func));
                    func.func_decl(std::move(setter));
                    EBMA_ADD_STATEMENT(setter_ref, std::move(func));
                    prop = all_stmts[i].body.property_decl();  // refresh
                    prop->getter_function = ebm::LoweredStatementRef{getter_ref};
                    prop->setter_function = ebm::LoweredStatementRef{setter_ref};
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
        MAYBE_VOID(prop_setter_getter, derive_property_setter_getter(ctx));
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
