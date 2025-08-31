/*license*/
#include "control_flow_graph.hpp"
#include <memory>
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include <optional>
#include <set>
#include <unordered_map>

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
        std::sort(cfg->next.begin(), cfg->next.end(), [](const auto& a, const auto& b) {
            return a < b;
        });
        auto result1 = std::unique(cfg->next.begin(), cfg->next.end());
        cfg->next.erase(result1, cfg->next.end());
        std::sort(cfg->prev.begin(), cfg->prev.end(), [](const auto& a, const auto& b) {
            return a.lock() < b.lock();
        });
        auto result2 = std::unique(cfg->prev.begin(), cfg->prev.end(), [](const auto& a, const auto& b) {
            return a.lock() == b.lock();
        });
        cfg->prev.erase(result2, cfg->prev.end());
        if (cfg->prev.size() && cfg->next.size() == 1 && cfg->original_node.id.value() == 0) {
            auto rem = cfg;
            for (auto& prev : rem->prev) {
                if (auto p = prev.lock()) {
                    for (auto& n : p->next) {
                        if (n == cfg) {
                            n = rem->next[0];
                        }
                    }
                }
            }
            std::erase_if(rem->next[0]->prev, [&](auto& a) {
                return a.lock() == rem;
            });
            rem->next[0]->prev.insert(rem->next[0]->prev.end(), rem->prev.begin(), rem->prev.end());
            return optimize_cfg_node(rem->next[0], visited);
        }
        visited.insert(cfg);
        for (auto& n : cfg->next) {
            n = optimize_cfg_node(n, visited);
        }
        return cfg;
    }

    expected<DominatorTree> analyze_dominators(CFGTuple& root, std::set<std::shared_ptr<CFG>>& all_node) {
        DominatorTree dom_tree;
        dom_tree.root = root.start;
        std::map<std::shared_ptr<CFG>, std::set<std::shared_ptr<CFG>>> doms;
        for (auto& n : all_node) {
            if (n == root.start) {
                doms[n].insert(n);
            }
            else {
                doms[n] = all_node;
            }
        }
        bool changed = true;
        while (changed) {
            changed = false;
            // 開始ノード以外の全てのノードnについてループ
            for (auto& n : all_node) {
                if (n == root.start) continue;

                // ===== nの支配集合を再計算 =====

                // 1. nの先行ノード(prev)全ての支配集合の共通部分(intersection)を求める
                std::optional<std::set<std::shared_ptr<CFG>>> intersection;

                for (auto& weak_pred : n->prev) {
                    if (auto pred = weak_pred.lock()) {  // weak_ptrからshared_ptrを取得
                        if (!intersection.has_value()) {
                            // 最初の先行ノード
                            intersection = doms.at(pred);
                        }
                        else {
                            // 2つ目以降は、現在の共通集合と積集合を取る
                            // (C++では std::set_intersection などを利用)
                            std::set<std::shared_ptr<CFG>> temp_result;
                            std::set_intersection(
                                intersection->begin(), intersection->end(),
                                doms.at(pred).begin(), doms.at(pred).end(),
                                std::inserter(temp_result, temp_result.begin()));
                            intersection = std::move(temp_result);
                        }
                    }
                }

                // 2. 求めた共通部分に、ノードn自身を加える
                std::set<std::shared_ptr<CFG>> new_doms_for_n;
                if (intersection.has_value()) {
                    new_doms_for_n = *intersection;
                }
                new_doms_for_n.insert(n);

                // 3. もし支配集合が変化していたら、更新してフラグを立てる
                if (new_doms_for_n != doms.at(n)) {
                    doms[n] = new_doms_for_n;
                    changed = true;
                }
            }
        }

        // 全てのノードnについてループ
        for (const auto& n : all_node) {
            if (n == root.start) continue;

            // nの支配ノードからn自身を除いた集合Sを用意
            const auto& n_doms = doms.at(n);

            std::shared_ptr<CFG> best_candidate = nullptr;
            size_t max_size = 0;

            // 集合Sの中で、|doms(d)|が最大になるdを探す
            for (const auto& d : n_doms) {
                if (d == n) continue;  // n自身は除外

                const size_t candidate_size = doms.at(d).size();
                if (candidate_size > max_size) {
                    max_size = candidate_size;
                    best_candidate = d;
                }
            }

            if (best_candidate) {
                dom_tree.parent[n] = best_candidate;
            }
        }

        // Compute dominator tree
        return dom_tree;
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
            MAYBE(dom_tree, analyze_dominators(cfg, visited));
            cfg_list.list.emplace_back(stmt.id,
                                       CFGResult{
                                           .cfg = std::move(cfg),
                                           .dom_tree = std::move(dom_tree),
                                       });
        }
        return cfg_list;
    }

    void write_cfg(futils::binary::writer& w, CFGList& m, TransformContext& ctx) {
        w.write("digraph G {\n");
        std::uint64_t id = 0;
        std::map<std::shared_ptr<CFG>, std::uint64_t> node_id;
        std::set<std::pair<std::shared_ptr<CFG>, std::shared_ptr<CFG>>> dominate_edges;
        auto write_node = [&](auto&& write, DominatorTree& dom_tree, std::optional<std::string> name, std::shared_ptr<CFG>& cfg) -> void {
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
                write(write, dom_tree, std::nullopt, n);
                w.write(std::format("  {} -> {}", node_id[cfg], node_id[n]));
                if (n->condition) {
                    auto cond_expr = ctx.expression_repository().get(*n->condition);
                    w.write(std::format("[label=\"{}:{}\"]", node_id[cfg], node_id[n], cond_expr ? to_string(cond_expr->body.kind) : "<unknown expr>", n->condition->id.value()));
                }
                w.write(";\n");
                auto parent = dom_tree.parent[n];
                auto dom_id = node_id[parent];
                if (dominate_edges.contains({parent, n})) {
                    continue;
                }
                dominate_edges.insert({parent, n});
                w.write(std::format("  {} -> {} [style=dotted,label=\"dominates\"];\n", dom_id, node_id[n]));
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
            write_node(write_node, cfg.second.dom_tree, name, cfg.second.cfg.start);
        }
        w.write("}\n");
    }

}  // namespace ebmgen
