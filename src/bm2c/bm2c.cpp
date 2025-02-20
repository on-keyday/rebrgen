/*license*/
#include <bm2/context.hpp>
#include <bm/helper.hpp>
#include "bm2c.hpp"
namespace bm2c {
    using TmpCodeWriter = bm2::TmpCodeWriter;
    struct Context : bm2::Context {
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
    std::vector<std::string> eval(const rebgn::Code& code, Context& ctx) {
        std::vector<std::string> result;
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
                result.push_back(std::format("({}){}", type_str, evaluated.back()));
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
                result.push_back(std::format("({} {} {})", left_eval.back(), opstr, right_eval.back()));
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                auto op = code.uop().value();
                auto ref = code.ref().value();
                auto target = eval(ctx.ref(ref), ctx);
                auto opstr = to_string(op);
                result.push_back(std::format("({}{})", opstr, target.back()));
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);
                result.push_back(std::format("{} = {}", left_eval.back(), right_eval.back()));
                result.push_back(left_eval.back());
                break;
            }
            case rebgn::AbstractOp::ACCESS: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_ident = ctx.ident(right_ref);
                result.push_back(std::format("{}.{}", left_eval.back(), right_ident));
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                result.insert(result.end(), right_eval.begin(), right_eval.end() - 1);
                result.push_back(std::format("{}[{}]", left_eval.back(), right_eval.back()));
                break;
            }
            case rebgn::AbstractOp::CALL: {
                result.push_back("/*Unimplemented CALL*/");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TRUE: {
                result.push_back("true");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_FALSE: {
                result.push_back("false");
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT: {
                result.push_back(std::format("{}", code.int_value()->value()));
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT64: {
                result.push_back(std::format("{}", *code.int_value64()));
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
                result.push_back(std::format("{}.size()", vector_eval.back()));
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
                    ctx.this_as.push_back(left_eval.back());
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
                    w.writeln(ident, " = ", evaluated.back(), ",");
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
    void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
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
                    w.writeln("/*Unimplemented ENCODE_INT*/ ");
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT: {
                    w.writeln("/*Unimplemented DECODE_INT*/ ");
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
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR_FIXED*/ ");
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
                    w.writeln("while (", evaluated.back(), ") {");
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
                        w.writeln(std::format("/*Unimplemented {}*/", to_string(code.op)));
                    }
                    break;
                }
            }
        }
    }
    void to_c(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {
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
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            /* exclude DEFINE_PROGRAM and END_PROGRAM */
            TmpCodeWriter w;
            inner_block(ctx, w, rebgn::Range{.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});
            ctx.cw.write_unformatted(w.out());
        }
        for (auto& def : ctx.on_functions) {
            def.execute();
        }
        for (size_t i = 0; i < bm.ident_ranges.ranges.size(); i++) {
            auto& range = bm.ident_ranges.ranges[i];
            auto& code = bm.code[range.range.start.value()];
            if (code.op != rebgn::AbstractOp::DEFINE_FUNCTION) {
                continue;
            }
            TmpCodeWriter w;
            inner_function(ctx, w, rebgn::Range{.start = range.range.start.value(), .end = range.range.end.value()});
            ctx.cw.write_unformatted(w.out());
        }
    }
}  // namespace bm2c
