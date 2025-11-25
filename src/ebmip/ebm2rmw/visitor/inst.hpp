/*license*/
#pragma once

#include "ebm/extended_binary_module.hpp"
namespace ebm2rmw {

    struct Instruction {
        ebm::Instruction instr;
        std::string str_repr;
    };

    struct Env {
        std::vector<Instruction> instructions;
    };
}  // namespace ebm2rmw
