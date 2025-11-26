/*license*/
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <variant>
#include "../codegen.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmcodegen/stub/util.hpp"
#include "ebmgen/common.hpp"
#include "inst.hpp"
#include "ebmcodegen/stub/ops_macros.hpp"

namespace ebm2rmw {
    struct Value {
        std::variant<std::monostate, std::uint64_t, std::string, std::uint8_t*, std::shared_ptr<Value>, std::unordered_map<std::uint64_t, std::shared_ptr<Value>>> value;

        void unref() {
            if (auto ptr = std::get_if<std::shared_ptr<Value>>(&value)) {
                value = ptr->get()->value;
            }
            if (auto elem = std::get_if<std::uint8_t*>(&value)) {
                value = **elem;
            }
        }

        void may_object() {
            // if monotstate, make it an object
            if (std::holds_alternative<std::monostate>(value)) {
                value = std::unordered_map<std::uint64_t, std::shared_ptr<Value>>{};
            }
        }

        void may_bytes(std::size_t size) {
            if (std::holds_alternative<std::monostate>(value)) {
                std::string buf;
                buf.resize(size);
                value = buf;
            }
        }
    };

    struct StackFrame {
        std::shared_ptr<Value> self;
        std::map<std::uint64_t, std::shared_ptr<Value>> params;
        std::map<std::uint64_t, std::shared_ptr<Value>> heap;
        std::vector<Value> stack;
    };

    struct RuntimeEnv {
        futils::view::rvec input;
        size_t input_pos = 0;

        ebmgen::expected<std::shared_ptr<Value>> new_object(InitialContext& ctx, ebm::StatementRef ref) {
            auto obj = std::make_shared<Value>();
            std::unordered_map<std::uint64_t, std::shared_ptr<Value>> c{};
            MAYBE(fields, ctx.get(ref));
            MAYBE(field_list, fields.body.struct_decl());
            auto res = handle_fields(ctx, field_list.fields, true, [&](ebm::StatementRef field_ref, const ebm::Statement& field) -> ebmgen::expected<void> {
                if (field.body.property_decl()) {
                    // skip property fields
                    return {};
                }
                MAYBE(field_body, field.body.field_decl());
                if (ctx.is(ebm::TypeKind::STRUCT, field_body.field_type)) {
                    MAYBE(type_impl, ctx.get(field_body.field_type));
                    MAYBE(struct_, type_impl.body.id());
                    MAYBE(nested_obj, new_object(ctx, struct_));
                    c[get_id(field_ref)] = nested_obj;
                }
                else {
                    c[get_id(field_ref)] = std::make_shared<Value>();
                }
                return {};
            });
            if (!res) {
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            return obj;
        }

        ebmgen::expected<void> interpret(InitialContext& ctx, std::map<std::uint64_t, std::shared_ptr<Value>>& params) {
            size_t ip = 0;
            call_stack.emplace_back();
            auto _end = futils::helper::defer([&] {
                call_stack.pop_back();
            });
            auto& this_ = call_stack.back();
            this_.params = params;
            auto res = interpret_impl(ctx, ip);
            if (!res) {
                if (ip < ctx.config().env.get_instructions().size()) {
                    return ebmgen::unexpect_error("Runtime error at instruction {}: {}:{}:{}", ip, to_string(ctx.config().env.get_instructions()[ip].instr.op), ctx.config().env.get_instructions()[ip].str_repr, res.error().error());
                }
            }
            return res;
        }

       private:
        std::vector<StackFrame> call_stack;

        ebmgen::expected<void> interpret_impl(InitialContext& ctx, size_t& ip) {
            auto& env = ctx.config().env;
            auto& this_ = call_stack.back();
            auto& self = this_.self;
            auto& params = this_.params;
            auto& stack = this_.stack;
            auto& heap = this_.heap;
            while (ip < env.get_instructions().size()) {
                auto& instr = env.get_instructions()[ip];
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
                        val.unref();
                        auto val_ptr = std::make_shared<Value>(std::move(val));
                        heap[reg->index.id.value()] = val_ptr;
                        break;
                    }
                    case ebm::OpCode::STORE_REF: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on STORE_REF");
                        }
                        auto ref = stack.back();
                        stack.pop_back();
                        auto val = stack.back();
                        stack.pop_back();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(ref.value)) {
                            return ebmgen::unexpect_error("STORE_REF target is not a reference");
                        }
                        val.unref();
                        auto ref_ptr = std::get<std::shared_ptr<Value>>(ref.value);
                        *ref_ptr = std::move(val);
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
                        std::size_t size = 0;
                        if (auto static_size = imm.size()) {
                            size = static_size->value();
                        }
                        else {
                            if (stack.empty()) {
                                return ebmgen::unexpect_error("stack underflow on READ_BYTES");
                            }
                            auto size_expr = stack.back();
                            stack.pop_back();
                            size_expr.unref();
                            if (!std::holds_alternative<std::uint64_t>(size_expr.value)) {
                                return ebmgen::unexpect_error("READ_BYTES size is not an integer");
                            }
                            size = static_cast<std::size_t>(std::get<std::uint64_t>(size_expr.value));
                        }
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on READ_BYTES target");
                        }
                        auto target = stack.back();
                        stack.pop_back();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(target.value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not an object");
                        }
                        auto& arr = std::get<std::shared_ptr<Value>>(target.value);
                        arr->may_bytes(size);
                        if (!std::holds_alternative<std::string>(arr->value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not a byte array");
                        }
                        auto& byte_array = std::get<std::string>(arr->value);
                        byte_array.resize(size);  // Simulate reading bytes by resizing
                        auto read = input.substr(input_pos, size);
                        if (read.size() < size) {
                            return ebmgen::unexpect_error("expected {} bytes, but only {} bytes available in input", size, read.size());
                        }
                        std::copy(read.begin(), read.end(), byte_array.begin());
                        input_pos += size;
                        stack.push_back(Value{std::move(arr)});
                        break;
                    }
                    case ebm::OpCode::LOAD_MEMBER: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on LOAD_MEMBER");
                        }
                        auto base_val = stack.back();
                        stack.pop_back();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto base_ptr = std::get<std::shared_ptr<Value>>(base_val.value);
                        base_ptr->may_object();
                        if (!std::holds_alternative<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(base_ptr->value)) {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto& obj = std::get<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(base_ptr->value);
                        auto member_id = instr.instr.member_id();
                        auto& ref = obj[member_id->id.value()];
                        if (!ref) {
                            ref = std::make_shared<Value>();
                        }
                        stack.push_back(Value{ref});
                        break;
                    }
                    case ebm::OpCode::LOAD_PARAM: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in LOAD_PARAM");
                        }
                        auto found = params.find(reg->index.id.value());
                        if (found == params.end()) {
                            return ebmgen::unexpect_error("undefined parameter variable");
                        }
                        stack.push_back(Value{found->second});
                        break;
                    }
                    case ebm::OpCode::CALL: {
                        size_t func_ip = 0;
                        MAYBE_VOID(r, interpret_impl(ctx, func_ip));
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