/*license*/
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <variant>
#include "../codegen.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "inst.hpp"
#include "ebmcodegen/stub/ops_macros.hpp"

namespace ebm2rmw {
    struct Value {
        std::variant<std::uint64_t, std::string, std::uint8_t*, std::shared_ptr<Value>, std::unordered_map<std::string, std::shared_ptr<Value>>> value;

        void unref() {
            if (auto ptr = std::get_if<std::shared_ptr<Value>>(&value)) {
                value = ptr->get()->value;
            }
            if (auto elem = std::get_if<std::uint8_t*>(&value)) {
                value = **elem;
            }
        }
    };
    struct RuntimeEnv {
        futils::view::rvec input;
        size_t input_pos = 0;

        std::shared_ptr<Value> self;
        std::map<std::uint64_t, std::shared_ptr<Value>> heap;
        std::vector<Value> stack;

        ebmgen::expected<void> interpret(BaseVisitor& ctx) {
            auto& env = ctx.env;
            size_t ip = 0;
            while (ip < env.instructions.size()) {
                auto& instr = env.instructions[ip];
                if (auto bop = OpCode_to_BinaryOp(instr.instr.op)) {
                    if (stack.size() < 2) {
                        return ebmgen::unexpect_error("stack underflow on binary operation");
                    }
                    auto right = stack.back();
                    stack.pop_back();
                    auto left = stack.back();
                    stack.pop_back();
                    auto r = [&]() -> ebmgen::expected<void> {
                        APPLY_BINARY_OP(*bop, [&](auto&& op) -> ebmgen::expected<void> {
                            right.unref();
                            left.unref();
                            if (!std::holds_alternative<std::uint64_t>(left.value) || !std::holds_alternative<std::uint64_t>(right.value)) {
                                return ebmgen::unexpect_error("binary operation operands must be integers");
                            }
                            auto lval = std::get<std::uint64_t>(left.value);
                            auto rval = std::get<std::uint64_t>(right.value);
                            auto result = op(lval, rval);
                            stack.push_back(Value{result});
                            return ebmgen::expected<void>{};
                        });
                        return ebmgen::unexpect_error("unsupported binary operation in interpreter: {}", to_string(*bop));
                    }();
                    MAYBE_VOID(r_, r);
                    ip++;
                    continue;
                }
                if (auto uop = OpCode_to_UnaryOp(instr.instr.op)) {
                    if (stack.empty()) {
                        return ebmgen::unexpect_error("stack underflow on unary operation");
                    }
                    auto operand = stack.back();
                    stack.pop_back();
                    auto r = [&]() -> ebmgen::expected<void> {
                        APPLY_UNARY_OP(*uop, [&](auto&& op) -> ebmgen::expected<void> {
                            operand.unref();
                            if (!std::holds_alternative<std::uint64_t>(operand.value)) {
                                return ebmgen::unexpect_error("unary operation operand must be an integer");
                            }
                            auto val = std::get<std::uint64_t>(operand.value);
                            auto result = op(val);
                            stack.push_back(Value{result});
                            return ebmgen::expected<void>{};
                        });
                        return ebmgen::unexpect_error("unsupported unary operation in interpreter: {}", to_string(*uop));
                    }();
                    MAYBE_VOID(r_, r);
                    ip++;
                    continue;
                }
                switch (instr.instr.op) {
                    case ebm::OpCode::HALT:
                        return {};
                    case ebm::OpCode::PUSH_IMM_INT: {
                        auto val = instr.instr.value();
                        stack.push_back(Value{val->value()});
                        break;
                    }
                    case ebm::OpCode::NEW_BYTES: {
                        // For simplicity, we skip actual byte array creation
                        MAYBE(imm, instr.instr.imm());
                        if (auto v = imm.size()) {
                            std::string buf;
                            buf.resize(v->value());
                            stack.push_back(Value{std::move(buf)});
                        }
                        else {
                            stack.push_back(Value{std::string()});
                        }
                        break;
                    }
                    case ebm::OpCode::LOAD_SELF: {
                        stack.push_back(Value{self});
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
                        stack.push_back(Value{found->second});
                        break;
                    }
                    case ebm::OpCode::STORE_LOCAL: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in STORE_LOCAL");
                        }
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on STORE_LOCAL");
                        }
                        auto val = stack.back();
                        stack.pop_back();
                        auto& slot = heap[reg->index.id.value()];
                        if (!slot) {
                            slot = std::make_shared<Value>(std::move(val));
                        }
                        else {
                            *slot = std::move(val);
                        }
                        break;
                    }
                    case ebm::OpCode::ASSERT: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on ASSERT");
                        }
                        auto cond = stack.back();
                        stack.pop_back();
                        cond.unref();
                        if (!std::holds_alternative<std::uint64_t>(cond.value)) {
                            return ebmgen::unexpect_error("ASSERT condition is not an integer");
                        }
                        auto cond_val = std::get<std::uint64_t>(cond.value);
                        if (cond_val == 0) {
                            return ebmgen::unexpect_error("ASSERT failed");
                        }
                        break;
                    }
                    case ebm::OpCode::ARRAY_GET: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on ARRAY_GET");
                        }
                        auto index_val = stack.back();
                        stack.pop_back();
                        auto base_val = stack.back();
                        stack.pop_back();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto base_ptr = std::get<std::shared_ptr<Value>>(base_val.value);
                        if (!std::holds_alternative<std::string>(base_ptr->value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto& arr = std::get<std::string>(base_ptr->value);
                        if (!std::holds_alternative<std::uint64_t>(index_val.value)) {
                            return ebmgen::unexpect_error("index value is not an integer");
                        }
                        auto index = std::get<std::uint64_t>(index_val.value);
                        if (index >= arr.size()) {
                            return ebmgen::unexpect_error("array index out of bounds");
                        }
                        std::uint8_t* elem = (std::uint8_t*)&arr[index];
                        stack.push_back(Value{elem});
                        break;
                    }
                    case ebm::OpCode::READ_BYTES: {
                        MAYBE(imm, instr.instr.imm());
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on READ_BYTES");
                        }
                        std::size_t size = 0;
                        if (auto static_size = imm.size()) {
                            size = static_size->value();
                        }
                        else {
                            auto size_expr = stack.back();
                            stack.pop_back();
                            size_expr.unref();
                            if (!std::holds_alternative<std::uint64_t>(size_expr.value)) {
                                return ebmgen::unexpect_error("READ_BYTES size is not an integer");
                            }
                            size = static_cast<std::size_t>(std::get<std::uint64_t>(size_expr.value));
                            if (stack.empty()) {
                                return ebmgen::unexpect_error("stack underflow on READ_BYTES target");
                            }
                        }
                        auto target = stack.back();
                        stack.pop_back();
                        target.unref();
                        if (!std::holds_alternative<std::string>(target.value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not a byte array");
                        }
                        auto& arr = std::get<std::string>(target.value);
                        arr.resize(size);  // Simulate reading bytes by resizing
                        auto read = input.substr(input_pos, size);
                        if (read.size() < size) {
                            return ebmgen::unexpect_error("not enough input data for READ_BYTES");
                        }
                        std::copy(read.begin(), read.end(), arr.begin());
                        input_pos += size;
                        stack.push_back(Value{std::move(arr)});
                        break;
                    }
                    default:
                        return ebmgen::unexpect_error("unsupported opcode in interpreter: {}(0x{:x})", to_string(instr.instr.op, true), static_cast<std::uint32_t>(instr.instr.op));
                }
                ip++;
            }
            return {};
        }
    };

}  // namespace ebm2rmw