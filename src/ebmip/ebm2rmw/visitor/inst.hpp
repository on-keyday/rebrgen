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

        void add_instruction(const ebm::Instruction& instr, std::string str_repr) {
            instructions.push_back(Instruction{
                .instr = instr,
                .str_repr = std::move(str_repr),
            });
        }
    };
}  // namespace ebm2rmw
