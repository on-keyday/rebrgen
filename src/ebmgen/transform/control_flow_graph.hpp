/*license*/
#pragma once
#include <vector>
#include <memory>
#include <ebm/extended_binary_module.hpp>
#include "ebmgen/converter.hpp"

namespace ebmgen {
    // Control Flow Graph
    // basic_block: list of Module.code index
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
    };

    struct CFGList {
        std::vector<std::pair<ebm::StatementRef, CFGTuple>> list;
    };

    struct CFGContext {
        TransformContext& tctx;
        std::vector<CFGTuple> loop_stack;
    };

    expected<CFGList> analyze_control_flow_graph(CFGContext& ctx);
    void write_cfg(futils::binary::writer& w, CFGList& m, TransformContext& ctx);
}  // namespace ebmgen
