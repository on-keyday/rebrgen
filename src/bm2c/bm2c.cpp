/*license*/
#include <bm2/context.hpp>
#include <bm/helper.hpp>
#include "bm2c.hpp"
namespace bm2c {
    using TmpCodeWriter = bm2::TmpCodeWriter;
    struct Context : bm2::Context {
        Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,"r","w","(*this)", std::move(escape_ident)} {}
    };
    struct EvalResult {
        std::string result;
    };
    EvalResult make_eval_result(std::string result) {
        return EvalResult{std::move(result)};
    }
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
    EvalResult field_accessor(const rebgn::Code& code, Context& ctx) {
        EvalResult result;
        switch(code.op) {
        case rebgn::AbstractOp::DEFINE_FORMAT: {
            result = make_eval_result(ctx.this_());
            break;
        }
        case rebgn::AbstractOp::DEFINE_FIELD: {
            auto belong = code.belong().value();
            if(belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx);
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ctx.ident(code.ident().value())));
            }
            else {
                result = make_eval_result(ctx.ident(code.ident().value()));
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_UNION: {
            auto belong = code.belong().value();
            if(belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx);
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ctx.ident(code.ident().value())));
            }
            else {
                result = make_eval_result(ctx.ident(code.ident().value()));
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
            auto belong = code.belong().value();
            if(belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx);
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ctx.ident(code.ident().value())));
            }
            else {
                result = make_eval_result(ctx.ident(code.ident().value()));
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_STATE: {
            result = make_eval_result(ctx.this_());
            break;
        }
        case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
            auto belong = code.belong().value();
            if(belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx);
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ctx.ident(code.ident().value())));
            }
            else {
                result = make_eval_result(ctx.ident(code.ident().value()));
            }
            break;
        }
        default: {
            result = make_eval_result(std::format("/*Unimplemented {}*/", to_string(code.op)));
            break;
        }
        }
    return result;
    }
    EvalResult eval(const rebgn::Code& code, Context& ctx) {
        EvalResult result;
        switch(code.op) {
        case rebgn::AbstractOp::DEFINE_FIELD: {
            result = field_accessor(code, ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_PROPERTY: {
            result = make_eval_result("/*Unimplemented DEFINE_PROPERTY*/");
            break;
        }
        case rebgn::AbstractOp::DEFINE_PARAMETER: {
            result = make_eval_result("/*Unimplemented DEFINE_PARAMETER*/");
            break;
        }
        case rebgn::AbstractOp::INPUT_BYTE_OFFSET: {
            result = make_eval_result("/*Unimplemented INPUT_BYTE_OFFSET*/");
            break;
        }
        case rebgn::AbstractOp::OUTPUT_BYTE_OFFSET: {
            result = make_eval_result("/*Unimplemented OUTPUT_BYTE_OFFSET*/");
            break;
        }
        case rebgn::AbstractOp::INPUT_BIT_OFFSET: {
            result = make_eval_result("/*Unimplemented INPUT_BIT_OFFSET*/");
            break;
        }
        case rebgn::AbstractOp::OUTPUT_BIT_OFFSET: {
            result = make_eval_result("/*Unimplemented OUTPUT_BIT_OFFSET*/");
            break;
        }
        case rebgn::AbstractOp::REMAIN_BYTES: {
            result = make_eval_result("/*Unimplemented REMAIN_BYTES*/");
            break;
        }
        case rebgn::AbstractOp::CAN_READ: {
            result = make_eval_result("/*Unimplemented CAN_READ*/");
            break;
        }
        case rebgn::AbstractOp::IS_LITTLE_ENDIAN: {
            result = make_eval_result("/*Unimplemented IS_LITTLE_ENDIAN*/");
            break;
        }
        case rebgn::AbstractOp::CAST: {
            auto type = code.type().value();
            auto ref = code.ref().value();
            auto type_str = type_to_string(ctx, type);
            auto evaluated = eval(ctx.ref(ref), ctx);
            result = make_eval_result(std::format("({}){}", type_str, evaluated.result));
            break;
        }
        case rebgn::AbstractOp::CALL_CAST: {
            result = make_eval_result("/*Unimplemented CALL_CAST*/");
            break;
        }
        case rebgn::AbstractOp::ADDRESS_OF: {
            result = make_eval_result("/*Unimplemented ADDRESS_OF*/");
            break;
        }
        case rebgn::AbstractOp::OPTIONAL_OF: {
            result = make_eval_result("/*Unimplemented OPTIONAL_OF*/");
            break;
        }
        case rebgn::AbstractOp::EMPTY_PTR: {
            result = make_eval_result("/*Unimplemented EMPTY_PTR*/");
            break;
        }
        case rebgn::AbstractOp::EMPTY_OPTIONAL: {
            result = make_eval_result("/*Unimplemented EMPTY_OPTIONAL*/");
            break;
        }
        case rebgn::AbstractOp::DEFINE_VARIABLE: {
            auto ident = ctx.ident(code.ident().value());
            result = make_eval_result(ident);
            break;
        }
        case rebgn::AbstractOp::DEFINE_VARIABLE_REF: {
            auto ref=code.ref().value();
            return eval(ctx.ref(ref), ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_CONSTANT: {
            result = make_eval_result("/*Unimplemented DEFINE_CONSTANT*/");
            break;
        }
        case rebgn::AbstractOp::DECLARE_VARIABLE: {
            auto ref=code.ref().value();
            return eval(ctx.ref(ref), ctx);
            break;
        }
        case rebgn::AbstractOp::BINARY: {
            auto op = code.bop().value();
            auto left_ref = code.left_ref().value();
            auto right_ref = code.right_ref().value();
            auto left_eval = eval(ctx.ref(left_ref), ctx);
            auto right_eval = eval(ctx.ref(right_ref), ctx);
            auto opstr = to_string(op);
            result = make_eval_result(std::format("({} {} {})", left_eval.result, opstr, right_eval.result));
            break;
        }
        case rebgn::AbstractOp::UNARY: {
            auto op = code.uop().value();
            auto ref = code.ref().value();
            auto target = eval(ctx.ref(ref), ctx);
            auto opstr = to_string(op);
            result = make_eval_result(std::format("({}{})", opstr, target.result));
            break;
        }
        case rebgn::AbstractOp::ASSIGN: {
            result = make_eval_result("/*Unimplemented ASSIGN*/");
            break;
        }
        case rebgn::AbstractOp::ACCESS: {
            auto left_ref = code.left_ref().value();
            auto right_ref = code.right_ref().value();
            auto left_eval = eval(ctx.ref(left_ref), ctx);
            auto right_ident = ctx.ident(right_ref);
            result = make_eval_result(std::format("{}.{}", left_eval.result, right_ident));
            break;
        }
        case rebgn::AbstractOp::INDEX: {
            auto left_ref = code.left_ref().value();
            auto right_ref = code.right_ref().value();
            auto left_eval = eval(ctx.ref(left_ref), ctx);
            auto right_eval = eval(ctx.ref(right_ref), ctx);
            result = make_eval_result(std::format("{}[{}]", left_eval.result, right_eval.result));
            break;
        }
        case rebgn::AbstractOp::CALL: {
            result = make_eval_result("/*Unimplemented CALL*/");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_TRUE: {
            result = make_eval_result("true");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_FALSE: {
            result = make_eval_result("false");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_INT: {
            result = make_eval_result(std::format("{}", code.int_value()->value()));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_INT64: {
            result = make_eval_result(std::format("{}", *code.int_value64()));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_CHAR: {
            auto char_code = code.int_value()->value();
            result = make_eval_result("/*Unimplemented IMMEDIATE_CHAR*/");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_STRING: {
            result = make_eval_result("/*Unimplemented IMMEDIATE_STRING*/");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_TYPE: {
            auto type = code.type().value();
            result = make_eval_result(type_to_string(ctx, type));
            break;
        }
        case rebgn::AbstractOp::NEW_OBJECT: {
            result = make_eval_result("/*Unimplemented NEW_OBJECT*/");
            break;
        }
        case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
            auto ident = ctx.ident(code.ident().value());
            result = make_eval_result(ident);
            break;
        }
        case rebgn::AbstractOp::ARRAY_SIZE: {
            auto vector_ref = code.ref().value();
            auto vector_eval = eval(ctx.ref(vector_ref), ctx);
            result = make_eval_result(std::format("{}.size()", vector_eval.result));
            break;
        }
        case rebgn::AbstractOp::FIELD_AVAILABLE: {
            auto left_ref = code.left_ref().value();
            if(left_ref.value() == 0) {
                auto right_ref = code.right_ref().value();
                result = eval(ctx.ref(right_ref), ctx);
            }
            else {
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                ctx.this_as.push_back(left_eval.result);
                auto right_ref = code.right_ref().value();
                result = eval(ctx.ref(right_ref), ctx);
                ctx.this_as.pop_back();
            }
            break;
        }
        case rebgn::AbstractOp::PHI: {
            auto ref=code.ref().value();
            return eval(ctx.ref(ref), ctx);
            break;
        }
        default: {
            result = make_eval_result(std::format("/*Unimplemented {}*/", to_string(code.op)));
            break;
        }
        }return result;
    }
    void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        size_t params = 0;
        for(size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch(code.op) {
                case rebgn::AbstractOp::DEFINE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto type = type_to_string(ctx,code.type().value());
                    auto ident = ctx.ident(ref);
                    w.write(type, " ", ident);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented ENCODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.writeln("/*Unimplemented DECODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ref().value();
                    auto type = type_to_string(ctx,ctx.ref(ref).type().value());
                    auto ident = ctx.ident(ref);
                    w.write(type, " ", ident);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto type = type_to_string(ctx,code.type().value());
                    auto ident = ctx.ident(ref);
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
        for(size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch(code.op) {
                case rebgn::AbstractOp::DEFINE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("/*Unimplemented DEFINE_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("/*Unimplemented ENCODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("/*Unimplemented DECODER_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("/*Unimplemented STATE_VARIABLE_PARAMETER*/ ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ref = code.ident().value();
                    auto ident = ctx.ident(ref);
                    w.write(ident);
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
        for(size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch(code.op) {
            case rebgn::AbstractOp::DEFINE_FORMAT: {
                auto ident = ctx.ident(code.ident().value());
                w.writeln("struct ", ident, " {");
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
                w.writeln("enum ", ident, " {");
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
                w.writeln(ident, " = ", evaluated.result, ",");
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
                w.writeln("struct ", ident, " {");
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
                if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {
                    w.writeln(std::format("/*Unimplemented {}*/", to_string(code.op)));
                }
                break;
            }
            }
        }
    }
    void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for(size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch(code.op) {
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
                    auto found_type_pos = find_op(ctx,range,rebgn::AbstractOp::RETURN_TYPE);
                    if(!found_type_pos) {
                    w.writeln("void");
                    }
                    else {
                    auto type = type_to_string(ctx, ctx.bm.code[*found_type_pos].type().value());
                    w.write(type);
                    }
                    w.write(" ", ident, "(");
                    add_parameter(ctx, w, range);
                    w.writeln(") {");
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
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented ENCODE_INT*/");
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented DECODE_INT*/");
                }
                break;
            }
            case rebgn::AbstractOp::ENCODE_INT_VECTOR: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented ENCODE_INT_VECTOR*/");
                }
                break;
            }
            case rebgn::AbstractOp::ENCODE_INT_VECTOR_FIXED: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented ENCODE_INT_VECTOR_FIXED*/");
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR*/");
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR_UNTIL_EOF*/");
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED: {
                auto fallback = code.fallback().value();
                if(fallback.value() != 0) {
                    auto range = ctx.range(fallback);
                    inner_function(ctx, w, range);
                }
                else {
                    w.writeln("/*Unimplemented DECODE_INT_VECTOR_FIXED*/");
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
                    w.writeln("while (",evaluated.result,") {");
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
                    w.writeln("if (",evaluated.result,") {");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::ELIF: {
                auto ref = code.ref().value();
                auto evaluated = eval(ctx.ref(ref), ctx);
                defer.pop_back();
                    w.writeln("}");
                    w.writeln("else if (",evaluated.result,") {");
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
                auto ident = ctx.ident(code.ident().value());
                auto ref = code.ref().value();
                auto type = type_to_string(ctx,code.type().value());
                auto evaluated = eval(ctx.ref(ref), ctx);
                w.writeln(std::format("{} {} = {};",type, ident, evaluated.result));
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                w.writeln("/*Unimplemented DEFINE_CONSTANT*/ ");
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_ref = code.left_ref().value();
                auto right_ref = code.right_ref().value();
                auto left_eval = eval(ctx.ref(left_ref), ctx);
                auto right_eval = eval(ctx.ref(right_ref), ctx);
                w.writeln("", left_eval.result, " = ", right_eval.result, ";");
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
                auto evaluated = eval(ctx.ref(param.refs[0]), ctx);
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
                auto ref = code.ref().value();
                auto evaluated = eval(ctx.ref(ref), ctx);
                w.writeln(evaluated.result, "+= 1;");
            }
            case rebgn::AbstractOp::RET: {
                auto ref = code.ref().value();
                if(ref.value() != 0) {
                    auto evaluated = eval(ctx.ref(ref), ctx);
                    w.writeln("return ", evaluated.result, ";");
                }
                else {
                    w.writeln("return;");
                }
                break;
            }
            case rebgn::AbstractOp::RET_SUCCESS: {
                w.writeln("return true;");
                break;
            }
            case rebgn::AbstractOp::RET_PROPERTY_SETTER_OK: {
                w.writeln("return true;");
                break;
            }
            case rebgn::AbstractOp::RET_PROPERTY_SETTER_FAIL: {
                w.writeln("return false;");
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
            case rebgn::AbstractOp::SEEK_ENCODER: {
                w.writeln("/*Unimplemented SEEK_ENCODER*/ ");
                break;
            }
            case rebgn::AbstractOp::SEEK_DECODER: {
                w.writeln("/*Unimplemented SEEK_DECODER*/ ");
                break;
            }
            case rebgn::AbstractOp::PROPERTY_FUNCTION: {
                w.writeln("/*Unimplemented PROPERTY_FUNCTION*/ ");
                break;
            }
            default: {
                if (!rebgn::is_marker(code.op)&&!rebgn::is_struct_define_related(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {
                    w.writeln(std::format("/*Unimplemented {}*/", to_string(code.op)));
                }
                break;
            }
            }
        }
    }
    std::string escape_c_keyword(const std::string& str) {
        if (str == "auto" || str == "break" || str == "case" || str == "char" || str == "const" || str == "continue" || str == "default" || str == "do" || str == "double" || str == "else" || str == "enum" || str == "extern" || str == "float" || str == "for" || str == "goto" || str == "if" || str == "inline" || str == "int" || str == "long" || str == "register" || str == "restrict" || str == "return" || str == "short" || str == "signed" || str == "sizeof" || str == "static" || str == "struct" || str == "switch" || str == "typedef" || str == "union" || str == "unsigned" || str == "void" || str == "volatile" || str == "while" || str == "_Alignas" || str == "_Alignof" || str == "_Atomic" || str == "_Bool" || str == "_Complex" || str == "_Generic" || str == "_Imaginary" || str == "_Noreturn" || str == "_Static_assert" || str == "_Thread_local") {
            return str + "_";
        }
        return str;
    }
    void to_c(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {
        Context ctx{w, bm, [&](bm2::Context& ctx, std::uint64_t id, auto&& str) {
            return escape_c_keyword(str);
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
            inner_function(ctx, w, rebgn::Range{.start = range.range.start.value() , .end = range.range.end.value()});
            ctx.cw.write_unformatted(w.out());
        }
    }
}  // namespace bm2c
