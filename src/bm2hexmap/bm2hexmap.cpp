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
        std::variant<std::monostate, std::uint64_t, std::vector<std::shared_ptr<Value>>,
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
    };

    struct Context : bm2::Context {
        futils::view::rvec input_binary;
        json::JSON output;

        std::vector<OffsetRange> offset_ranges;

        using Map = std::unordered_map<std::string, std::shared_ptr<Value>>;

        Map variables;

        Map self;

        std::vector<Map> self_stack;

        // map identifier to format name
        std::unordered_map<std::string, rebgn::Varint> format_table;
        Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident)
            : bm2::Context{w, bm, "r", "w", "(*this)", std::move(escape_ident)} {}
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

    std::shared_ptr<Value> new_array() {
        auto v = std::make_shared<Value>();
        v->value = std::vector<std::shared_ptr<Value>>();
        return v;
    }

    std::shared_ptr<Value> new_map() {
        auto v = std::make_shared<Value>();
        v->value = std::unordered_map<std::string, std::shared_ptr<Value>>();
        return v;
    }

    struct EvalResult {
        expected<std::shared_ptr<Value>> value;
        std::string repr;

        EvalResult() = default;

        EvalResult(const char* repr)
            : repr(repr) {}

        EvalResult(std::string repr)
            : repr(repr) {}

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
                result.push_back("/*Unimplemented DEFINE_FIELD*/");
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
                result.push_back("/*Unimplemented INPUT_BYTE_OFFSET*/");
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
                result.push_back("/*Unimplemented CAN_READ*/");
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
                result.push_back(std::format("({}){}", type_str, evaluated.back().repr));
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
                auto ref = code.ref().value();
                auto type = type_to_string(ctx, code.type().value());
                auto evaluated = eval(ctx.ref(ref), ctx);
                result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);
                result.push_back(std::format("{} {} = {}", type, ident, evaluated.back()));
                result.push_back(ident);
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
                auto repr = std::format("({} {} {})", left_eval.back(), opstr, right_eval.back());
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
                                    return size_t(left && right);
                                case rebgn::BinaryOp::logical_or:
                                    return size_t(left || right);
                                case rebgn::BinaryOp::equal:
                                    return size_t(left == right);
                                case rebgn::BinaryOp::not_equal:
                                    return size_t(left != right);
                                case rebgn::BinaryOp::less:
                                    return size_t(left < right);
                                case rebgn::BinaryOp::less_or_eq:
                                    return size_t(left <= right);
                                case rebgn::BinaryOp::grater:
                                    return size_t(left > right);
                                case rebgn::BinaryOp::grater_or_eq:
                                    return size_t(left >= right);
                                default:
                                    return unexpect_error("Unhandled binary op: {}", to_string(op));
                            }
                        }());
                    };
                };
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                auto op = code.uop().value();
                auto ref = code.ref().value();
                auto target = eval(ctx.ref(ref), ctx);
                auto opstr = to_string(op);
                auto repr = std::format("({}{})", opstr, target.back());
                auto value = target.back().value & [&](auto v) {
                    return new_int(v->as_int() & [&](auto i) {
                        switch (op) {
                            case rebgn::UnaryOp::minus_sign:
                                return -i;
                            case rebgn::UnaryOp::bit_not:
                                return ~i;
                            case rebgn::UnaryOp::logical_not:
                                return !i ? size_t(1) : size_t(0);
                            default:
                                return i;
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
                auto value = left_eval.back().value & [&](auto v) {
                    return v->index(right_ident);
                };
                result.push_back(EvalResult{std::move(value), repr});
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
                result.push_back("/*Unimplemented NEW_OBJECT*/");
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
                auto repr = std::format("{}.size()", vector_eval.back());
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
                    auto ident = ctx.ident(code.ident().value());
                    w.writeln(type, " ", ident, ";");
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
            return true;
        }
        ctx.output["error"] = "bit size not multiple of 8";
        return false;
    }

    void inner_function(Context& ctx, futils::binary::reader& r, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        TmpCodeWriter w;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
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
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_FUNCTION: {
                    defer.pop_back();
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
                    w.writeln("/*Unimplemented CALL_DECODE*/ ");
                    break;
                }
                case rebgn::AbstractOp::LOOP_INFINITE: {
                    w.writeln("for(;;) {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::LOOP_CONDITION: {
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    w.writeln("while (", evaluated.back().repr, ") {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::CONTINUE: {
                    w.writeln("continue;");
                    break;
                }
                case rebgn::AbstractOp::BREAK: {
                    w.writeln("break;");
                    break;
                }
                case rebgn::AbstractOp::END_LOOP: {
                    defer.pop_back();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::IF: {
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    w.writeln("if (", evaluated.back(), ") {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELIF: {
                    auto ref = code.ref().value();
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    defer.pop_back();
                    w.writeln("}");
                    w.writeln("else if (", evaluated.back(), ") {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELSE: {
                    defer.pop_back();
                    w.writeln("}");
                    w.writeln("else {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_IF: {
                    defer.pop_back();
                    w.writeln("}");
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
                    auto evaluated = eval(code, ctx);
                    w.writeln(evaluated[evaluated.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::DEFINE_CONSTANT: {
                    w.writeln("/*Unimplemented DEFINE_CONSTANT*/ ");
                    break;
                }
                case rebgn::AbstractOp::ASSIGN: {
                    auto evaluated = eval(code, ctx);
                    w.writeln(evaluated[evaluated.size() - 2]);
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
                    w.writeln("/*Unimplemented APPEND*/");
                    break;
                }
                case rebgn::AbstractOp::INC: {
                    w.writeln("/*Unimplemented INC*/ ");
                    break;
                }
                case rebgn::AbstractOp::RET: {
                    auto ref = code.ref().value();
                    if (ref.value() != 0) {
                        auto evaluated = eval(ctx.ref(ref), ctx);
                        w.writeln("return ", evaluated.back(), ";");
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

    void dump_json(Context& ctx) {
        std::vector<json::JSON> offset_ranges;
        for (auto& range : ctx.offset_ranges) {
            json::JSON json;
            json["start"] = range.start;
            json["end"] = range.end;
            json["related_field"] = related_field_to_string(ctx, range.related_field);
            std::string related_data_base64;
            futils::base64::encode(range.related_data, related_data_base64);
            json["related_data"] = related_data_base64;
            json["interpreted_value"] = range.interpreted_value;
            offset_ranges.push_back(json);
        }
        ctx.output["offset_ranges"] = std::move(offset_ranges);
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
        inner_function(ctx, r, func_range);
        dump_json(ctx);
    }
}  // namespace bm2hexmap
