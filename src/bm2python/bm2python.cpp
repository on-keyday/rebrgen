/*license*/
#include <bm2/context.hpp>
#include <bmgen/helper.hpp>
#include <escape/escape.h>
#include "bm2python.hpp"
namespace bm2python {
    using TmpCodeWriter = bm2::TmpCodeWriter;
    struct Context : bm2::Context {
        Context(::futils::binary::writer& w, const rebgn::BinaryModule& bm, auto&& escape_ident) : bm2::Context{w, bm,"r","w","self", std::move(escape_ident)} {}
    };
    struct EvalResult {
        std::string result;
    };
    EvalResult make_eval_result(std::string result) {
        return EvalResult{std::move(result)};
    }
    void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);
    void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);
    void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range);
    void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range);
    EvalResult field_accessor(const rebgn::Code& code, Context& ctx);
    std::string type_accessor(const rebgn::Code& code, Context& ctx);
    EvalResult eval(const rebgn::Code& code, Context& ctx);
    std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0);
    std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref,size_t* bit_size = nullptr);
    std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size, size_t index) {
        if (s.storages.size() <= index) {
            return "\"\"\"type index overflow\"\"\"";
        }
        auto& storage = s.storages[index];
        switch (storage.type) {
            case rebgn::StorageType::INT: {
                auto size = storage.size()->value(); //bit size
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 8) {
                    return "int";
                }
                else if (size <= 16) {
                    return "int";
                }
                else if (size <= 32) {
                    return "int";
                }
                else {
                    return "int";
                }
            }
            case rebgn::StorageType::UINT: {
                auto size = storage.size()->value(); //bit size
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 8) {
                    return "int";
                }
                else if (size <= 16) {
                    return "int";
                }
                else if (size <= 32) {
                    return "int";
                }
                else {
                    return "int";
                }
            }
            case rebgn::StorageType::FLOAT: {
                auto size = storage.size()->value(); //bit size
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 32) {
                    return "float32_t";
                }
                else {
                    return "float64_t";
                }
            }
            case rebgn::StorageType::STRUCT_REF: {
                auto ident_ref = storage.ref().value(); //reference of struct
                auto ident = ctx.ident(ident_ref); //identifier of struct
                return ident;
            }
            case rebgn::StorageType::RECURSIVE_STRUCT_REF: {
                auto ident_ref = storage.ref().value(); //reference of recursive struct
                auto ident = ctx.ident(ident_ref); //identifier of recursive struct
                return std::format("{}", ident);
            }
            case rebgn::StorageType::BOOL: {
                return "bool";
            }
            case rebgn::StorageType::ENUM: {
                auto ident_ref = storage.ref().value(); //reference of enum
                auto ident = ctx.ident(ident_ref); //identifier of enum
                return ident;
            }
            case rebgn::StorageType::ARRAY: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1); //base type
                auto is_byte_vector = index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8; //is byte vector
                auto length = storage.size()->value(); //array length
                if (is_byte_vector) {
                    return futils::strutil::concat<std::string>("bytearray");
                }
                return futils::strutil::concat<std::string>("list[",base_type,"]");
            }
            case rebgn::StorageType::VECTOR: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1); //base type
                auto is_byte_vector = index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8; //is byte vector
                if (is_byte_vector) {
                    return "bytearray";
                }
                return std::format("list[{}]", base_type);
            }
            case rebgn::StorageType::VARIANT: {
                auto ident_ref = storage.ref().value(); //reference of variant
                auto ident = ctx.ident(ident_ref); //identifier of variant
                std::vector<std::string> types = {}; //variant types
                for (size_t i = index + 1; i < s.storages.size(); i++) {
                    types.push_back(type_to_string_impl(ctx, s, bit_size, i));
                }
                std::string result;
                for (size_t i = 0; i < types.size(); i++) {
                    if (i != 0) {
                        result += " , ";
                    }
                    result += types[i];
                }
                return std::format("Union[{}]", result);
            }
            case rebgn::StorageType::CODER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::PROPERTY_SETTER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::OPTIONAL: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1); //base type
                return std::format("Optional[{}]", base_type);
            }
            case rebgn::StorageType::PTR: {
                auto base_type = type_to_string_impl(ctx, s, bit_size, index + 1); //base type
                return std::format("Optional[{}]", base_type);
            }
            default: {
                return std::format("{}{}{}","\"\"\"",to_string(storage.type),"\"\"\"");
            }
        }
    }
    std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref,size_t* bit_size) {
        auto& storage = ctx.storage_table[ref.ref.value()];
        return type_to_string_impl(ctx, storage, bit_size);
    }
    EvalResult field_accessor(const rebgn::Code& code, Context& ctx) {
        EvalResult result;
        switch(code.op) {
        case rebgn::AbstractOp::DEFINE_FORMAT: {
            result = make_eval_result(ctx.this_());
            break;
        }
        case rebgn::AbstractOp::DEFINE_FIELD: {
            auto ident_ref = code.ident().value(); //reference of FIELD
            auto ident = ctx.ident(ident_ref); //identifier of FIELD
            auto belong = code.belong().value(); //reference of belong
            auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
            if(is_member) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx); //belong eval
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ident));
            }
            else {
                result = make_eval_result(ident);
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_PROPERTY: {
            auto ident_ref = code.ident().value(); //reference of PROPERTY
            auto ident = ctx.ident(ident_ref); //identifier of PROPERTY
            auto belong = code.belong().value(); //reference of belong
            auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
            if(is_member) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx); //belong eval
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ident));
            }
            else {
                result = make_eval_result(ident);
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_UNION: {
            auto ident_ref = code.ident().value(); //reference of UNION
            auto ident = ctx.ident(ident_ref); //identifier of UNION
            auto belong = code.belong().value(); //reference of belong
            auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
            if(is_member) {
                auto belong_eval = field_accessor(ctx.ref(belong), ctx); //belong eval
                result = make_eval_result(std::format("{}.{}", belong_eval.result, ident));
            }
            else {
                result = make_eval_result(ident);
            }
            break;
        }
        case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
            auto ident_ref = code.ident().value(); //reference of UNION_MEMBER
            auto ident = ctx.ident(ident_ref); //identifier of UNION_MEMBER
            auto belong = code.belong().value(); //reference of belong
            auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
            auto union_member_ref = code.ident().value(); //reference of union member
            auto union_ref = belong; //reference of union
            auto union_field_ref = ctx.ref(union_ref).belong().value(); //reference of union field
            auto union_field_belong = ctx.ref(union_field_ref).belong().value(); //reference of union field belong
            result = field_accessor(ctx.ref(union_field_ref),ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_STATE: {
            result = make_eval_result(ctx.this_());
            break;
        }
        case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
            auto ident_ref = code.ident().value(); //reference of BIT_FIELD
            auto ident = ctx.ident(ident_ref); //identifier of BIT_FIELD
            auto belong = code.belong().value(); //reference of belong
            auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
            result = field_accessor(ctx.ref(belong),ctx);
            break;
        }
        default: {
            result = make_eval_result(std::format("{}{}{}","\"\"\"",to_string(code.op),"\"\"\""));
            break;
        }
        }
    return result;
    }
    std::string type_accessor(const rebgn::Code& code, Context& ctx) {
        std::string result;
        switch(code.op) {
        case rebgn::AbstractOp::DEFINE_FORMAT: {
        auto ident_ref = code.ident().value(); //reference of FORMAT
        auto ident = ctx.ident(ident_ref); //identifier of FORMAT
        result = ident;
        break;
        }
        case rebgn::AbstractOp::DEFINE_FIELD: {
        auto ident_ref = code.ident().value(); //reference of FIELD
        auto ident = ctx.ident(ident_ref); //identifier of FIELD
        auto belong = code.belong().value(); //reference of belong
        auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
        if(is_member) {
            auto belong_eval = type_accessor(ctx.ref(belong),ctx); //field accessor
            result = std::format("{}.{}", belong_eval, ident);
        }
        else {
            result = ident;
        }
        break;
        }
        case rebgn::AbstractOp::DEFINE_PROPERTY: {
        auto ident_ref = code.ident().value(); //reference of PROPERTY
        auto ident = ctx.ident(ident_ref); //identifier of PROPERTY
        auto belong = code.belong().value(); //reference of belong
        auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
        if(is_member) {
            auto belong_eval = type_accessor(ctx.ref(belong),ctx); //field accessor
            result = std::format("{}.{}", belong_eval, ident);
        }
        else {
            result = ident;
        }
        break;
        }
        case rebgn::AbstractOp::DEFINE_UNION: {
        auto ident_ref = code.ident().value(); //reference of UNION
        auto ident = ctx.ident(ident_ref); //identifier of UNION
        auto belong = code.belong().value(); //reference of belong
        auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
        if(is_member) {
            auto belong_eval = type_accessor(ctx.ref(belong),ctx); //field accessor
            result = std::format("{}.{}", belong_eval, ident);
        }
        else {
            result = ident;
        }
        break;
        }
        case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
        auto ident_ref = code.ident().value(); //reference of UNION_MEMBER
        auto ident = ctx.ident(ident_ref); //identifier of UNION_MEMBER
        auto belong = code.belong().value(); //reference of belong
        auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
        auto union_member_ref = code.ident().value(); //reference of union member
        auto union_ref = belong; //reference of union
        auto union_field_ref = ctx.ref(union_ref).belong().value(); //reference of union field
        auto union_field_belong = ctx.ref(union_field_ref).belong().value(); //reference of union field belong
        auto belong_type = type_accessor(ctx.ref(union_field_belong),ctx);
        result = std::format("{}.{}",belong_type,ident);
        break;
        }
        case rebgn::AbstractOp::DEFINE_STATE: {
        auto ident_ref = code.ident().value(); //reference of STATE
        auto ident = ctx.ident(ident_ref); //identifier of STATE
        result = ident;
        break;
        }
        case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
        auto ident_ref = code.ident().value(); //reference of BIT_FIELD
        auto ident = ctx.ident(ident_ref); //identifier of BIT_FIELD
        auto belong = code.belong().value(); //reference of belong
        auto is_member = belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM; //is member of a struct
        result = type_accessor(ctx.ref(belong),ctx);
        break;
        }
        default: {
            return std::format("{}{}{}","\"\"\"",to_string(code.op),"\"\"\"");
        }
        }
    return result;
    }
    EvalResult eval(const rebgn::Code& code, Context& ctx) {
        EvalResult result;
        switch(code.op) {
        case rebgn::AbstractOp::DEFINE_FIELD: {
            result = field_accessor(code,ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_PROPERTY: {
            result = field_accessor(code,ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_PARAMETER: {
            auto ident_ref = code.ident().value(); //reference of variable value
            auto ident = ctx.ident(ident_ref); //identifier of variable value
            result = make_eval_result(ident);
            break;
        }
        case rebgn::AbstractOp::INPUT_BYTE_OFFSET: {
            result = make_eval_result(futils::strutil::concat<std::string>("",ctx.r(),".tell()"));
            break;
        }
        case rebgn::AbstractOp::OUTPUT_BYTE_OFFSET: {
            result = make_eval_result(futils::strutil::concat<std::string>("",ctx.w(),".tell()"));
            break;
        }
        case rebgn::AbstractOp::INPUT_BIT_OFFSET: {
            result = make_eval_result("\"\"\"Unimplemented INPUT_BIT_OFFSET\"\"\"");
            break;
        }
        case rebgn::AbstractOp::OUTPUT_BIT_OFFSET: {
            result = make_eval_result("\"\"\"Unimplemented OUTPUT_BIT_OFFSET\"\"\"");
            break;
        }
        case rebgn::AbstractOp::REMAIN_BYTES: {
            result = make_eval_result("\"\"\"Unimplemented REMAIN_BYTES\"\"\"");
            break;
        }
        case rebgn::AbstractOp::CAN_READ: {
            result = make_eval_result("\"\"\"Unimplemented CAN_READ\"\"\"");
            break;
        }
        case rebgn::AbstractOp::IS_LITTLE_ENDIAN: {
            auto fallback = code.fallback().value(); //reference of fallback expression
            if(fallback.value() != 0) {
                result = eval(ctx.ref(fallback), ctx);
            }
            else {
                result = make_eval_result("sys.byteorder == 'little'");
            }
            break;
        }
        case rebgn::AbstractOp::CAST: {
            auto type_ref = code.type().value(); //reference of cast target type
            auto type = type_to_string(ctx,type_ref); //cast target type
            auto from_type_ref = code.from_type().value(); //reference of cast source type
            auto evaluated_ref = code.ref().value(); //reference of cast source value
            auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //cast source value
            result = make_eval_result(std::format("{}({})", type, evaluated.result));
            break;
        }
        case rebgn::AbstractOp::CALL_CAST: {
            result = make_eval_result("\"\"\"Unimplemented CALL_CAST\"\"\"");
            break;
        }
        case rebgn::AbstractOp::ADDRESS_OF: {
            auto target_ref = code.ref().value(); //reference of target object
            auto target = eval(ctx.ref(target_ref), ctx); //target object
            result = make_eval_result(std::format("{}", target.result));
            break;
        }
        case rebgn::AbstractOp::OPTIONAL_OF: {
            auto target_ref = code.ref().value(); //reference of target object
            auto target = eval(ctx.ref(target_ref), ctx); //target object
            auto type_ref = code.type().value(); //reference of type of optional (not include optional)
            auto type = type_to_string(ctx,type_ref); //type of optional (not include optional)
            result = make_eval_result(std::format("{}", target.result));
            break;
        }
        case rebgn::AbstractOp::EMPTY_PTR: {
            result = make_eval_result("None");
            break;
        }
        case rebgn::AbstractOp::EMPTY_OPTIONAL: {
            result = make_eval_result("None");
            break;
        }
        case rebgn::AbstractOp::DEFINE_VARIABLE: {
            auto ident_ref = code.ident().value(); //reference of variable value
            auto ident = ctx.ident(ident_ref); //identifier of variable value
            result = make_eval_result(ident);
            break;
        }
        case rebgn::AbstractOp::DEFINE_VARIABLE_REF: {
            auto ref = code.ref().value(); //reference of expression
            result = eval(ctx.ref(ref), ctx);
            break;
        }
        case rebgn::AbstractOp::DEFINE_CONSTANT: {
            result = make_eval_result("\"\"\"Unimplemented DEFINE_CONSTANT\"\"\"");
            break;
        }
        case rebgn::AbstractOp::DECLARE_VARIABLE: {
            auto ref = code.ref().value(); //reference of expression
            result = eval(ctx.ref(ref), ctx);
            break;
        }
        case rebgn::AbstractOp::BINARY: {
            auto op = code.bop().value(); //binary operator
            auto left_eval_ref = code.left_ref().value(); //reference of left operand
            auto left_eval = eval(ctx.ref(left_eval_ref), ctx); //left operand
            auto right_eval_ref = code.right_ref().value(); //reference of right operand
            auto right_eval = eval(ctx.ref(right_eval_ref), ctx); //right operand
            auto opstr = to_string(op); //binary operator string
            if(op == rebgn::BinaryOp::logical_or) {
                opstr = "or";
            }
            else if(op == rebgn::BinaryOp::logical_and) {
                opstr = "and";
            }
            result = make_eval_result(std::format("({} {} {})", left_eval.result, opstr, right_eval.result));
            break;
        }
        case rebgn::AbstractOp::UNARY: {
            auto op = code.uop().value(); //unary operator
            auto target_ref = code.ref().value(); //reference of target
            auto target = eval(ctx.ref(target_ref), ctx); //target
            auto opstr = to_string(op); //unary operator string
            result = make_eval_result(std::format("({}{})", opstr, target.result));
            break;
        }
        case rebgn::AbstractOp::ASSIGN: {
            auto left_ref = code.left_ref().value(); //reference of assign target
            auto right_ref = code.right_ref().value(); //reference of assign source
            auto ref = code.ref().value(); //reference of previous assignment or phi or definition
            if(ref.value() != 0) {
                result = eval(ctx.ref(ref), ctx);
            }
            else {
                result = eval(ctx.ref(left_ref), ctx);
            }
            break;
        }
        case rebgn::AbstractOp::ACCESS: {
            auto left_eval_ref = code.left_ref().value(); //reference of left operand
            auto left_eval = eval(ctx.ref(left_eval_ref), ctx); //left operand
            auto right_ident_ref = code.right_ref().value(); //reference of right operand
            auto right_ident = ctx.ident(right_ident_ref); //identifier of right operand
            result = make_eval_result(std::format("{}.{}", left_eval.result, right_ident));
            break;
        }
        case rebgn::AbstractOp::INDEX: {
            auto left_eval_ref = code.left_ref().value(); //reference of indexed object
            auto left_eval = eval(ctx.ref(left_eval_ref), ctx); //indexed object
            auto right_eval_ref = code.right_ref().value(); //reference of index
            auto right_eval = eval(ctx.ref(right_eval_ref), ctx); //index
            result = make_eval_result(std::format("{}[{}]", left_eval.result, right_eval.result));
            break;
        }
        case rebgn::AbstractOp::CALL: {
            result = make_eval_result("\"\"\"Unimplemented CALL\"\"\"");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_TRUE: {
            result = make_eval_result("True");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_FALSE: {
            result = make_eval_result("False");
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_INT: {
            auto value = code.int_value()->value(); //immediate value
            result = make_eval_result(std::format("{}", value));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_INT64: {
            auto value = *code.int_value64(); //immediate value
            result = make_eval_result(std::format("{}", value));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_CHAR: {
            auto char_code = code.int_value()->value(); //immediate char code
            result = make_eval_result(std::format("{}", char_code));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_STRING: {
            auto str = ctx.string_table[code.ident().value().value()]; //immediate string
            result = make_eval_result(std::format("\"{}\"", futils::escape::escape_str<std::string>(str,futils::escape::EscapeFlag::hex,futils::escape::no_escape_set(),futils::escape::escape_all())));
            break;
        }
        case rebgn::AbstractOp::IMMEDIATE_TYPE: {
            auto type_ref = code.type().value(); //reference of immediate type
            auto type = type_to_string(ctx,type_ref); //immediate type
            result = make_eval_result(type);
            break;
        }
        case rebgn::AbstractOp::NEW_OBJECT: {
            auto type_ref = code.type().value(); //reference of object type
            auto type = type_to_string(ctx,type_ref); //object type
            result = make_eval_result(std::format("{}()", type));
            break;
        }
        case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
            auto ident_ref = code.ident().value(); //reference of variable value
            auto ident = ctx.ident(ident_ref); //identifier of variable value
            result = make_eval_result(ident);
            break;
        }
        case rebgn::AbstractOp::ARRAY_SIZE: {
            auto vector_eval_ref = code.ref().value(); //reference of array
            auto vector_eval = eval(ctx.ref(vector_eval_ref), ctx); //array
            result = make_eval_result(std::format("__builtins__.len({})", vector_eval.result));
            break;
        }
        case rebgn::AbstractOp::FIELD_AVAILABLE: {
            auto left_ref = code.left_ref().value(); //reference of field (maybe null)
            if(left_ref.value() == 0) {
                auto right_ref = code.right_ref().value(); //reference of condition
                result = eval(ctx.ref(right_ref), ctx);
            }
            else {
                auto left_eval = eval(ctx.ref(left_ref), ctx); //field
                ctx.this_as.push_back(left_eval.result);
                auto right_ref = code.right_ref().value(); //reference of condition
                result = eval(ctx.ref(right_ref), ctx);
                ctx.this_as.pop_back();
            }
            break;
        }
        case rebgn::AbstractOp::PHI: {
            auto ref = code.ref().value(); //reference of expression
            result = eval(ctx.ref(ref), ctx);
            break;
        }
        case rebgn::AbstractOp::BEGIN_COND_BLOCK: {
            auto ref = code.ref().value(); //reference of expression
            result = eval(ctx.ref(ref), ctx);
            break;
        }
        default: {
            result = make_eval_result(std::format("{}{}{}","\"\"\"",to_string(code.op),"\"\"\""));
            break;
        }
        }return result;
    }
    void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        size_t params = 0;
        auto belong = ctx.bm.code[range.start].belong().value();
        auto is_member = belong.value() != 0 && ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM;
        if(is_member) {
            w.write("self");
            params++;
        }
        for(size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch(code.op) {
                case rebgn::AbstractOp::DEFINE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ident_ref = code.ident().value(); //reference of parameter
                    auto ident = ctx.ident(ident_ref); //identifier of parameter
                    auto type_ref = code.type().value(); //reference of parameter type
                    auto type = type_to_string(ctx,type_ref); //parameter type
                    w.write(ident, " :", type);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("w :IO[bytes]");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write("r :IO[bytes]");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ident_ref = code.ref().value(); //reference of state variable
                    auto ident = ctx.ident(ident_ref); //identifier of state variable
                    auto type_ref = ctx.ref(ident_ref).type().value(); //reference of state variable type
                    auto type = type_to_string(ctx,type_ref); //state variable type
                    w.write(ident, " :", type);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ident_ref = code.ident().value(); //reference of parameter
                    auto ident = ctx.ident(ident_ref); //identifier of parameter
                    auto type_ref = code.type().value(); //reference of parameter type
                    auto type = type_to_string(ctx,type_ref); //parameter type
                    w.write(ident, " :", type);
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
                    w.write("\"\"\"Unimplemented DEFINE_PARAMETER\"\"\" ");
                    params++;
                    break;
                }
                case rebgn::AbstractOp::ENCODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write(ctx.w());
                    params++;
                    break;
                }
                case rebgn::AbstractOp::DECODER_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    w.write(ctx.r());
                    params++;
                    break;
                }
                case rebgn::AbstractOp::STATE_VARIABLE_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ident_ref = code.ref().value(); //reference of state variable
                    auto ident = ctx.ident(ident_ref); //identifier of state variable
                    w.write(ident);
                    params++;
                    break;
                }
                case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                    if(params > 0) {
                        w.write(", ");
                    }
                    auto ident_ref = code.ident().value(); //reference of parameter
                    auto ident = ctx.ident(ident_ref); //identifier of parameter
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
                auto ident_ref = code.ident().value(); //reference of format
                auto ident = ctx.ident(ident_ref); //identifier of format
                w.writeln("class ", ident, " :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_FORMAT: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DECLARE_FORMAT: {
                auto ref = code.ref().value(); //reference of FORMAT
                auto inner_range = ctx.range(code.ref().value()); //range of FORMAT
                inner_block(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD: {
                if (ctx.ref(code.belong().value()).op == rebgn::AbstractOp::DEFINE_PROGRAM) {
                    break;
                }
                auto type = type_to_string(ctx,code.type().value()); //field type
                auto ident_ref = code.ident().value(); //reference of field
                auto ident = ctx.ident(ident_ref); //identifier of field
                auto belong = code.belong().value(); //reference of belonging struct or bit field
                auto is_bit_field = belong.value()!=0&&ctx.ref(belong).op==rebgn::AbstractOp::DEFINE_BIT_FIELD; //is part of bit field
                w.writeln(ident, " :", type, "");
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                break;
            }
            case rebgn::AbstractOp::END_PROPERTY: {
                break;
            }
            case rebgn::AbstractOp::DECLARE_PROPERTY: {
                auto ref = code.ref().value(); //reference of PROPERTY
                auto inner_range = ctx.range(code.ref().value()); //range of PROPERTY
                inner_block(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY_SETTER: {
                auto func = code.right_ref().value(); //reference of function
                auto inner_range = ctx.range(func); //range of function
                inner_function(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY_GETTER: {
                auto func = code.right_ref().value(); //reference of function
                auto inner_range = ctx.range(func); //range of function
                inner_function(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DECLARE_FUNCTION: {
                auto ref = code.ref().value(); //reference of FUNCTION
                auto inner_range = ctx.range(code.ref().value()); //range of FUNCTION
                auto func_type = ctx.ref(ref).func_type().value(); //function type
                if(func_type != rebgn::FunctionType::BIT_GETTER &&
                   func_type != rebgn::FunctionType::BIT_SETTER) {
                    inner_function(ctx, w, inner_range);
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_ENUM: {
                auto ident_ref = code.ident().value(); //reference of enum
                auto ident = ctx.ident(ident_ref); //identifier of enum
                auto base_type_ref = code.type().value(); //type reference of enum base type
                std::optional<std::string> base_type = std::nullopt; //enum base type
                if(base_type_ref.ref.value() != 0) {
                    base_type = type_to_string(ctx,base_type_ref);
                }
                w.writeln("class ",ident,"(Enum):");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_ENUM: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DECLARE_ENUM: {
                auto ref = code.ref().value(); //reference of ENUM
                auto inner_range = ctx.range(code.ref().value()); //range of ENUM
                inner_block(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_ENUM_MEMBER: {
                auto ident_ref = code.ident().value(); //reference of enum member
                auto ident = ctx.ident(ident_ref); //identifier of enum member
                auto evaluated_ref = code.left_ref().value(); //reference of enum member value
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //enum member value
                auto enum_ident_ref = code.belong().value(); //reference of enum
                auto enum_ident = ctx.ident(enum_ident_ref); //identifier of enum
                w.writeln(ident, " = ", evaluated.result, "");
                break;
            }
            case rebgn::AbstractOp::DEFINE_UNION: {
                auto ident_ref = code.ident().value(); //reference of union
                auto ident = ctx.ident(ident_ref); //identifier of union
                break;
            }
            case rebgn::AbstractOp::END_UNION: {
                break;
            }
            case rebgn::AbstractOp::DECLARE_UNION: {
                auto ref = code.ref().value(); //reference of UNION
                auto inner_range = ctx.range(ref); //range of UNION
                inner_block(ctx,w,inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
                auto ident_ref = code.ident().value(); //reference of format
                auto ident = ctx.ident(ident_ref); //identifier of format
                w.writeln("class ", ident, " :");
                defer.push_back(w.indent_scope_ex());
                if(ctx.bm.code[i+1].op == rebgn::AbstractOp::END_UNION_MEMBER) {
                    w.writeln("pass");
                }
                break;
            }
            case rebgn::AbstractOp::END_UNION_MEMBER: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DECLARE_UNION_MEMBER: {
                auto ref = code.ref().value(); //reference of UNION_MEMBER
                auto inner_range = ctx.range(ref); //range of UNION_MEMBER
                inner_block(ctx,w,inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_STATE: {
                auto ident_ref = code.ident().value(); //reference of format
                auto ident = ctx.ident(ident_ref); //identifier of format
                w.writeln("class ", ident, " :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_STATE: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DECLARE_STATE: {
                auto ref = code.ref().value(); //reference of STATE
                auto inner_range = ctx.range(code.ref().value()); //range of STATE
                inner_block(ctx, w, inner_range);
                break;
            }
            case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
                auto type = type_to_string(ctx,code.type().value()); //bit field type
                auto ident_ref = code.ident().value(); //reference of bit field
                auto ident = ctx.ident(ident_ref); //identifier of bit field
                auto belong = code.belong().value(); //reference of belonging struct or bit field
                break;
            }
            case rebgn::AbstractOp::END_BIT_FIELD: {
                break;
            }
            case rebgn::AbstractOp::DECLARE_BIT_FIELD: {
                auto ref = code.ref().value(); //reference of bit field
                auto ident = ctx.ident(ref); //identifier of bit field
                auto inner_range = ctx.range(ref); //range of bit field
                auto type_ref = ctx.ref(ref).type().value(); //reference of bit field type
                auto type = type_to_string(ctx,type_ref); //bit field type
                inner_block(ctx,w,inner_range);
                break;
            }
            default: {
                if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {
                    w.writeln(std::format("{}{}{}","\"\"\"",to_string(code.op),"\"\"\""));
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
                w.writeln("\"\"\"Unimplemented METADATA\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::IMPORT: {
                w.writeln("\"\"\"Unimplemented IMPORT\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::DYNAMIC_ENDIAN: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    w.writeln("\"\"\"Unimplemented DYNAMIC_ENDIAN\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_FUNCTION: {
                auto ident_ref = code.ident().value(); //reference of function
                auto ident = ctx.ident(ident_ref); //identifier of function
                auto func_type = code.func_type().value(); //function type
                auto found_type_pos = find_op(ctx,range,rebgn::AbstractOp::RETURN_TYPE);
                std::optional<std::string> type = std::nullopt; //function return type
                if(found_type_pos) {
                    auto type_ref = ctx.bm.code[*found_type_pos].type().value();
                    type = type_to_string(ctx,type_ref);
                }
                std::optional<std::string> belong_name = std::nullopt; //function belong name
                if(auto belong = code.belong();belong&&belong->value()!=0) {
                    belong_name = ctx.ident(belong.value());
                }
                auto params = get_parameters(ctx,range);
                if(code.func_type().value() == rebgn::FunctionType::UNION_GETTER) {
                    if(params.size() == 0) {
                        w.writeln("@property");
                    }
                }
                else if(code.func_type().value() == rebgn::FunctionType::UNION_SETTER) {
                    if(params.size() == 1) {
                        w.writeln("@",ident,".setter");
                    }
                }
                w.write("def ");
                w.write(" ", ident, "(");
                add_parameter(ctx, w, range);
                w.write(") ");
                if(type) {
                    w.write("->", *type);
                }
                else {
                    w.write("");
                }
                w.writeln(":");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_FUNCTION: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    w.writeln("\"\"\"Unimplemented BEGIN_ENCODE_PACKED_OPERATION\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    w.writeln("\"\"\"Unimplemented END_ENCODE_PACKED_OPERATION\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    w.writeln("\"\"\"Unimplemented BEGIN_DECODE_PACKED_OPERATION\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::END_DECODE_PACKED_OPERATION: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    w.writeln("\"\"\"Unimplemented END_DECODE_PACKED_OPERATION\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::ENCODE_INT: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto evaluated_ref = code.ref().value(); //reference of element
                    auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //element
                    if(bit_size == 8) {
                        w.writeln("\"\"\"Unimplemented ENCODE_INT\"\"\"");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented ENCODE_INT\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto evaluated_ref = code.ref().value(); //reference of element
                    auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //element
                    if(bit_size == 8) {
                        w.writeln("\"\"\"Unimplemented DECODE_INT\"\"\"");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented DECODE_INT\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::ENCODE_INT_VECTOR: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto vector_value_ref = code.left_ref().value(); //reference of vector
                    auto vector_value = eval(ctx.ref(vector_value_ref), ctx); //vector
                    auto size_value_ref = code.right_ref().value(); //reference of size
                    auto size_value = eval(ctx.ref(size_value_ref), ctx); //size
                    if(bit_size == 8) {
                        w.writeln("w.write(",vector_value.result,"[:",size_value.result,"])");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented ENCODE_INT_VECTOR\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::ENCODE_INT_VECTOR_FIXED: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto vector_value_ref = code.left_ref().value(); //reference of vector
                    auto vector_value = eval(ctx.ref(vector_value_ref), ctx); //vector
                    auto size_value_ref = code.right_ref().value(); //reference of size
                    auto size_value = eval(ctx.ref(size_value_ref), ctx); //size
                    if(bit_size == 8) {
                        w.writeln("w.write(",vector_value.result,"[:",size_value.result,"])");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented ENCODE_INT_VECTOR_FIXED\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto vector_value_ref = code.left_ref().value(); //reference of vector
                    auto vector_value = eval(ctx.ref(vector_value_ref), ctx); //vector
                    auto size_value_ref = code.right_ref().value(); //reference of size
                    auto size_value = eval(ctx.ref(size_value_ref), ctx); //size
                    if(bit_size == 8) {
                        w.writeln("",vector_value.result," = ",ctx.r(),".read(",size_value.result,")");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented DECODE_INT_VECTOR\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto evaluated_ref = code.ref().value(); //reference of element
                    auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //element
                    if(bit_size == 8) {
                        w.writeln("",evaluated.result," = ",ctx.r(),".read()");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented DECODE_INT_VECTOR_UNTIL_EOF\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto vector_value_ref = code.left_ref().value(); //reference of vector
                    auto vector_value = eval(ctx.ref(vector_value_ref), ctx); //vector
                    auto size_value_ref = code.right_ref().value(); //reference of size
                    auto size_value = eval(ctx.ref(size_value_ref), ctx); //size
                    if(bit_size == 8) {
                        w.writeln("",vector_value.result," = ",ctx.r(),".read(",size_value.result,")");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented DECODE_INT_VECTOR_FIXED\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::PEEK_INT_VECTOR: {
                auto fallback = code.fallback().value(); //reference of fallback operation
                if(fallback.value() != 0) {
                    auto inner_range = ctx.range(fallback); //range of fallback operation
                    inner_function(ctx, w, inner_range);
                }
                else {
                    auto bit_size = code.bit_size()->value(); //bit size of element
                    auto endian = code.endian().value(); //endian of element
                    auto vector_value_ref = code.left_ref().value(); //reference of vector
                    auto vector_value = eval(ctx.ref(vector_value_ref), ctx); //vector
                    auto size_value_ref = code.right_ref().value(); //reference of size
                    auto size_value = eval(ctx.ref(size_value_ref), ctx); //size
                    if(bit_size == 8) {
                        w.writeln("",vector_value.result," = ",ctx.r(),".peek(",size_value.result,")");
                    }
                    else {
                        w.writeln("\"\"\"Unimplemented PEEK_INT_VECTOR\"\"\"");
                    }
                }
                break;
            }
            case rebgn::AbstractOp::BACKWARD_INPUT: {
                auto evaluated_ref = code.ref().value(); //reference of backward offset to move (in byte)
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //backward offset to move (in byte)
                w.writeln("",ctx.r(),".backward(",evaluated.result,")");
                break;
            }
            case rebgn::AbstractOp::BACKWARD_OUTPUT: {
                auto evaluated_ref = code.ref().value(); //reference of backward offset to move (in byte)
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //backward offset to move (in byte)
                w.writeln("",ctx.w(),".backward(",evaluated.result,")");
                break;
            }
            case rebgn::AbstractOp::CALL_ENCODE: {
                auto func_ref = code.left_ref().value(); //reference of function
                auto func_belong = ctx.ref(func_ref).belong().value(); //reference of function belong
                auto func_belong_name = type_accessor(ctx.ref(func_belong), ctx); //function belong name
                auto func_name = ctx.ident(func_ref); //identifier of function
                auto obj_eval_ref = code.right_ref().value(); //reference of `this` object
                auto obj_eval = eval(ctx.ref(obj_eval_ref), ctx); //`this` object
                auto inner_range = ctx.range(func_ref); //range of function call range
                w.writeln("if not isinstance(",obj_eval.result,",",func_belong_name,"):");
                auto scope_2 = w.indent_scope();
                w.writeln("return False");
                scope_2.execute();
                w.write("if not ", obj_eval.result, ".", func_name, "(");
                add_call_parameter(ctx, w,range);
                w.writeln("):");
                auto scope = w.indent_scope();
                w.writeln("return False");
                scope.execute();
                break;
            }
            case rebgn::AbstractOp::CALL_DECODE: {
                auto func_ref = code.left_ref().value(); //reference of function
                auto func_belong = ctx.ref(func_ref).belong().value(); //reference of function belong
                auto func_belong_name = type_accessor(ctx.ref(func_belong), ctx); //function belong name
                auto func_name = ctx.ident(func_ref); //identifier of function
                auto obj_eval_ref = code.right_ref().value(); //reference of `this` object
                auto obj_eval = eval(ctx.ref(obj_eval_ref), ctx); //`this` object
                auto inner_range = ctx.range(func_ref); //range of function call range
                w.writeln("if not isinstance(",obj_eval.result,",",func_belong_name,"):");
                auto scope_2 = w.indent_scope();
                w.writeln(obj_eval.result,"=",func_belong_name,"()");
                scope_2.execute();
                w.write("if not ", obj_eval.result, ".", func_name, "(");
                add_call_parameter(ctx, w,range);
                w.writeln("):");
                auto scope = w.indent_scope();
                w.writeln("return False");
                scope.execute();
                break;
            }
            case rebgn::AbstractOp::LOOP_INFINITE: {
                w.writeln("while True :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::LOOP_CONDITION: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                w.writeln("while ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::CONTINUE: {
                w.writeln("continue");
                break;
            }
            case rebgn::AbstractOp::BREAK: {
                w.writeln("break");
                break;
            }
            case rebgn::AbstractOp::END_LOOP: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::IF: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                w.writeln("if ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                auto next = find_next_else_or_end_if(ctx,i,true);
                if(next == i +1||ctx.bm.code[i+1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK) {
                    w.writeln("pass");
                }
                break;
            }
            case rebgn::AbstractOp::ELIF: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                defer.pop_back();
                w.writeln("");
                w.writeln("elif ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                auto next = find_next_else_or_end_if(ctx,i,true);
                if(next == i +1||ctx.bm.code[i+1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK) {
                    w.writeln("pass");
                }
                break;
            }
            case rebgn::AbstractOp::ELSE: {
                defer.pop_back();
                w.writeln("");
                w.writeln("else :");
                defer.push_back(w.indent_scope_ex());
                auto next = find_next_else_or_end_if(ctx,i,true);
                if(next == i +1||ctx.bm.code[i+1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK) {
                    w.writeln("pass");
                }
                break;
            }
            case rebgn::AbstractOp::END_IF: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::MATCH: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                w.writeln("match ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::EXHAUSTIVE_MATCH: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                w.writeln("match ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::CASE: {
                auto evaluated_ref = code.ref().value(); //reference of condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //condition
                w.writeln("case ",evaluated.result," :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_CASE: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DEFAULT_CASE: {
                w.writeln("case _ :");
                defer.push_back(w.indent_scope_ex());
                break;
            }
            case rebgn::AbstractOp::END_MATCH: {
                defer.pop_back();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto ident_ref = code.ident().value(); //reference of variable
                auto ident = ctx.ident(ident_ref); //identifier of variable
                auto init_ref = code.ref().value(); //reference of variable initialization
                auto init = eval(ctx.ref(init_ref), ctx); //variable initialization
                auto type_ref = code.type().value(); //reference of variable
                auto type = type_to_string(ctx,type_ref); //variable
                w.writeln(std::format("{} :{} = {}", ident, type, init.result));
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                w.writeln("\"\"\"Unimplemented DEFINE_CONSTANT\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::DECLARE_VARIABLE: {
                auto ident_ref = code.ref().value(); //reference of variable
                auto ident = ctx.ident(ident_ref); //identifier of variable
                auto init_ref = ctx.ref(code.ref().value()).ref().value(); //reference of variable initialization
                auto init = eval(ctx.ref(init_ref), ctx); //variable initialization
                auto type_ref = ctx.ref(code.ref().value()).type().value(); //reference of variable
                auto type = type_to_string(ctx,type_ref); //variable
                w.writeln(std::format("{} :{} = {}", ident, type, init.result));
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_eval_ref = code.left_ref().value(); //reference of assignment target
                auto left_eval = eval(ctx.ref(left_eval_ref), ctx); //assignment target
                auto right_eval_ref = code.right_ref().value(); //reference of assignment source
                auto right_eval = eval(ctx.ref(right_eval_ref), ctx); //assignment source
                w.writeln("", left_eval.result, " = ", right_eval.result, "");
                break;
            }
            case rebgn::AbstractOp::PROPERTY_ASSIGN: {
                w.writeln("\"\"\"Unimplemented PROPERTY_ASSIGN\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::ASSERT: {
                auto evaluated_ref = code.ref().value(); //reference of assertion condition
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //assertion condition
                w.writeln("assert(", evaluated.result, ")");
                break;
            }
            case rebgn::AbstractOp::LENGTH_CHECK: {
                auto vector_eval_ref = code.left_ref().value(); //reference of vector to check
                auto vector_eval = eval(ctx.ref(vector_eval_ref), ctx); //vector to check
                auto size_eval_ref = code.right_ref().value(); //reference of size to check
                auto size_eval = eval(ctx.ref(size_eval_ref), ctx); //size to check
                w.writeln("assert(__builtins__.len(",vector_eval.result,") == ", size_eval.result, ")");
                break;
            }
            case rebgn::AbstractOp::EXPLICIT_ERROR: {
                auto param = code.param().value(); //error message parameters
                auto evaluated = eval(ctx.ref(param.refs[0]), ctx); //error message
                w.writeln("throw std::runtime_error(", evaluated.result, ")");
                break;
            }
            case rebgn::AbstractOp::APPEND: {
                auto vector_eval_ref = code.left_ref().value(); //reference of vector (not temporary)
                auto vector_eval = eval(ctx.ref(vector_eval_ref), ctx); //vector (not temporary)
                auto new_element_eval_ref = code.right_ref().value(); //reference of new element
                auto new_element_eval = eval(ctx.ref(new_element_eval_ref), ctx); //new element
                w.writeln(vector_eval.result, ".append(", new_element_eval.result, ")");
                break;
            }
            case rebgn::AbstractOp::INC: {
                auto evaluated_ref = code.ref().value(); //reference of increment target
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //increment target
                w.writeln(evaluated.result, "+= 1");
                break;
            }
            case rebgn::AbstractOp::RET: {
                auto ref = code.ref().value(); //reference of return value
                if(ref.value() != 0) {
                    auto evaluated = eval(ctx.ref(ref), ctx); //return value
                    w.writeln("return ", evaluated.result, "");
                }
                else {
                    w.writeln("return");
                }
                break;
            }
            case rebgn::AbstractOp::RET_SUCCESS: {
                w.writeln("return True");
                break;
            }
            case rebgn::AbstractOp::RET_PROPERTY_SETTER_OK: {
                w.writeln("return True");
                break;
            }
            case rebgn::AbstractOp::RET_PROPERTY_SETTER_FAIL: {
                w.writeln("return False");
                break;
            }
            case rebgn::AbstractOp::INIT_RECURSIVE_STRUCT: {
                w.writeln("\"\"\"Unimplemented INIT_RECURSIVE_STRUCT\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::CHECK_RECURSIVE_STRUCT: {
                w.writeln("\"\"\"Unimplemented CHECK_RECURSIVE_STRUCT\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::SWITCH_UNION: {
                auto union_member_ref = code.ref().value(); //reference of current union member
                auto union_ref = ctx.ref(union_member_ref).belong().value(); //reference of union
                auto union_field_ref = ctx.ref(union_ref).belong().value(); //reference of union field
                auto union_member_index = ctx.ref(union_member_ref).int_value()->value(); //current union member index
                auto union_member_ident = ctx.ident(union_member_ref); //identifier of union member
                auto union_ident = ctx.ident(union_ref); //identifier of union
                auto union_field_ident = eval(ctx.ref(union_field_ref), ctx); //union field
                w.writeln("if not isinstance(",union_field_ident.result,", ",type_accessor(ctx.ref(union_member_ref),ctx),") :");
                auto scope = w.indent_scope_ex();
                w.writeln("",union_field_ident.result," = ",type_accessor(ctx.ref(union_member_ref),ctx),"()");
                scope.execute();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::CHECK_UNION: {
                auto union_member_ref = code.ref().value(); //reference of current union member
                auto union_ref = ctx.ref(union_member_ref).belong().value(); //reference of union
                auto union_field_ref = ctx.ref(union_ref).belong().value(); //reference of union field
                auto union_member_index = ctx.ref(union_member_ref).int_value()->value(); //current union member index
                auto union_member_ident = ctx.ident(union_member_ref); //identifier of union member
                auto union_ident = ctx.ident(union_ref); //identifier of union
                auto union_field_ident = eval(ctx.ref(union_field_ref), ctx); //union field
                auto check_type = code.check_at().value(); //union check location
                w.writeln("if not isinstance(",union_field_ident.result,", ",type_accessor(ctx.ref(union_member_ref),ctx),") :");
                auto scope = w.indent_scope_ex();
                if(check_type == rebgn::UnionCheckAt::ENCODER) {
                    w.writeln("return False");
                }
                else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_PTR) {
                    w.writeln("return None");
                }
                else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_OPTIONAL) {
                    w.writeln("return None");
                }
                scope.execute();
                w.writeln("");
                break;
            }
            case rebgn::AbstractOp::EVAL_EXPR: {
                w.writeln("\"\"\"Unimplemented EVAL_EXPR\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::RESERVE_SIZE: {
                w.writeln("\"\"\"Unimplemented RESERVE_SIZE\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::BEGIN_ENCODE_SUB_RANGE: {
                auto evaluated_ref = code.ref().value(); //reference of sub range length or vector expression
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //sub range length or vector expression
                auto sub_range_type = code.sub_range_type().value(); //sub range type (byte_len or replacement)
                if(sub_range_type == rebgn::SubRangeType::byte_len) {
                    w.writeln("\"\"\"Unimplemented BEGIN_ENCODE_SUB_RANGE\"\"\" ");
                }
                else if(sub_range_type == rebgn::SubRangeType::replacement) {
                    w.writeln("\"\"\"Unimplemented BEGIN_ENCODE_SUB_RANGE\"\"\" ");
                }
                else {
                    w.writeln("\"\"\"Unimplemented BEGIN_ENCODE_SUB_RANGE\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::END_ENCODE_SUB_RANGE: {
                w.writeln("\"\"\"Unimplemented END_ENCODE_SUB_RANGE\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::BEGIN_DECODE_SUB_RANGE: {
                auto evaluated_ref = code.ref().value(); //reference of sub range length or vector expression
                auto evaluated = eval(ctx.ref(evaluated_ref), ctx); //sub range length or vector expression
                auto sub_range_type = code.sub_range_type().value(); //sub range type (byte_len or replacement)
                if(sub_range_type == rebgn::SubRangeType::byte_len) {
                    w.writeln("\"\"\"Unimplemented BEGIN_DECODE_SUB_RANGE\"\"\" ");
                }
                else if(sub_range_type == rebgn::SubRangeType::replacement) {
                    w.writeln("\"\"\"Unimplemented BEGIN_DECODE_SUB_RANGE\"\"\" ");
                }
                else {
                    w.writeln("\"\"\"Unimplemented BEGIN_DECODE_SUB_RANGE\"\"\" ");
                }
                break;
            }
            case rebgn::AbstractOp::END_DECODE_SUB_RANGE: {
                w.writeln("\"\"\"Unimplemented END_DECODE_SUB_RANGE\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::SEEK_ENCODER: {
                w.writeln("\"\"\"Unimplemented SEEK_ENCODER\"\"\" ");
                break;
            }
            case rebgn::AbstractOp::SEEK_DECODER: {
                w.writeln("\"\"\"Unimplemented SEEK_DECODER\"\"\" ");
                break;
            }
            default: {
                if (!rebgn::is_marker(code.op)&&!rebgn::is_struct_define_related(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {
                    w.writeln(std::format("{}{}{}","\"\"\"",to_string(code.op),"\"\"\""));
                }
                break;
            }
            }
        }
    }
    std::string escape_python_keyword(const std::string& str) {
        if (str == "False"||str == "None"||str == "True"||str == "and"||str == "as"||str == "assert"||str == "async"||str == "await"||str == "break"||str == "class"||str == "continue"||str == "def"||str == "del"||str == "elif"||str == "else"||str == "except"||str == "finally"||str == "for"||str == "from"||str == "global"||str == "if"||str == "import"||str == "in"||str == "is"||str == "lambda"||str == "nonlocal"||str == "not"||str == "or"||str == "pass"||str == "raise"||str == "return"||str == "try"||str == "while"||str == "with"||str == "yield") {
            return str + "_";
        }
        return str;
    }
    void to_python(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags) {
        Context ctx{w, bm, [&](bm2::Context& ctx, std::uint64_t id, auto&& str) {
            auto& code = ctx.ref(rebgn::Varint{id});
            if(code.op == rebgn::AbstractOp::DEFINE_FUNCTION) {
                auto func_type = code.func_type().value();
                auto func_range = ctx.range(code.ident().value());
                if(func_type == rebgn::FunctionType::UNION_SETTER) {
                    auto params = get_parameters(ctx,func_range);
                    if(params.size() != 1) {
                        return "set_" + str;
                    }
                }
                if(func_type== rebgn::FunctionType::VECTOR_SETTER) {
                    return "set_" + str;
                }
            }
            return escape_python_keyword(str);
        }};
        // search metadata
        bool has_is_little_endian = false;
        for (size_t i = 0; i < bm.code.size(); i++) {
            auto& code = bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::IS_LITTLE_ENDIAN: {
                    has_is_little_endian = true;
                    goto OUT_FIRST_SCAN;
                    break;
                }
                default: {}
            }
        }
        OUT_FIRST_SCAN:;
        {
            auto& w = ctx.cw;
            w.writeln("# Code generated by bm2py of https://github.com/on-keyday/rebrgen");
            w.writeln("from typing import IO,Optional,Union");
            w.writeln("from enum import Enum");
            w.writeln("");
        }
        if(has_is_little_endian) {
            ctx.cw.writeln("import sys");
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
    }
}  // namespace bm2python
