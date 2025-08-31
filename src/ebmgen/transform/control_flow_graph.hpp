/*license*/
#pragma once
#include <vector>
#include <memory>
#include <ebm/extended_binary_module.hpp>
#include "ebmgen/converter.hpp"

namespace ebmgen {
    // Control Flow Graph
    // original_node: reference to original statement
    // next: list of next CFG
    // prev: list of previous CFG
    struct CFG {
        ebm::StatementRef original_node;
        std::optional<ebm::ExpressionRef> condition;
        std::optional<ebm::StatementOp> statement_op;  // for debug
        std::vector<std::shared_ptr<CFG>> next;
        std::vector<std::weak_ptr<CFG>> prev;
    };

    struct CFGTuple {
        std::shared_ptr<CFG> start;
        std::shared_ptr<CFG> end;
        bool brk = false;  // break the flow
    };

    struct DominatorTree {
        std::shared_ptr<CFG> root;
        std::unordered_map<std::shared_ptr<CFG>, std::shared_ptr<CFG>> parent;
    };

    struct CFGResult {
        CFGTuple cfg;
        DominatorTree dom_tree;
    };

    struct CFGList {
        std::vector<std::pair<ebm::StatementRef, CFGResult>> list;
    };

    struct CFGContext {
        TransformContext& tctx;
        std::vector<CFGTuple> loop_stack;
        std::shared_ptr<CFG> end_of_function;
        std::map<std::uint64_t, std::shared_ptr<CFG>> cfg_map;
    };

    expected<CFGList> analyze_control_flow_graph(CFGContext& ctx);
    void write_cfg(futils::binary::writer& w, CFGList& m, TransformContext& ctx);
}  // namespace ebmgen
