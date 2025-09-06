/*license*/
#pragma once
#include <vector>
#include <memory>
#include <ebm/extended_binary_module.hpp>
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"

namespace ebmgen {

    struct CFG;
    struct CFGTuple {
        std::shared_ptr<CFG> start;
        std::shared_ptr<CFG> end;
        bool brk = false;  // break the flow
    };

    struct CFGExpression {
        std::weak_ptr<CFGExpression> parent;
        std::string_view relation_name;
        ebm::ExpressionRef original_node;
        std::vector<std::shared_ptr<CFGExpression>> children;
        std::optional<CFGTuple> related_cfg;
    };

    // Control Flow Graph
    // original_node: reference to original statement
    // next: list of next CFG
    // prev: list of previous CFG
    struct CFG {
        ebm::StatementRef original_node;
        std::shared_ptr<CFGExpression> condition;
        std::optional<ebm::StatementOp> statement_op;  // for debug
        std::vector<std::shared_ptr<CFG>> next;
        std::vector<std::weak_ptr<CFG>> prev;
        // for lowered statement representation
        std::vector<CFGTuple> lowered;
    };

    struct DominatorTree {
        std::vector<std::shared_ptr<CFG>> roots;
        std::unordered_map<std::shared_ptr<CFG>, std::shared_ptr<CFG>> parent;
    };

    struct CFGResult {
        CFGTuple cfg;
        DominatorTree dom_tree;
    };

    struct CFGList {
        std::map<std::uint64_t, CFGResult> list;
    };

    struct CFGContext {
        TransformContext& tctx;
        std::vector<CFGTuple> loop_stack;
        std::shared_ptr<CFG> end_of_function;
        std::map<std::uint64_t, std::shared_ptr<CFG>> cfg_map;
    };

    expected<CFGList> analyze_control_flow_graph(CFGContext& ctx);
    void write_cfg(futils::binary::writer& w, const CFGList& m, const MappingTable& ctx);
}  // namespace ebmgen
