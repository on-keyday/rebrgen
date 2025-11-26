/*license*/
#pragma once

#include <vector>
#include "ebm/extended_binary_module.hpp"
#include "helper/defer.h"
namespace ebm2rmw {

    struct Instruction {
        ebm::Instruction instr;
        std::string str_repr;
    };

    struct Env {
       private:
        std::vector<Instruction>* instructions;

        std::map<std::uint64_t, std::vector<Instruction>> functions;

       public:
        const std::vector<Instruction>& get_instructions() const {
            return *instructions;
        }

        std::vector<Instruction>& access_instructions() {
            return *instructions;
        }

        void add_instruction(const ebm::Instruction& instr, std::string str_repr) {
            instructions->push_back(Instruction{
                .instr = instr,
                .str_repr = std::move(str_repr),
            });
        }

        bool has_function(ebm::StatementRef func_id) const {
            return functions.find(ebmgen::get_id(func_id)) != functions.end();
        }

        auto new_function(ebm::StatementRef func_id) {
            auto old = instructions;
            instructions = &functions[ebmgen::get_id(func_id)];
            return futils::helper::defer([&, old] {
                instructions = old;
            });
        }
    };
}  // namespace ebm2rmw
