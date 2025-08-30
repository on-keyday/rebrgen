/*license*/
#include "control_flow_graph.hpp"
#include <memory>
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include <optional>
#include <set>

namespace ebmgen {

    expected<CFGTuple> analyze_ref(CFGContext& tctx, ebm::StatementRef ref) {
        auto root = std::make_shared<CFG>();
        root->original_node = ref;
        auto current = root;
        auto link = [](auto& from, auto& to) {
            from->next.push_back(to);
            to->prev.push_back(from);
        };
        auto& ctx = tctx.tctx.context();
        MAYBE(stmt, ctx.repository().get_statement(ref));
        root->statement_op = stmt.body.kind;
        tctx.cfg_map[ref.id.value()] = root;
        bool brk = false;
        if (auto block = stmt.body.block()) {
            auto join = std::make_shared<CFG>();
            for (auto& ref : block->container) {
                MAYBE(child, analyze_ref(tctx, ref));
                link(current, child.start);
                if (!child.brk) {
                    link(child.end, join);
                    current = std::move(join);
                    join = std::make_shared<CFG>();
                    continue;
                }
                current = child.end;
                brk = true;
                break;
            }
        }
        else if (auto if_stmt = stmt.body.if_statement()) {
            MAYBE(then_block, analyze_ref(tctx, if_stmt->then_block));
            then_block.start->condition = if_stmt->condition;
            auto join = std::make_shared<CFG>();
            link(current, then_block.start);
            if (!then_block.brk) {
                link(then_block.end, join);
            }
            if (if_stmt->else_block.id.value()) {
                MAYBE(else_block, analyze_ref(tctx, if_stmt->else_block));
                link(current, else_block.start);
                if (!else_block.brk) {
                    link(else_block.end, join);
                }
            }
            else {
                link(current, join);
            }
            current = std::move(join);
        }
        else if (auto loop_ = stmt.body.loop()) {
            auto join = std::make_shared<CFG>();
            tctx.loop_stack.push_back(CFGTuple{current, join});
            MAYBE(body, analyze_ref(tctx, loop_->body));
            tctx.loop_stack.pop_back();
            link(current, body.start);
            if (loop_->loop_type != ebm::LoopType::INFINITE) {
                link(current, join);
            }
            if (!body.brk) {
                link(body.end, current);
            }
            current = std::move(join);
        }
        else if (auto match_ = stmt.body.match_statement()) {
            auto join = std::make_shared<CFG>();
            for (auto& b : match_->branches.container) {
                MAYBE(branch_stmt, ctx.repository().get_statement(b));
                MAYBE(branch_ptr, branch_stmt.body.match_branch());
                MAYBE(branch, analyze_ref(tctx, branch_ptr.body));
                branch.start->condition = branch_ptr.condition;
                link(current, branch.start);
                if (!branch.brk) {
                    link(branch.end, join);
                }
            }
            link(current, join);
            current = std::move(join);
        }
        else if (auto cont = stmt.body.continue_()) {
            if (tctx.loop_stack.size() == 0) {
                return unexpect_error("Continue outside of loop");
            }
            link(current, tctx.loop_stack.back().start);
            brk = true;
        }
        else if (auto brk_ = stmt.body.break_()) {
            if (tctx.loop_stack.size() == 0) {
                return unexpect_error("Break outside of loop");
            }
            link(current, tctx.loop_stack.back().end);
            brk = true;
        }
        else if (stmt.body.kind == ebm::StatementOp::RETURN ||
                 stmt.body.kind == ebm::StatementOp::ERROR_RETURN ||
                 stmt.body.kind == ebm::StatementOp::ERROR_REPORT) {
            link(current, tctx.end_of_function);
            brk = true;
        }
        return CFGTuple{root, current, brk};
    }

    std::shared_ptr<CFG> optimize_cfg_node(std::shared_ptr<CFG>& cfg, std::set<std::shared_ptr<CFG>>& visited) {
        if (visited.find(cfg) != visited.end()) {
            return cfg;
        }
        if (cfg->prev.size() && cfg->next.size() == 1 && cfg->original_node.id.value() == 0) {
            auto rem = cfg;
            for (auto& prev : cfg->prev) {
                if (auto p = prev.lock()) {
                    for (auto& n : p->next) {
                        if (n == cfg) {
                            n = rem->next[0];
                        }
                    }
                }
            }
            rem->next[0]->prev.insert(rem->next[0]->prev.end(), rem->prev.begin(), rem->prev.end());
            return optimize_cfg_node(rem->next[0], visited);
        }
        visited.insert(cfg);
        for (auto& n : cfg->next) {
            n = optimize_cfg_node(n, visited);
        }
        return cfg;
    }

    expected<CFGList> analyze_control_flow_graph(CFGContext& tctx) {
        CFGList cfg_list;
        for (auto& stmt : tctx.tctx.statement_repository().get_all()) {
            auto fn = stmt.body.func_decl();
            if (!fn) {
                continue;
            }
            tctx.end_of_function = std::make_shared<CFG>();
            MAYBE(cfg, analyze_ref(tctx, fn->body));
            cfg.end->next.push_back(tctx.end_of_function);
            tctx.end_of_function->prev.push_back(cfg.end);
            std::set<std::shared_ptr<CFG>> visited;
            cfg.start = optimize_cfg_node(cfg.start, visited);
            cfg_list.list.emplace_back(stmt.id, std::move(cfg));
        }
        return cfg_list;
    }

    void write_cfg(futils::binary::writer& w, CFGList& m, TransformContext& ctx) {
        w.write("digraph G {\n");
        std::uint64_t id = 0;
        std::map<std::shared_ptr<CFG>, std::uint64_t> node_id;
        std::map<std::uint64_t, std::shared_ptr<CFG>> fn_to_cfg;
        auto write_node = [&](auto&& write, std::optional<std::string> name, std::shared_ptr<CFG>& cfg) -> void {
            if (node_id.find(cfg) != node_id.end()) {
                return;
            }
            node_id[cfg] = id++;
            w.write(std::format("  {} [label=\"", node_id[cfg]));
            if (name) {
                w.write(std::format("fn {}\\n", name.value()));
            }
            auto origin = ctx.statement_repository().get(cfg->original_node);
            if (cfg->next.size() == 0) {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<end>", cfg->original_node.id.value()));
            }
            else {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<phi>", cfg->original_node.id.value()));
            }
            w.write("\"];\n");
            for (auto& n : cfg->next) {
                write(write, std::nullopt, n);
                if (n->condition) {
                    auto cond_expr = ctx.expression_repository().get(*n->condition);
                    w.write(std::format("  {} -> {} [label=\"{}:{}\"];\n", node_id[cfg], node_id[n], cond_expr ? to_string(cond_expr->body.kind) : "<unknown expr>", n->condition->id.value()));
                    continue;
                }
                w.write(std::format("  {} -> {};\n", node_id[cfg], node_id[n]));
            }
        };
        for (auto& cfg : m.list) {
            auto fn = ctx.statement_repository().get(cfg.first);
            std::optional<std::string> name;
            if (fn) {
                if (auto fn_decl = fn->body.func_decl()) {
                    auto ident = ctx.identifier_repository().get(fn_decl->name);
                    if (ident) {
                        name = ident->body.data;
                    }
                }
            }
            write_node(write_node, name, cfg.second.start);
        }
        w.write("}\n");
    }

}  // namespace ebmgen
