/*license*/
#include <cstdint>
#include <variant>
#include "../codegen.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "inst.hpp"

namespace ebm2rmw {
    struct Value {
        std::uint64_t int_value = 0;
    };
    struct RuntimeEnv {
        std::shared_ptr<Value> self;
        std::map<std::uint64_t, std::shared_ptr<Value>> heap;
        std::vector<std::shared_ptr<Value>> stack;
        ebmgen::expected<void> interpret(BaseVisitor& ctx) {
            auto& env = ctx.env;
            size_t ip = 0;
            while (ip < env.instructions.size()) {
                auto& instr = env.instructions[ip];
                switch (instr.instr.op) {
                    case ebm::OpCode::HALT:
                        return {};
                    case ebm::OpCode::PUSH_IMM_INT: {
                        auto val = std::make_shared<Value>();
                        val->int_value = instr.instr.value()->value();
                        stack.push_back(val);
                        break;
                    }
                    case ebm::OpCode::LOAD_SELF: {
                        stack.push_back(self);
                        break;
                    }
                    case ebm::OpCode::LOAD_LOCAL: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in LOAD_LOCAL");
                        }
                        auto found = heap.find(reg->index.id.value());
                        if (found == heap.end()) {
                            return ebmgen::unexpect_error("undefined local variable");
                        }
                        stack.push_back(found->second);
                        break;
                    }
                    default:
                        return ebmgen::unexpect_error("unsupported opcode in interpreter: {}({:x})", to_string(instr.instr.op), static_cast<std::uint32_t>(instr.instr.op));
                }
                ip++;
            }
            return {};
        }
    };

}  // namespace ebm2rmw