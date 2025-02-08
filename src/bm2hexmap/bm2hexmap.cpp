/*license*/
#include <bm2/context.hpp>
#include <bm/helper.hpp>
#include "bm2hexmap.hpp"
#include <json/json_export.h>
#include <json/to_string.h>
#include <fnet/util/base64.h>
#include <helper/expected.h>
#include <helper/expected_op.h>
namespace bm2hexmap {
    using TmpCodeWriter = bm2::TmpCodeWriter;
    namespace json = futils::json;

    struct OffsetRange {
        size_t start = 0;
        size_t end = 0;
        rebgn::Varint related_field;
        futils::view::rvec related_data;
        std::uint64_t interpreted_value = 0;
    };

    using Error = futils::error::Error<std::allocator<char>, std::string>;
    template <typename T>
    using expected = futils::helper::either::expected<T, Error>;

    template <typename... Args>
    auto unexpect_error(std::format_string<Args...> s, Args&&... args) {
        return futils::helper::either::unexpected{Error(futils::error::StrError{std::format(s, std::forward<decltype(args)>(args)...)})};
    }

    struct Value {
        std::variant<std::monostate, std::uint64_t,
                     std::string /*enum */, std::vector<std::shared_ptr<Value>>,
                     std::unordered_map<std::string, std::shared_ptr<Value>>>
            value;

        expected<std::uint64_t> as_int() {
            if (auto p = std::get_if<std::uint64_t>(&value); p) {
                return *p;
            }
            return unexpect_error("Not an int");
        }

        expected<std::shared_ptr<Value>> index(expected<std::uint64_t> index) {
            if (!index) {
                return unexpect_error("Index error: {}", index.error().error());
            }
            if (auto p = std::get_if<std::vector<std::shared_ptr<Value>>>(&value); p) {
                if (*index < p->size()) {
                    return (*p)[*index];
                }
            }
            return unexpect_error("Index error");
        }

        expected<size_t> size() {
            if (auto p = std::get_if<std::vector<std::shared_ptr<Value>>>(&value); p) {
                return p->size();
            }
            return unexpect_error("Size error");
        }

        expected<std::shared_ptr<Value>> index(const std::string& index) {
            if (std::holds_alternative<std::monostate>(value)) {
                value = std::unordered_map<std::string, std::shared_ptr<Value>>();
            }
            if (auto p = std::get_if<std::unordered_map<std::string, std::shared_ptr<Value>>>(&value); p) {
                if (auto found = p->find(index); found != p->end()) {
                    return found->second;
                }
                auto v = std::make_shared<Value>();
                (*p)[index] = v;
                return v;
            }
            return unexpect_error("Index error");
        }

        expected<void> push(expected<std::shared_ptr<Value>> v) {
            if (auto p = std::get_if<std::vector<std::shared_ptr<Value>>>(&value); p) {
                p->push_back(*v);
                return {};
            }
            return unexpect_error("Push error");
        }
    };

    using Map = std::unordered_map<std::string, std::shared_ptr<Value>>;

    struct Stack {
        rebgn::AbstractOp op = rebgn::AbstractOp::IMMEDIATE_INT;
        Map variables;
        size_t pc = 0;
        size_t end = 0;  // for loop or if
    };

    struct Context : bm2::Context {
        futils::view::rvec input_binary;
        json::JSON output;

        std::vector<OffsetRange> offset_ranges;

        std::vector<Stack> stack;

        Map self;

        std::vector<Map> self_stack;

        size_t read_offset = 0;

        std::map<std::string, std::vector<std::pair<std::uint64_t, std::string>>> enum_map;

        // map identifier to format name
        std::unordered_map<std::string, rebgn::Varint> format_table;
        Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident)
            : bm2::Context{w, bm, "r", "w", "(*this)", std::move(escape_ident)} {}

        std::vector<size_t> op_step;

        bool do_break(size_t& i) {
            while (stack.size()) {
                auto& top = stack.back();
                if (top.op == rebgn::AbstractOp::LOOP_INFINITE ||
                    top.op == rebgn::AbstractOp::LOOP_CONDITION) {
                    i = top.end;
                    stack.pop_back();
                    return true;
                }
                stack.pop_back();
            }
            output["error"] = "break";
            return false;
        }

        bool do_continue(size_t& i) {
            while (stack.size()) {
                auto& top = stack.back();
                if (top.op == rebgn::AbstractOp::LOOP_INFINITE ||
                    top.op == rebgn::AbstractOp::LOOP_CONDITION) {
                    i = top.pc - 1;
                    return true;
                }
                stack.pop_back();
            }
            output["error"] = "continue";
            return false;
        }

        bool do_end_if(size_t& i) {
            if (!stack.empty()) {
                auto& top = stack.back();
                if (top.op == rebgn::AbstractOp::IF || top.op == rebgn::AbstractOp::ELIF ||
                    top.op == rebgn::AbstractOp::ELSE) {
                    i = top.end;
                    stack.pop_back();
                    return true;
                }
            }
            output["error"] = "end if";
            return false;
        }

        void enter_stack(rebgn::AbstractOp op, size_t pc, size_t end) {
            stack.push_back(Stack{op, {}, pc, end});
        }

        std::string enum_str(const std::string& belong, std::uint64_t value) {
            auto found = enum_map.find(belong);
            if (found != enum_map.end()) {
                for (auto& [v, s] : found->second) {
                    if (v == value) {
                        return belong + "." + s;
                    }
                }
            }
            return std::format("{}({})", belong, value);
        }

        std::shared_ptr<Value> variable(const std::string& ident) {
            for (auto it = stack.rbegin(); it != stack.rend(); it++) {
                auto found = it->variables.find(ident);
                if (found != it->variables.end()) {
                    return found->second;
                }
            }
            return stack.back().variables[ident] = std::make_shared<Value>();
        }
    };
    std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0) {
        if (s.storages.size() <= index) {
            return "/*type index overflow*/";
        }
        auto& storage = s.storages[index];
        switch (storage.type) {
            case rebgn::StorageType::INT: {
                auto size = storage.size().value().value();
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 8) {
                    return "int8_t";
                }
                else if (size <= 16) {
                    return "int16_t";
                }
                else if (size <= 32) {
                    return "int32_t";
                }
                else {
                    return "int64_t";
                }
            }
            case rebgn::StorageType::UINT: {
                auto size = storage.size().value().value();
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 8) {
                    return "uint8_t";
                }
                else if (size <= 16) {
                    return "uint16_t";
                }
                else if (size <= 32) {
                    return "uint32_t";
                }
                else {
                    return "uint64_t";
                }
            }
            case rebgn::StorageType::FLOAT: {
                return "/*Unimplemented FLOAT*/";
            }
            case rebgn::StorageType::STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::RECURSIVE_STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return std::format("{}*", ident);
            }
            case rebgn::StorageType::BOOL: {
                return "bool";
            }
            case rebgn::StorageType::ENUM: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::ARRAY: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1);
                return "/*Unimplemented ARRAY*/";
            }
            case rebgn::StorageType::VECTOR: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1);
                return "/*Unimplemented VECTOR*/";
            }
            case rebgn::StorageType::VARIANT: {
                return "/*Unimplemented VARIANT*/";
            }
            case rebgn::StorageType::CODER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::PROPERTY_SETTER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::OPTIONAL: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1);
                return "/*Unimplemented OPTIONAL*/";
            }
            case rebgn::StorageType::PTR: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1);
                return std::format("{}*", base_type);
            }
            default: {
                return std::format("/*Unimplemented {}*/", to_string(storage.type));
            }
        }
    }
    std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref) {
        auto& storage = ctx.storage_table[ref.ref.value()];
        return type_to_string_impl(ctx, storage);
    }

    expected<std::shared_ptr<Value>> new_int(expected<std::uint64_t> value) {
        if (!value) {
            return unexpect_error("Value error: {}", value.error().error());
        }
        auto v = std::make_shared<Value>();
        v->value = value.value();
        return v;
    }

    std::shared_ptr<Value> new_array(size_t initial = 0) {
        auto v = std::make_shared<Value>();
        v->value = std::vector<std::shared_ptr<Value>>(initial);
        return v;
    }

    std::shared_ptr<Value> new_map() {
        auto v = std::make_shared<Value>();
        v->value = std::unordered_map<std::string, std::shared_ptr<Value>>();
        return v;
    }

    std::shared_ptr<Value> new_enum(const std::string& enum_str) {
        auto v = std::make_shared<Value>();
        v->value = enum_str;
        return v;
    }

    struct EvalResult {
        expected<std::shared_ptr<Value>> value;
        std::string repr;

        EvalResult() = default;

        EvalResult(const char* repr)
            : repr(repr) {
            value = unexpect_error("Not implemented");
        }

        EvalResult(std::string repr)
            : repr(repr) {
            value = unexpect_error("Not implemented");
        }

        EvalResult(expected<std::shared_ptr<Value>> value, std::string repr)
            : value(std::move(value)), repr(repr) {}
    };

    template <typename T, typename U>
    expected<std::pair<T, U>> get_left_and_right(expected<T> left, expected<U> right) {
        return left.and_then([right = std::move(right)](auto&& l) { return right.transform([l = std::move(l)](auto&& r) { return std::pair{l, r}; }); });
    }

    std::vector<EvalResult> eval(const rebgn::Code& code, Context& ctx) {
        std::vector<EvalResult> result;
        switch (code.op) {
            case rebgn::AbstractOp::DEFINE_FIELD: {
                auto ident = ctx.ident(code.ident().value());
                auto& field = ctx.self[ident];
                if (!field) {
                    field = std::make_shared<Value>();
                }
                result.push_back(EvalResult(field, ident));
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                result.push_back("/*Unimplemented DEFINE_PROPERTY*/");
                break;
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                result.push_back("/*Unimplemented DEFINE_PARAMETER*/");
                break;
            }
            case rebgn::AbstractOp::INPUT_BYTE_OFFSET: {
                // result.push_back("/*Unimplemented INPUT_BYTE_OFFSET*/");
                result.push_back(EvalResult(new_int(ctx.read_offset), "input.offset"));
                break;
            }
            case rebgn::AbstractOp::OUTPUT_BYTE_OFFSET: {
                result.push_back("/*Unimplemented OUTPUT_BYTE_OFFSET*/");
                break;
            }
            case rebgn::AbstractOp::INPUT_BIT_OFFSET: {
                result.push_back("/*Unimplemented INPUT_BIT_OFFSET*/");
                break;
            }
            case rebgn::AbstractOp::OUTPUT_BIT_OFFSET: {
                result.push_back("/*Unimplemented OUTPUT_BIT_OFFSET*/");
                break;
            }
            case rebgn::AbstractOp::REMAIN_BYTES: {
                result.push_back("/*Unimplemented REMAIN_BYTES*/");
                break;
            }
            case rebgn::AbstractOp::CAN_READ: {
                auto can_read = ctx.read_offset < ctx.input_binary.size();
                result.push_back(EvalResult(new_int(can_read), "can_read"));
                break;
            }
            case rebgn::AbstractOp::IS_LITTLE_ENDIAN: {
                result.push_back("/*Unimplemented IS_LITTLE_ENDIAN*/");
                break;
            }
            case rebgn::AbstractOp::CAST: {
                auto type = code.type().value();
                auto ref = code.ref().value();
                auto type_str = type_to_string(ctx, type);
                auto evaluated = eval(ctx.ref(ref), ctx);
                result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);
                if (code.cast_type() == rebgn::CastType::INT_TO_ENUM) {
                    auto last_int = evaluated.back().value & [&](auto v) {
                        return v->as_int();
                    };
                    if (!last_int) {
                        return {EvalResult(last_int.error().error()), ""};
                    }
                    auto enum_ref = ctx.storage_table[type.ref.value()].storages[0].ref().value();
                    auto ident = ctx.ident(enum_ref);
                    auto repr = ctx.enum_str(ident, *last_int);
                    result.push_back(EvalResult(new_enum(ctx.enum_str(ident, *last_int)), repr));
                }
                else {
                    result.push_back(EvalResult(evaluated.back().value, std::format("({}){}", type_str, evaluated.back().repr)));
                }
                break;
            }
            case rebgn::AbstractOp::CALL_CAST: {
                result.push_back("/*Unimplemented CALL_CAST*/");
                break;
            }
            case rebgn::AbstractOp::ADDRESS_OF: {
                result.push_back("/*Unimplemented ADDRESS_OF*/");
                break;
            }
            case rebgn::AbstractOp::OPTIONAL_OF: {
                result.push_back("/*Unimplemented OPTIONAL_OF*/");
                break;
            }
            case rebgn::AbstractOp::EMPTY_PTR: {
                result.push_back("/*Unimplemented EMPTY_PTR*/");
                break;
            }
            case rebgn::AbstractOp::EMPTY_OPTIONAL: {
                result.push_back("/*Unimplemented EMPTY_OPTIONAL*/");
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto ident = ctx.ident(code.ident().value());
                // auto ref = code.ref().value();
                // auto type = type_to_string(ctx, code.type().value());
                // auto evaluated = eval(ctx.ref(ref), ctx);
                // result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);
                // result.push_back(std::format("{} {} = {}", type, ident, evaluated.back().repr));
                result.push_back(EvalResult(ctx.variable(ident), ident));
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE_REF: {
                auto ref = code.ref().value();
                return eval(ctx.ref(ref), ctx);
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                result.push_back("/*Unimplemented DEFINE_CONSTANT*/");
                break;
            }
            case rebgn::AbstractOp::DECLARE_VARIABLE: {
                auto ref = code.ref().value();
                return eval(ctx.ref(ref), ctx);
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                auto op = code.bop().value();
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);
                auto opstr = to_string(op);
                auto repr = std::format("({} {} {})", left_eval.back().repr, opstr, right_eval.back().repr);
                auto value = get_left_and_right(left_eval.back().value, right_eval.back().value) & [&](auto pair) {
                    return get_left_and_right(pair.first->as_int(), pair.second->as_int()) & [&](auto pair) {
                        return new_int([&] -> expected<std::uint64_t> {
                            auto left = pair.first;
                            auto right = pair.second;
                            switch (op) {
                                case rebgn::BinaryOp::add:
                                    return left + right;
                                case rebgn::BinaryOp::sub:
                                    return left - right;
                                case rebgn::BinaryOp::mul:
                                    return left * right;
                                case rebgn::BinaryOp::div:
                                    return left / right;
                                case rebgn::BinaryOp::mod:
                                    return left % right;
                                case rebgn::BinaryOp::bit_and:
                                    return left & right;
                                case rebgn::BinaryOp::bit_or:
                                    return left | right;
                                case rebgn::BinaryOp::bit_xor:
                                    return left ^ right;
                                case rebgn::BinaryOp::left_logical_shift:
                                    return left << right;
                                case rebgn::BinaryOp::right_logical_shift:
                                    return left >> right;
                                case rebgn::BinaryOp::logical_and:
                                    return std::uint64_t(left && right);
                                case rebgn::BinaryOp::logical_or:
                                    return std::uint64_t(left || right);
                                case rebgn::BinaryOp::equal:
                                    return std::uint64_t(left == right);
                                case rebgn::BinaryOp::not_equal:
                                    return std::uint64_t(left != right);
                                case rebgn::BinaryOp::less:
                                    return std::uint64_t(left < right);
                                case rebgn::BinaryOp::less_or_eq:
                                    return std::uint64_t(left <= right);
                                case rebgn::BinaryOp::grater:
                                    return std::uint64_t(left > right);
                                case rebgn::BinaryOp::grater_or_eq:
                                    return std::uint64_t(left >= right);
                                default:
                                    return unexpect_error("Unhandled binary op: {}", to_string(op));
                            }
                        }());
                    };
                };
                result.push_back(EvalResult{std::move(value), repr});
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                auto op = code.uop().value();
                auto ref = code.ref().value();
                auto target = eval(ctx.ref(ref), ctx);
                auto opstr = to_string(op);
                auto repr = std::format("({}{})", opstr, target.back().repr);
                auto value = target.back().value & [&](auto v) {
                    return new_int(v->as_int() & [&](auto i) -> expected<std::uint64_t> {
                        switch (op) {
                            case rebgn::UnaryOp::minus_sign:
                                return -i;
                            case rebgn::UnaryOp::bit_not:
                                return ~i;
                            case rebgn::UnaryOp::logical_not:
                                return !i ? std::uint64_t(1) : std::uint64_t(0);
                            default:
                                return unexpect_error("Unhandled unary op: {}", to_string(op));
                        }
                    });
                };
                result.push_back(EvalResult{std::move(value), repr});
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);
                auto repr = std::format("{} = {}", left_eval.back().repr, right_eval.back().repr);
                result.push_back(right_eval.back());
                result.push_back(left_eval.back());
                break;
            }
            case rebgn::AbstractOp::ACCESS: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_ident = ctx.ident(right_ref);
                auto repr = std::format("{}.{}", left_eval.back().repr, right_ident);
                if (ctx.ref(left_ref).op == rebgn::AbstractOp::IMMEDIATE_TYPE) {
                    // enum
                    result.push_back(EvalResult(new_enum(repr), repr));
                }
                else {
                    auto value = left_eval.back().value & [&](auto v) {
                        return v->index(right_ident);
                    };
                    result.push_back(EvalResult{std::move(value), repr});
                }
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);
                auto repr = std::format("{}[{}]", left_eval.back().repr, right_eval.back().repr);
                auto value = get_left_and_right(left_eval.back().value, right_eval.back().value) & [&](auto pair) {
                    return pair.first->index(pair.second->as_int());
                };
                result.push_back(EvalResult{std::move(value), repr});
                break;
            }
            case rebgn::AbstractOp::CALL: {
                result.push_back("/*Unimplemented CALL*/");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TRUE: {
                result.push_back(EvalResult{new_int(1), "true"});
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_FALSE: {
                result.push_back(EvalResult{new_int(0), "false"});
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT: {
                result.push_back(EvalResult{new_int(code.int_value().value().value()), std::format("{}", code.int_value().value().value())});
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT64: {
                result.push_back(EvalResult{new_int(*code.int_value64()), std::format("{}", *code.int_value64())});
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_CHAR: {
                auto char_code = code.int_value()->value();
                result.push_back("/*Unimplemented IMMEDIATE_CHAR*/");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_STRING: {
                result.push_back("/*Unimplemented IMMEDIATE_STRING*/");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TYPE: {
                auto type = code.type().value();
                result.push_back(type_to_string(ctx, type));
                break;
            }
            case rebgn::AbstractOp::NEW_OBJECT: {
                auto& typ = ctx.storage_table[code.type().value().ref.value()].storages[0];
                if (typ.type == rebgn::StorageType::BOOL || typ.type == rebgn::StorageType::INT || typ.type == rebgn::StorageType::UINT) {
                    result.push_back(EvalResult{new_int(0), "0"});
                }
                else if (typ.type == rebgn::StorageType::ARRAY) {
                    auto size = typ.size().value().value();
                    result.push_back(EvalResult{new_array(size), "[]"});
                }
                else if (typ.type == rebgn::StorageType::VECTOR) {
                    result.push_back(EvalResult{new_array(0), "[]"});
                }
                else {
                    result.push_back(EvalResult(new_map(), "{}"));
                }
                break;
            }
            case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                result.push_back("/*Unimplemented PROPERTY_INPUT_PARAMETER*/");
                break;
            }
            case rebgn::AbstractOp::ARRAY_SIZE: {
                auto vector_ref = code.ref().value();
                auto vector_eval = eval(ctx.ref(vector_ref), ctx);
                result.insert(result.end(), vector_eval.begin(), vector_eval.end() - 1);
                auto repr = std::format("{}.size()", vector_eval.back().repr);
                auto value = new_int(vector_eval.back().value & [&](auto v) {
                    return v->size();
                });
                result.push_back(EvalResult{std::move(value), repr});
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                auto left_ref = code.left_ref().value();
                if (left_ref.value() == 0) {
                    auto right_ref = code.right_ref().value();
                    auto right_eval = eval(ctx.ref(right_ref), ctx);
                    result.insert(result.end(), right_eval.begin(), right_eval.end());
                }
                else {
                    auto left_eval = eval(ctx.ref(left_ref), ctx);
                    result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                    ctx.this_as.push_back(left_eval.back().repr);
                    auto right_ref = code.right_ref().value();
                    auto right_eval = eval(ctx.ref(right_ref), ctx);
                    result.insert(result.end(), right_eval.begin(), right_eval.end());
                    ctx.this_as.pop_back();
                }
                break;
            }
            case rebgn::AbstractOp::PHI: {
                auto ref = code.ref().value();
                return eval(ctx.ref(ref), ctx);
                break;
            }
            default: {
                result.push_back(std::format("/*Unimplemented {}*/", to_string(code.op)));
                break;
            }
        }
        return result;
    }
    void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto type = type_to_string(ctx, code.type().value());
                    auto ident = ctx.ident(code.ident().value());
                    w.write(type, " ", ident);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented ENCODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented DECODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto type = type_to_string(ctx, code.type().value());
                    auto ident = ctx.ident(code.ident().value());
                    w.write(type, " ", ident);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto type = type_to_string(ctx, code.type().value());
                    auto ident = ctx.ident(code.ident().value());
                    w.write(type, " ", ident);
                    params++;
                    break;
                }
                default: {
                    // skip other op
                    break;
                }
            }
        }
    }
    void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented DEFINE_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented ENCODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented DECODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented STATE_VARIABLE_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if (params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto ident = ctx.ident(ref);
                    w.writeln(ident);
                    params++;
                    break;
                }
                default: {
                    // skip other op
                    break;
                }
            }
        }
    }
    void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_FORMAT: {
                    auto ident = ctx.ident(code.ident().value());
                    w.writeln("struct ", ident, " { ");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_FORMAT: {
                    defer.pop_back();
                    w.writeln("};");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FORMAT: {
                    auto ref = code.ref().value();
                    auto range = ctx.range(ref);
                    inner_block(ctx, w, range);
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FIELD: {
                    auto type = type_to_string(ctx, code.type().value());

                    break;
                }
                case rebgn::AbstractOp::DEFINE_PROPERTY: {
                    w.writeln("/*Unimplemented DEFINE_PROPERTY*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_PROPERTY: {
                    w.writeln("/*Unimplemented END_PROPERTY*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_PROPERTY: {
                    auto ref = code.ref().value();
                    auto range = ctx.range(ref);
                    inner_block(ctx, w, range);
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FUNCTION: {
                    w.writeln("/*Unimplemented DECLARE_FUNCTION*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_ENUM: {
                    auto ident = ctx.ident(code.ident().value());
                    w.writeln("enum ", ident, " { ");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_ENUM: {
                    defer.pop_back();
                    w.writeln("};");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_ENUM: {
                    auto ref = code.ref().value();
                    auto range = ctx.range(ref);
                    inner_block(ctx, w, range);
                    break;
                }
                case rebgn::AbstractOp::DEFINE_ENUM_MEMBER: {
                    auto ident = ctx.ident(code.ident().value());
                    auto evaluated = eval(ctx.ref(code.left_ref().value()), ctx);
                    w.writeln(ident, " = ", evaluated.back().repr, ",");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_UNION: {
                    w.writeln("/*Unimplemented DEFINE_UNION*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_UNION: {
                    w.writeln("/*Unimplemented END_UNION*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_UNION: {
                    w.writeln("/*Unimplemented DECLARE_UNION*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
                    w.writeln("/*Unimplemented DEFINE_UNION_MEMBER*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_UNION_MEMBER: {
                    w.writeln("/*Unimplemented END_UNION_MEMBER*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_UNION_MEMBER: {
                    w.writeln("/*Unimplemented DECLARE_UNION_MEMBER*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_STATE: {
                    auto ident = ctx.ident(code.ident().value());
                    w.writeln("struct ", ident, " { ");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_STATE: {
                    defer.pop_back();
                    w.writeln("};");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_STATE: {
                    auto ref = code.ref().value();
                    auto range = ctx.range(ref);
                    inner_block(ctx, w, range);
                    break;
                }
                case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
                    w.writeln("/*Unimplemented DEFINE_BIT_FIELD*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_BIT_FIELD: {
                    w.writeln("/*Unimplemented END_BIT_FIELD*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_BIT_FIELD: {
                    w.writeln("/*Unimplemented DECLARE_BIT_FIELD*/ ");
                    break;
                }
                default: {
                    if (!rebgn::is_marker(code.op) && !rebgn::is_expr(code.op) && !rebgn::is_parameter_related(code.op)) {
                        w.writeln(std::format("/*Unimplemented {}*/", to_string(code.op)));
                    }
                    break;
                }
            }
        }
    }

    bool map_n_bit_to_field(Context& ctx, futils::binary::reader& r, size_t bit_size, rebgn::Varint ref, rebgn::Varint related_field, rebgn::EndianExpr endian) {
        auto evaluated = eval(ctx.ref(ref), ctx);
        if (!evaluated.back().value) {
            ctx.output["error"] = evaluated.back().value.error().error();
            return false;
        }

        if (bit_size % 8 == 0) {
            futils::view::rvec data;
            auto offset = r.offset();
            if (!r.read(data, bit_size / 8)) {
                ctx.output["error"] = "shorter than expected";
                return false;
            }
            auto then_offset = r.offset();
            std::uint64_t interpreted_value = 0;
            for (size_t i = 0; i < data.size(); i++) {
                if (endian.endian() == rebgn::Endian::little ||
                    (endian.endian() == rebgn::Endian::native && futils::binary::is_little())) {
                    interpreted_value |= (std::uint64_t)data[i] << (i * 8);
                }
                else {
                    interpreted_value |= (std::uint64_t)data[i] << ((data.size() - 1 - i) * 8);
                }
            }
            ctx.offset_ranges.push_back({
                .start = offset,
                .end = then_offset,
                .related_field = related_field,
                .related_data = data,
                .interpreted_value = interpreted_value,
            });
            ctx.read_offset = r.offset();
            evaluated.back().value.value()->value = interpreted_value;
            return true;
        }
        ctx.output["error"] = "bit size not multiple of 8";
        return false;
    }

    size_t find_next_else_or_end_if(Context& ctx, size_t start, bool include_else = false) {
        size_t nested = 0;
        for (size_t i = start + 1;; i++) {
            if (i >= ctx.bm.code.size()) {
                return ctx.bm.code.size();
            }
            auto& code = ctx.bm.code[i];
            if (nested) {
                if (code.op == rebgn::AbstractOp::END_IF) {
                    nested--;
                }
            }
            else {
                if (!include_else) {
                    if (code.op == rebgn::AbstractOp::END_IF) {
                        return i;
                    }
                }
                else {
                    if (code.op == rebgn::AbstractOp::ELIF || code.op == rebgn::AbstractOp::ELSE || code.op == rebgn::AbstractOp::END_IF) {
                        return i;
                    }
                }
            }
            if (code.op == rebgn::AbstractOp::IF) {
                nested++;
            }
        }
    }

    size_t find_next_end_loop(Context& ctx, size_t start) {
        size_t nested = 0;
        for (size_t i = start + 1;; i++) {
            if (i >= ctx.bm.code.size()) {
                return ctx.bm.code.size();
            }
            auto& code = ctx.bm.code[i];
            if (nested) {
                if (code.op == rebgn::AbstractOp::END_LOOP) {
                    nested--;
                }
            }
            else {
                if (code.op == rebgn::AbstractOp::END_LOOP) {
                    return i;
                }
            }
            if (code.op == rebgn::AbstractOp::LOOP_INFINITE || code.op == rebgn::AbstractOp::LOOP_CONDITION) {
                nested++;
            }
        }
    }

    void inner_function(Context& ctx, futils::binary::reader& r, rebgn::Range range) {
        // std::vector<futils::helper::DynDefer> defer;
        bool from_if = false;
        TmpCodeWriter w;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            auto d = futils::helper::defer([&] {
                ctx.op_step.push_back(i);
            });
            switch (code.op) {
                case rebgn::AbstractOp::METADATA: {
                    w.writeln("/*Unimplemented METADATA*/ ");
                    break;
                }
                case rebgn::AbstractOp::IMPORT: {
                    w.writeln("/*Unimplemented IMPORT*/ ");
                    break;
                }
                case rebgn::AbstractOp::DYNAMIC_ENDIAN: {
                    w.writeln("/*Unimplemented DYNAMIC_ENDIAN*/ ");
                    break;
                }
                case rebgn::AbstractOp::SPECIFY_BIT_ORDER: {
                    w.writeln("/*Unimplemented SPECIFY_BIT_ORDER*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FUNCTION: {
                    auto ident = ctx.ident(code.ident().value());
                    auto range = ctx.range(code.ident().value());
                    auto found_type_pos = find_op(ctx, range, rebgn::AbstractOp::RETURN_TYPE);
                    if (!found_type_pos) {
                        w.writeln("void");
                    }
                    else {
                        auto type = type_to_string(ctx, ctx.bm.code[*found_type_pos].type().value());
                        w.writeln(type);
                    }
                    w.writeln(" ", ident, "(");
                    add_parameter(ctx, w, range);
                    w.writeln(") { ");
                    // defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_FUNCTION: {
                    // defer.pop_back();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::SEEK_ENCODER:
                case rebgn::AbstractOp::SEEK_DECODER: {
                    break;
                }
                case rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION: {
                    w.writeln("/*Unimplemented BEGIN_ENCODE_PACKED_OPERATION*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION: {
                    w.writeln("/*Unimplemented END_ENCODE_PACKED_OPERATION*/ ");
                    break;
                }
                case rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION: {
                    w.writeln("/*Unimplemented BEGIN_DECODE_PACKED_OPERATION*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_DECODE_PACKED_OPERATION: {
                    w.writeln("/*Unimplemented END_DECODE_PACKED_OPERATION*/ ");
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT: {
                    break;  // no encode in hexmap
                }
                case rebgn::AbstractOp::DECODE_INT: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref = code.ref().value();
                    auto endian = code.endian().value();
                    auto belong = code.belong().value();
                    if (!map_n_bit_to_field(ctx, r, bit_size, ref, belong, endian)) {
                        return;
                    }
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT_VECTOR: {
                    w.writeln("/*Unimplemented ENCODE_INT_VECTOR*/ ");
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT_VECTOR_FIXED: {
                    w.writeln("/*Unimplemented ENCODE_INT_VECTOR_FIXED*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR: {
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF: {
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR_UNTIL_EOF*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref = code.ref().value();
                    auto endian = code.endian().value();
                    auto belong = code.belong().value();
                    auto array_size = code.array_length().value().value();
                    for (size_t i = 0; i < array_size; i++) {
                        if (!map_n_bit_to_field(ctx, r, bit_size, ref, belong, endian)) {
                            return;
                        }
                    }
                    break;
                }
                case rebgn::AbstractOp::PEEK_INT_VECTOR: {
                    w.writeln("/*Unimplemented PEEK_INT_VECTOR*/ ");
                    break;
                }
                case rebgn::AbstractOp::BACKWARD_INPUT: {
                    w.writeln("/*Unimplemented BACKWARD_INPUT*/ ");
                    break;
                }
                case rebgn::AbstractOp::BACKWARD_OUTPUT: {
                    w.writeln("/*Unimplemented BACKWARD_OUTPUT*/ ");
                    break;
                }
                case rebgn::AbstractOp::CALL_ENCODE: {
                    w.writeln("/*Unimplemented CALL_ENCODE*/ ");
                    break;
                }
                case rebgn::AbstractOp::CALL_DECODE: {
                    auto ref_to_decoder = code.left_ref().value();
                    ctx.self_stack.push_back(std::move(ctx.self));
                    ctx.enter_stack(rebgn::AbstractOp::CALL_DECODE, i, i);
                    auto range = ctx.range(ref_to_decoder);
                    inner_function(ctx, r, range);
                    auto ref = eval(ctx.ref(code.right_ref().value()), ctx);
                    if (!ref.back().value) {
                        ctx.output["error"] = ref.back().value.error().error();
                        return;
                    }
                    ref.back().value.value()->value = std::move(ctx.self);
                    ctx.stack.pop_back();
                    ctx.self = std::move(ctx.self_stack.back());
                    ctx.self_stack.pop_back();
                    // w.writeln("/*Unimplemented CALL_DECODE*/ ");
                    break;
                }
                case rebgn::AbstractOp::LOOP_INFINITE: {
                    // w.writeln("for(;;) {");
                    //  defer.push_back(w.indent_scope_ex());
                    auto end = find_next_end_loop(ctx, i);
                    if (end == ctx.bm.code.size()) {
                        ctx.output["error"] = "no end loop found";
                        return;
                    }
                    ctx.enter_stack(rebgn::AbstractOp::LOOP_INFINITE, i, end);
                    break;
                }
                case rebgn::AbstractOp::LOOP_CONDITION: {
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    if (!evaluated.back().value) {
                        ctx.output["error"] = evaluated.back().value.error().error();
                        return;
                    }
                    auto res = evaluated.back().value.value()->as_int();
                    if (!res) {
                        ctx.output["error"] = "loop condition not int";
                        return;
                    }
                    auto end = find_next_end_loop(ctx, i);
                    if (end == ctx.bm.code.size()) {
                        ctx.output["error"] = "no end loop found";
                        return;
                    }
                    if (*res) {
                        ctx.enter_stack(rebgn::AbstractOp::LOOP_CONDITION, i, end);
                    }
                    else {
                        i = end;
                    }
                    // w.writeln("while (", evaluated.back().repr, ") {");
                    // defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::CONTINUE: {
                    // w.writeln("continue;");
                    if (!ctx.do_continue(i)) {
                        return;
                    }
                    break;
                }
                case rebgn::AbstractOp::BREAK: {
                    if (!ctx.do_break(i)) {
                        return;
                    }
                    break;
                }
                case rebgn::AbstractOp::END_LOOP: {
                    // defer.pop_back();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::IF: {
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    // w.writeln("if (", evaluated.back().repr, ") {");
                    // defer.push_back(w.indent_scope_ex());
                    auto end = find_next_else_or_end_if(ctx, i, true);
                    if (end == ctx.bm.code.size()) {
                        ctx.output["error"] = "no end if found";
                        return;
                    }
                    if (!evaluated.back().value) {
                        ctx.output["error"] = evaluated.back().value.error().error();
                        return;
                    }
                    auto res = evaluated.back().value.value()->as_int();
                    if (!res) {
                        ctx.output["error"] = "if condition not int";
                        return;
                    }
                    if (*res) {
                        auto end_final = find_next_else_or_end_if(ctx, i, false);
                        ctx.enter_stack(rebgn::AbstractOp::IF, i, end_final);
                    }
                    else {
                        if (ctx.bm.code[i].op != rebgn::AbstractOp::END_IF) {
                            i = end - 1;
                            from_if = true;
                        }
                        else {
                            i = end;
                        }
                    }
                    break;
                }
                case rebgn::AbstractOp::ELIF: {
                    if (!from_if) {
                        if (!ctx.do_end_if(i)) {
                            return;
                        }
                        break;
                    }
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    // defer.pop_back();
                    // w.writeln("}");
                    // w.writeln("else if (", evaluated.back().repr, ") {");
                    // defer.push_back(w.indent_scope_ex());
                    auto end = find_next_else_or_end_if(ctx, i, true);
                    if (end == ctx.bm.code.size()) {
                        ctx.output["error"] = "no end if found";
                        return;
                    }
                    if (!evaluated.back().value) {
                        ctx.output["error"] = evaluated.back().value.error().error();
                        return;
                    }
                    auto res = evaluated.back().value.value()->as_int();
                    if (!res) {
                        ctx.output["error"] = "if condition not int";
                        return;
                    }
                    if (*res) {
                        auto end_final = find_next_else_or_end_if(ctx, i, false);
                        ctx.enter_stack(rebgn::AbstractOp::ELIF, i, end_final);
                    }
                    else {
                        if (ctx.bm.code[i].op != rebgn::AbstractOp::END_IF) {
                            i = end - 1;
                        }
                        else {
                            i = end;
                        }
                    }
                    break;
                }
                case rebgn::AbstractOp::ELSE: {
                    if (!from_if) {
                        if (!ctx.do_end_if(i)) {
                            return;
                        }
                    }
                    auto end = find_next_else_or_end_if(ctx, i, true);
                    if (end == ctx.bm.code.size()) {
                        ctx.output["error"] = "no end if found";
                        return;
                    }
                    ctx.enter_stack(rebgn::AbstractOp::ELSE, i, end);
                    // defer.pop_back();
                    // w.writeln("}");
                    // w.writeln("else {");
                    // defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_IF: {
                    if (!ctx.do_end_if(i)) {
                        return;
                    }
                    // defer.pop_back();
                    // w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::MATCH: {
                    w.writeln("/*Unimplemented MATCH*/ ");
                    break;
                }
                case rebgn::AbstractOp::EXHAUSTIVE_MATCH: {
                    w.writeln("/*Unimplemented EXHAUSTIVE_MATCH*/ ");
                    break;
                }
                case rebgn::AbstractOp::CASE: {
                    w.writeln("/*Unimplemented CASE*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_CASE: {
                    w.writeln("/*Unimplemented END_CASE*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFAULT_CASE: {
                    w.writeln("/*Unimplemented DEFAULT_CASE*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_MATCH: {
                    w.writeln("/*Unimplemented END_MATCH*/ ");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_VARIABLE: {
                    auto new_variable = std::make_shared<Value>();
                    auto ident = ctx.ident(code.ident().value());
                    auto ref = code.ref().value();
                    auto init_eval = eval(ctx.ref(code.ref().value()), ctx);
                    if (!init_eval.back().value) {
                        ctx.output["error"] = init_eval.back().value.error().error();
                        return;
                    }
                    new_variable->value = init_eval.back().value.value()->value;
                    ctx.stack.back().variables[ident] = new_variable;
                    break;
                }
                case rebgn::AbstractOp::DEFINE_CONSTANT: {
                    w.writeln("/*Unimplemented DEFINE_CONSTANT*/ ");
                    break;
                }
                case rebgn::AbstractOp::ASSIGN: {
                    auto evaluated = eval(code, ctx);
                    // w.writeln(evaluated[evaluated.size() - 2].repr);
                    auto init = evaluated[evaluated.size() - 2].value;
                    auto ref = evaluated[evaluated.size() - 1].value;
                    if (!ref || !init) {
                        ctx.output["error"] = "variable assign error";
                        return;
                    }
                    (*ref)->value = (*init)->value;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_ASSIGN: {
                    w.writeln("/*Unimplemented PROPERTY_ASSIGN*/ ");
                    break;
                }
                case rebgn::AbstractOp::ASSERT: {
                    auto evaluated = eval(ctx.ref(code.ref().value()), ctx);
                    w.writeln("/*Unimplemented ASSERT*/");
                    break;
                }
                case rebgn::AbstractOp::LENGTH_CHECK: {
                    w.writeln("/*Unimplemented LENGTH_CHECK*/ ");
                    break;
                }
                case rebgn::AbstractOp::EXPLICIT_ERROR: {
                    auto param = code.param().value();
                    auto evaluated = eval(ctx.ref(param.expr_refs[0]), ctx);
                    w.writeln("/*Unimplemented EXPLICIT_ERROR*/");
                    break;
                }
                case rebgn::AbstractOp::APPEND: {
                    auto vector_ref = code.left_ref().value();
                    auto new_element_ref = code.right_ref().value();
                    auto vector_eval = eval(ctx.ref(vector_ref), ctx);
                    auto new_element_eval = eval(ctx.ref(new_element_ref), ctx);
                    if (!vector_eval.back().value) {
                        ctx.output["error"] = vector_eval.back().value.error().error();
                        return;
                    }
                    auto res = vector_eval.back().value.value()->push(new_element_eval.back().value);
                    if (!res) {
                        ctx.output["error"] = res.error().error();
                        return;
                    }
                    break;
                }
                case rebgn::AbstractOp::INC: {
                    auto evaluated = eval(ctx.ref(code.ref().value()), ctx);
                    if (!evaluated.back().value) {
                        ctx.output["error"] = evaluated.back().value.error().error();
                        return;
                    }
                    auto val = evaluated.back().value.value();
                    auto res = val->as_int();
                    if (!res) {
                        ctx.output["error"] = res.error().error();
                        return;
                    }
                    val->value = *res + 1;
                    // w.writeln("/*Unimplemented INC*/ ");
                    break;
                }
                case rebgn::AbstractOp::RET: {
                    auto ref = code.ref().value();
                    if (ref.value() != 0) {
                        auto evaluated = eval(ctx.ref(ref), ctx);
                        w.writeln("return ", evaluated.back().repr, ";");
                    }
                    else {
                        w.writeln("return;");
                    }
                    break;
                }
                case rebgn::AbstractOp::RET_SUCCESS: {
                    w.writeln("/*Unimplemented RET_SUCCESS*/ ");
                    break;
                }
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_OK: {
                    w.writeln("/*Unimplemented RET_PROPERTY_SETTER_OK*/ ");
                    break;
                }
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_FAIL: {
                    w.writeln("/*Unimplemented RET_PROPERTY_SETTER_FAIL*/ ");
                    break;
                }
                case rebgn::AbstractOp::INIT_RECURSIVE_STRUCT: {
                    w.writeln("/*Unimplemented INIT_RECURSIVE_STRUCT*/ ");
                    break;
                }
                case rebgn::AbstractOp::CHECK_RECURSIVE_STRUCT: {
                    w.writeln("/*Unimplemented CHECK_RECURSIVE_STRUCT*/ ");
                    break;
                }
                case rebgn::AbstractOp::SWITCH_UNION: {
                    w.writeln("/*Unimplemented SWITCH_UNION*/ ");
                    break;
                }
                case rebgn::AbstractOp::CHECK_UNION: {
                    w.writeln("/*Unimplemented CHECK_UNION*/ ");
                    break;
                }
                case rebgn::AbstractOp::EVAL_EXPR: {
                    w.writeln("/*Unimplemented EVAL_EXPR*/ ");
                    break;
                }
                case rebgn::AbstractOp::RESERVE_SIZE: {
                    w.writeln("/*Unimplemented RESERVE_SIZE*/ ");
                    break;
                }
                case rebgn::AbstractOp::BEGIN_ENCODE_SUB_RANGE: {
                    w.writeln("/*Unimplemented BEGIN_ENCODE_SUB_RANGE*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_ENCODE_SUB_RANGE: {
                    w.writeln("/*Unimplemented END_ENCODE_SUB_RANGE*/ ");
                    break;
                }
                case rebgn::AbstractOp::BEGIN_DECODE_SUB_RANGE: {
                    w.writeln("/*Unimplemented BEGIN_DECODE_SUB_RANGE*/ ");
                    break;
                }
                case rebgn::AbstractOp::END_DECODE_SUB_RANGE: {
                    w.writeln("/*Unimplemented END_DECODE_SUB_RANGE*/ ");
                    break;
                }
                default: {
                    if (!rebgn::is_marker(code.op) && !rebgn::is_struct_define_related(code.op) && !rebgn::is_expr(code.op) && !rebgn::is_parameter_related(code.op)) {
                        ctx.output["error"] = std::format("Unimplemented {}", to_string(code.op));
                        return;
                    }
                    d.cancel();
                    break;
                }
            }
        }
    }

    std::string related_field_to_string(Context& ctx, rebgn::Varint related_field) {
        if (auto belong = ctx.ref(related_field).belong()) {
            return related_field_to_string(ctx, *belong) + "." + ctx.ident(related_field);
        }
        return ctx.ident(related_field);
    }

    void map_value_to_json(Context& ctx, json::JSON& json, const std::shared_ptr<Value>& val) {
        if (std::holds_alternative<std::uint64_t>(val->value)) {
            json = std::uint64_t(std::get<std::uint64_t>(val->value));
        }
        else if (std::holds_alternative<std::string>(val->value)) {
            json = std::get<std::string>(val->value);
        }
        else if (std::holds_alternative<std::vector<std::shared_ptr<Value>>>(val->value)) {
            auto& vec = std::get<std::vector<std::shared_ptr<Value>>>(val->value);
            json::JSON arr;
            for (auto& v : vec) {
                json::JSON j;
                map_value_to_json(ctx, j, v);
                arr.push_back(j);
            }
            json = std::move(arr);
        }
        else if (std::holds_alternative<std::unordered_map<std::string, std::shared_ptr<Value>>>(val->value)) {
            auto& map = std::get<std::unordered_map<std::string, std::shared_ptr<Value>>>(val->value);
            json::JSON obj;
            for (auto& [k, v] : map) {
                json::JSON j;
                map_value_to_json(ctx, j, v);
                obj[k] = j;
            }
            json = std::move(obj);
        }
        else {
            json = nullptr;
        }
    }

    void dump_json(Context& ctx) {
        std::vector<json::JSON> offset_ranges;
        for (auto& range : ctx.offset_ranges) {
            json::JSON json;
            json["start"] = std::uint64_t(range.start);
            json["end"] = std::uint64_t(range.end);
            json["related_field"] = related_field_to_string(ctx, range.related_field);

            auto& x = json["related_data"];
            for (auto& y : range.related_data) {
                x.push_back(std::uint64_t(y));
            }
            json["interpreted_value"] = range.interpreted_value;
            offset_ranges.push_back(json);
        }
        ctx.output["offset_ranges"] = std::move(offset_ranges);
        std::vector<json::JSON> ops;
        for (auto& op : ctx.op_step) {
            ops.push_back(to_string(ctx.bm.code[op].op));
        }
        ctx.output["op_step"] = std::move(ops);
        auto& self = ctx.output["self"];
        for (auto& kv : ctx.self) {
            json::JSON json;
            map_value_to_json(ctx, json, kv.second);
            self[kv.first] = json;
        }
        auto& variables = ctx.output["variables"];
        variables.init_array();
        for (auto& s : ctx.stack) {
            json::JSON base;
            base.init_obj();
            for (auto& kv : s.variables) {
                json::JSON json;
                map_value_to_json(ctx, json, kv.second);
                base[kv.first] = json;
            }
            variables.push_back(base);
        }
        json::Stringer<futils::binary::writer&> s{ctx.cw.out()};
        json::to_string(ctx.output, s);
    }

    void to_hexmap(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {
        Context ctx{w, bm, [&](bm2::Context& ctx, std::uint64_t id, auto&& str) {
                        return str;
                    }};
        // search metadata
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::METADATA: {
                        auto meta = code.metadata();
                        auto str = ctx.metadata_table[meta->name.value()];
                        // handle metadata...
                        break;
                    }
                    case rebgn::AbstractOp::DEFINE_ENUM: {
                        auto range = ctx.range(code.ident().value());
                        auto ident = ctx.ident(code.ident().value());
                        auto& vec = ctx.enum_map[ident];
                        for (size_t k = range.start; k < range.end; k++) {
                            auto& code = ctx.bm.code[k];
                            if (code.op == rebgn::AbstractOp::DEFINE_ENUM_MEMBER) {
                                auto ident = ctx.ident(code.ident().value());
                                auto evaluated = eval(ctx.ref(code.left_ref().value()), ctx);
                                if (!evaluated.back().value) {
                                    ctx.output["error"] = evaluated.back().value.error().error();
                                    dump_json(ctx);
                                    return;
                                }
                                auto value = evaluated.back().value.value()->as_int();
                                if (!value) {
                                    ctx.output["error"] = value.error().error();
                                    dump_json(ctx);
                                    return;
                                }
                                vec.push_back({*value, ident});
                            }
                        }
                    }
                }
            }
        }
        for (size_t i = 0; i < bm.ident_ranges.ranges.size(); i++) {
            auto& range = bm.ident_ranges.ranges[i];
            auto& code = bm.code[range.range.start.value()];
            if (code.op != rebgn::AbstractOp::DEFINE_FORMAT) {
                continue;
            }
            auto found = find_op(ctx, rebgn::Range{.start = range.range.start.value(), .end = range.range.end.value()}, rebgn::AbstractOp::DEFINE_DECODER);
            if (!found) {
                continue;
            }
            auto func_ref = bm.code[*found].right_ref().value();
            auto name = ctx.ident_table[code.ident()->value()];
            ctx.format_table[name] = func_ref;
        }
        futils::json::JSON json;
        ctx.output["format"] = flags.start_format_name;
        auto found = ctx.format_table.find(flags.start_format_name);
        if (found == ctx.format_table.end()) {
            ctx.output["error"] = "format not found";
            dump_json(ctx);
            return;
        }
        auto format_ref = found->second;
        auto func_range = ctx.range(format_ref);
        futils::binary::reader r{flags.input_binary};
        ctx.enter_stack(rebgn::AbstractOp::DEFINE_PROGRAM, 0, ctx.bm.code.size());
        inner_function(ctx, r, func_range);
        dump_json(ctx);
    }
}  // namespace bm2hexmap
