/*license*/
#include "bm2rust.hpp"
#include <code/code_writer.h>
#include <unordered_map>
#include <unordered_set>
#include <format>
#include <bm/helper.hpp>
#include <algorithm>
#include <unicode/utf/convert.h>
#include <span>
#include <regex>
#include <escape/escape.h>

namespace bm2rust {
    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;
        std::unordered_map<std::uint64_t, std::uint64_t> ident_index_table;
        std::unordered_map<std::uint64_t, rebgn::Range> ident_range_table;
        std::unordered_map<std::uint64_t, std::string> metadata_table;
        std::string ptr_type;
        rebgn::BMContext bm_ctx;
        std::vector<futils::helper::DynDefer> on_functions;

        std::vector<std::string> this_as;
        std::vector<std::tuple<std::uint64_t /*index*/, std::uint64_t /*bit size*/, rebgn::PackedOpType>> bit_field_ident;
        std::string error_type = "Error";

        std::vector<std::string> current_r;
        std::vector<std::string> current_w;

        std::string r() {
            if (current_r.empty()) {
                return "r";
            }
            return current_r.back();
        }

        std::string mut_r() {
            if (current_r.empty()) {
                return "r";
            }
            return "&mut " + current_r.back();
        }

        std::string w() {
            if (current_w.empty()) {
                return "w";
            }
            return current_w.back();
        }

        bool on_assign = false;

        std::string this_() {
            if (this_as.empty()) {
                return "self";
            }
            return this_as.back();
        }

        const rebgn::Code& ref(rebgn::Varint ident) {
            return bm.code[ident_index_table[ident.value()]];
        }

        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm)
            : cw(w), bm(bm) {
        }

        bool use_async = false;
        bool use_buf_read_peek = false;

        std::string BufReader = "std::io::BufReader";
        std::string Read = "std::io::Read";
        std::string Write = "std::io::Write";
        std::string Seek = "std::io::Seek";
        std::string BufRead = "std::io::BufRead";

        void enable_async() {
            use_async = true;
            BufReader = "tokio::io::BufReader";
            Read = "tokio::io::AsyncReadExt";
            Write = "tokio::io::AsyncWriteExt";
            Seek = "tokio::io::AsyncSeekExt";
            BufRead = "tokio::io::AsyncBufReadExt";
        }
    };

    std::string may_wrap_pin(Context& ctx, std::string value) {
        if (ctx.use_async) {
            return std::format("std::pin::pin!({})", value);
        }
        return value;
    }

    std::uint64_t get_uint_size(size_t bit_size) {
        if (bit_size <= 8) {
            return 8;
        }
        else if (bit_size <= 16) {
            return 16;
        }
        else if (bit_size <= 32) {
            return 32;
        }
        else {
            return 64;
        }
    }

    std ::string get_uint(size_t bit_size) {
        return std::format("u{}", get_uint_size(bit_size));
    }

    constexpr auto rust_impl_Default_threshold = 32;

    std::string type_to_string(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0) {
        if (s.storages.size() <= index) {
            return "void";
        }
        auto& storage = s.storages[index];
        switch (storage.type) {
            case rebgn::StorageType::CODER_RETURN: {
                return "std::result::Result<(), " + ctx.error_type + ">";
            }
            case rebgn::StorageType::PROPERTY_SETTER_RETURN: {
                return "std::result::Result<(), " + ctx.error_type + ">";
            }
            case rebgn::StorageType::PTR: {
                auto inner = type_to_string(ctx, s, bit_size, index + 1);
                return "std::option::Option<&" + inner + ">";
            }
            case rebgn::StorageType::OPTIONAL: {
                auto inner = type_to_string(ctx, s, bit_size, index + 1);
                return "std::option::Option<" + inner + ">";
            }
            case rebgn::StorageType::UINT: {
                auto size = storage.size().value().value();
                if (bit_size) {
                    *bit_size = size;
                }
                return get_uint(size);
            }
            case rebgn::StorageType::INT: {
                auto size = storage.size().value().value();
                if (size <= 8) {
                    return "i8";
                }
                else if (size <= 16) {
                    return "i16";
                }
                else if (size <= 32) {
                    return "i32";
                }
                else {
                    return "i64";
                }
            }
            case rebgn::StorageType::FLOAT: {
                auto size = storage.size().value().value();
                if (size <= 32) {
                    return "float";
                }
                else {
                    return "double";
                }
            }
            case rebgn::StorageType::STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto idx = ctx.ident_index_table[ref];
                auto size = storage.size().value().value();
                if (size && bit_size) {
                    *bit_size = size - 1;
                }
                if (ctx.bm.code[idx].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                    return std::format("Variant{}", ref);
                }
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::RECURSIVE_STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                if (ctx.ptr_type == "*") {
                    return std::format("{}*", ident);
                }
                if (ctx.ptr_type.size()) {
                    return std::format("{}<{}>", ctx.ptr_type, ident);
                }
                return std::format("std::option::Option<std::boxed::Box<{}>>", ident);
            }
            case rebgn::StorageType::ENUM: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                if (bit_size) {
                    type_to_string(ctx, s, bit_size, index + 1);  // for bit_size
                }
                return ident;
            }
            case rebgn::StorageType::VARIANT: {
                auto ref = storage.ref().value().value();
                size_t bit_size_candidate = 0;
                for (index++; index < s.storages.size(); index++) {
                    auto& storage = s.storages[index];
                    type_to_string(ctx, s, &bit_size_candidate, index);
                    if (bit_size) {
                        *bit_size = std::max(*bit_size, bit_size_candidate);
                    }
                }
                return std::format("Variant{}", ref);
                /*
                size_t bit_size_candidate = 0;
                std::string variant = "std::variant<std::monostate";
                for (index++; index < s.storages.size(); index++) {
                    auto& storage = s.storages[index];
                    auto inner = type_to_string(ctx, s, &bit_size_candidate, index);
                    variant += ", " + inner;
                    if (bit_size) {
                        *bit_size = std::max(*bit_size, bit_size_candidate);
                    }
                }
                variant += ">";
                return variant;
                */
            }
            case rebgn::StorageType::ARRAY: {
                auto size = storage.size().value().value();
                auto inner = type_to_string(ctx, s, bit_size, index + 1);
                auto type = std::format("[{}; {}]", inner, size);
                if (size > rust_impl_Default_threshold) {
                    return std::format("std::vec::Vec<{}>", inner);
                }
                return type;
            }
            case rebgn::StorageType::VECTOR: {
                auto inner = type_to_string(ctx, s, bit_size, index + 1);
                return std::format("std::vec::Vec<{}>", inner);
            }
            case rebgn::StorageType::BOOL:
                return "bool";
            default: {
                return "void";
            }
        }
    }

    std::optional<size_t> find_op(Context& ctx, rebgn::Range range, rebgn::AbstractOp op, size_t from = 0) {
        if (from == 0) {
            from = range.start;
        }
        for (size_t i = from; i < range.end; i++) {
            if (ctx.bm.code[i].op == op) {
                return i;
            }
        }
        return std::nullopt;
    }

    std::vector<size_t> find_ops(Context& ctx, rebgn::Range range, rebgn::AbstractOp op) {
        std::vector<size_t> res;
        for (size_t i = range.start; i < range.end; i++) {
            if (ctx.bm.code[i].op == op) {
                res.push_back(i);
            }
        }
        return res;
    }

    std::optional<size_t> find_belong_op(Context& ctx, const rebgn::Code& code, rebgn::AbstractOp op) {
        if (code.op == op) {
            return code.ident().value().value();
        }
        if (auto belong = code.belong(); belong) {
            auto idx = ctx.ident_index_table[belong.value().value()];
            return find_belong_op(ctx, ctx.bm.code[idx], op);
        }
        return std::nullopt;
    }

    enum class BitField {
        none,
        first,
        second,
    };

    std::string retrieve_union_type(Context& ctx, const rebgn::Code& code) {
        if (code.op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
            auto ident = code.ident().value().value();
            return std::format("Variant{}", ident);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_UNION ||
            code.op == rebgn::AbstractOp::DEFINE_FIELD) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto& belong = ctx.bm.code[belong_idx];
            return retrieve_union_type(ctx, belong);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FORMAT) {
            auto ident = code.ident().value().value();
            return ctx.ident_table[ident];
        }
        return "";
    }

    enum class NeedTrait {
        none,
        encoder,
        decoder,
        property_setter,
    };

    void add_function_parameters(Context& ctx, TmpCodeWriter& w, rebgn::Range range, NeedTrait& trait,

                                 rebgn::EncodeParamFlags& encode_flags,
                                 rebgn::DecodeParamFlags& decode_flags) {
        size_t params = 0;
        auto is_decoder = find_op(ctx, range, rebgn::AbstractOp::DECODER_PARAMETER);
        auto is_input_parameter = find_op(ctx, range, rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER);
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::DEFINE_FUNCTION) {
                if (code.belong()->value() != 0) {
                    if (params > 0) {
                        w.write(", ");
                    }
                    if (is_decoder || is_input_parameter) {
                        w.write("&mut self");
                    }
                    else {
                        w.write("&self");
                    }
                    params++;
                }
            }
            if (code.op == rebgn::AbstractOp::ENCODER_PARAMETER) {
                if (params > 0) {
                    w.write(", ");
                }
                std::string typ = "&mut W";
                if (ctx.use_async) {
                    typ = "&mut std::pin::Pin<" + typ + ">";
                }
                w.write("w :", typ);
                trait = NeedTrait::encoder;
                encode_flags = code.encode_flags().value();
                params++;
            }
            if (code.op == rebgn::AbstractOp::DECODER_PARAMETER) {
                if (params > 0) {
                    w.write(", ");
                }
                trait = NeedTrait::decoder;
                decode_flags = code.decode_flags().value();
                std::string typ;
                if (decode_flags.has_peek() && ctx.use_buf_read_peek) {  // for peek method, use BufReader
                    typ = "&mut " + ctx.BufReader + "<R>";
                }
                else {
                    typ = "&mut R";
                }
                if (ctx.use_async) {
                    typ = "&mut std::pin::Pin<" + typ + ">";
                }
                w.write("r :", typ);
                params++;
            }
            if (code.op == rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER) {
                auto param_name = std::format("param{}", code.ident().value().value());
                auto type = type_to_string(ctx, *code.storage());
                if (params > 0) {
                    w.write(", ");
                }
                w.write(param_name, ": ", type);
                trait = NeedTrait::property_setter;
                params++;
            }
            if (code.op == rebgn::AbstractOp::STATE_VARIABLE_PARAMETER) {
                auto field_range = ctx.ident_range_table[code.ref().value().value()];
                auto specify = find_op(ctx, field_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                if (!specify) {
                    continue;
                }
                auto storage = *ctx.bm.code[*specify].storage();
                auto type = type_to_string(ctx, storage);
                auto& ident = ctx.ident_table[code.ref().value().value()];
                if (params > 0) {
                    w.write(", ");
                }
                w.write(ident, " :&mut ", type);
            }
            if (code.op == rebgn::AbstractOp::DECLARE_PARAMETER) {
                auto param_range = ctx.ident_range_table[code.ref().value().value()];
                auto specify = find_op(ctx, param_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                if (!specify) {
                    continue;
                }
                auto storage = *ctx.bm.code[*specify].storage();
                auto type = type_to_string(ctx, storage);
                auto& ident = ctx.ident_table[code.ref().value().value()];
                if (params > 0) {
                    w.write(", ");
                }
                w.write(ident, ": ", type);
            }
        }
    }

    void code_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::ENCODER_PARAMETER) {
                if (ctx.current_w.size()) {
                    w.write("&mut ", ctx.current_w.back());
                }
                else {
                    w.write(ctx.w());
                }
                params++;
            }
            if (code.op == rebgn::AbstractOp::DECODER_PARAMETER) {
                w.write(ctx.mut_r());
                params++;
            }
            if (code.op == rebgn::AbstractOp::STATE_VARIABLE_PARAMETER) {
                auto& ident = ctx.ident_table[code.ref().value().value()];
                if (params > 0) {
                    w.write(", ");
                }
                w.write(ident);
                params++;
            }
        }
    }

    std::string get_property_getter_argument(Context& ctx, std::uint64_t ident) {
        auto found_merged = find_ops(ctx, ctx.ident_range_table[ident], rebgn::AbstractOp::DEFINE_PROPERTY_GETTER);
        for (auto& idx : found_merged) {
            auto& p = ctx.bm.code[idx];
            auto func = p.right_ref().value().value();
            auto merged = p.left_ref().value().value();
            auto& merged_code = ctx.bm.code[ctx.ident_index_table[merged]];
            if (merged_code.merge_mode() == rebgn::MergeMode::COMMON_TYPE ||
                merged_code.merge_mode() == rebgn::MergeMode::STRICT_COMMON_TYPE) {
                TmpCodeWriter cw;
                code_call_parameter(ctx, cw, ctx.ident_range_table[func]);
                return cw.out();
            }
        }
        return "";
    }

    bool property_has_common_type(Context& ctx, std::uint64_t ident) {
        auto found_merged = find_ops(ctx, ctx.ident_range_table[ident], rebgn::AbstractOp::MERGED_CONDITIONAL_FIELD);
        for (auto& idx : found_merged) {
            auto& p = ctx.bm.code[idx];
            if (p.merge_mode() == rebgn::MergeMode::COMMON_TYPE) {
                return true;
            }
        }
        return false;
    }

    bool should_be_bool(Context& ctx, std::uint64_t ident) {
        size_t bit_size = 0;
        auto dstType = find_op(ctx, ctx.ident_range_table[ident], rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
        bool should_be_bool = false;
        if (dstType) {
            auto storage = *ctx.bm.code[*dstType].storage();
            type_to_string(ctx, storage, &bit_size);
            bool has_enum = false;
            for (auto& s : storage.storages) {
                if (s.type == rebgn::StorageType::ENUM) {
                    has_enum = true;
                    break;
                }
            }
            if (!has_enum && bit_size == 1 && !ctx.on_assign) {
                should_be_bool = true;
            }
        }
        return should_be_bool;
    }

    std::string retrieve_ident_internal(Context& ctx, const rebgn::Code& code, BitField& bit_field) {
        if (code.op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
            auto ident = code.ident().value().value();
            auto& upper = ctx.bm.code[ctx.ident_index_table[code.belong().value().value()]];
            auto base = retrieve_ident_internal(ctx, upper, bit_field);
            if (bit_field != BitField::none) {
                return base;
            }
            auto type = retrieve_union_type(ctx, code);
            auto belong_type = std::format("Variant{}", upper.ident().value().value());
            auto mut = ctx.on_assign ? "&mut " : "&";
            return std::format("match {}{} {{  {}::{}(x) => x, _ => unreachable!() }}", mut, base, belong_type, type);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_UNION) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto& belong = ctx.bm.code[belong_idx];
            return retrieve_ident_internal(ctx, belong, bit_field);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_BIT_FIELD) {
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[ctx.ident_index_table[code.belong().value().value()]], bit_field);
            bit_field = BitField::first;
            return base;
        }
        if (code.op == rebgn::AbstractOp::DEFINE_PROPERTY) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[belong_idx], bit_field);
            auto set = ctx.on_assign ? "set_" : "";
            bool is_common_type = property_has_common_type(ctx, code.ident().value().value());
            auto arg = get_property_getter_argument(ctx, code.ident().value().value());
            std::string ident;
            if (auto found = ctx.ident_table.find(code.ident().value().value());
                found != ctx.ident_table.end() && found->second.size()) {
                ident = found->second;
            }
            else {
                ident = std::format("field_{}", code.ident().value().value());
            }
            if (is_common_type) {
                return std::format("{}.{}{}({}).unwrap()", base, set, ident, arg);
            }
            else {
                return std::format("(*{}.{}{}({}).unwrap())", base, set, ident, arg);
            }
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FIELD) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[belong_idx], bit_field);
            std::string paren;
            const char* set = "";
            if (bit_field == BitField::second) {
                return base;
            }
            else if (bit_field == BitField::first) {
                bit_field = BitField::second;
                if (ctx.on_assign) {
                    set = "set_";
                }
                paren = "()";
            }
            if (base.size()) {
                base += ".";
            }
            if (auto found = ctx.ident_table.find(code.ident().value().value());
                found != ctx.ident_table.end() && found->second.size()) {
                auto fn = std::format("{}{}{}{}", base, set, found->second, paren);
                if (should_be_bool(ctx, code.ident().value().value())) {
                    fn = std::format("if {} {{1}} else {{0}}", fn);
                }
                return fn;
            }
            return std::format("{}{}field_{}{}", base, set, code.ident().value().value(), paren);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FORMAT) {
            return ctx.this_();
        }
        return "";
    }

    bool is_bit_field_property(Context& ctx, rebgn::Range range) {
        if (auto op = find_op(ctx, range, rebgn::AbstractOp::PROPERTY_FUNCTION); op) {
            // merged conditional field
            auto& p = ctx.bm.code[ctx.ident_index_table[ctx.bm.code[*op].ref()->value()]];
            if (p.merge_mode() == rebgn::MergeMode::STRICT_TYPE) {
                auto param = p.param();
                bool cont = false;
                for (auto& pa : param->expr_refs) {
                    auto& idx = ctx.bm.code[ctx.ident_index_table[pa.value()]];
                    auto& ident = ctx.bm.code[ctx.ident_index_table[idx.right_ref()->value()]];
                    if (find_belong_op(ctx, ident, rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::string retrieve_ident(Context& ctx, const rebgn::Code& code) {
        BitField bit_field = BitField::none;
        return retrieve_ident_internal(ctx, code, bit_field);
    }

    std::string get_belong_name(Context& ctx, const rebgn::Code& code) {
        if (auto belong = code.belong(); belong) {
            if (belong->value() == 0) {
                auto is_encode = to_string(code.op)[0];
                return is_encode == 'B' ? "backward" : is_encode == 'E' ? "output.put"
                                                                        : "input.get";
            }
            return retrieve_ident(ctx, ctx.bm.code[ctx.ident_index_table[belong.value().value()]]);
        }
        return "";
    }

    std::string may_insert_await(Context& ctx) {
        if (ctx.use_async) {
            return ".await";
        }
        return "";
    }

    std::string map_io_error(Context& ctx, const std::string& loc, bool should_insert_await = true) {
        return std::format("{}.map_err(|e| Error::IOError(\"{}\",e))?", should_insert_await ? may_insert_await(ctx) : "", loc);
    }

    std::vector<std::string> eval(const rebgn::Code& code, Context& ctx) {
        std::vector<std::string> res;
        switch (code.op) {
            case rebgn::AbstractOp::PHI: {
                auto ref = code.ref().value().value();
                auto idx = ctx.ident_index_table[ref];
                res = eval(ctx.bm.code[idx], ctx);
                break;
            }
            case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                auto value = code.ident().value().value();
                res.push_back(std::format("param{}", value));
                break;
            }
            case rebgn::AbstractOp::ARRAY_SIZE: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                res.push_back(std::format("{}.len()", ref.back()));
                break;
            }
            case rebgn::AbstractOp::OPTIONAL_OF: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                res.push_back(std::format("Some({}.into())", ref.back()));
                break;
            }
            case rebgn::AbstractOp::ADDRESS_OF: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto back = ref.back();
                if (back.ends_with(".as_ref().unwrap()")) {
                    back.erase(back.size() - 18, 18);
                }
                res.push_back(std::format("Some(&{})", back));
                break;
            }
            case rebgn::AbstractOp::CAN_READ: {
                auto belong_name = get_belong_name(ctx, code);
                res.push_back(std::format("{}.fill_buf(){}.map(|b| !b.is_empty()){}", ctx.r(), may_insert_await(ctx), map_io_error(ctx, belong_name, false)));
                break;
            }
            case rebgn::AbstractOp::CALL_CAST: {
                auto storage = *code.storage();
                auto type_str = type_to_string(ctx, storage);
                auto param = code.param().value();
                if (param.expr_refs.size() == 0) {
                    res.push_back(std::format("({}::default())", type_str));
                    break;
                }
                else if (param.expr_refs.size() == 1) {
                    auto eval_arg = eval(ctx.bm.code[ctx.ident_index_table[param.expr_refs[0].value()]], ctx).back();
                    if (storage.storages[0].type == rebgn::StorageType::ENUM) {
                        res.push_back(std::format("unsafe {{ std::mem::transmute::<_,{}>({}) }}", type_str, eval_arg));
                    }
                    else {
                        res.push_back(std::format("({} as {})", eval_arg, type_str));
                    }
                }
                else {
                    std::string arg_call;
                    for (size_t i = 0; i < param.expr_refs.size(); i++) {
                        auto index = ctx.ident_index_table[param.expr_refs[i].value()];
                        auto expr = eval(ctx.bm.code[index], ctx);
                        res.insert(res.end(), expr.begin(), expr.end() - 1);
                        if (i) {
                            arg_call += ", ";
                        }
                        arg_call += expr.back();
                    }
                    res.push_back(std::format("({}({}))", type_str, arg_call));
                }
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("({})[{} as usize]", left.back(), right.back()));
                break;
            }
            case rebgn::AbstractOp::APPEND: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                ctx.on_assign = true;
                auto left = eval(ctx.bm.code[left_index], ctx);
                ctx.on_assign = false;
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("{}.push({});", left.back(), right.back()));
                break;
            }
            case rebgn::AbstractOp::ASSIGN_CAST: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                size_t to_bit_size = 0, from_bit_size = 0;
                auto to_type = type_to_string(ctx, *code.storage(), &to_bit_size);
                auto from_type = type_to_string(ctx, *code.from(), &from_bit_size);
                if (to_type.starts_with("std::option::Option<std::boxed::Box<")) {
                    res.push_back("Some(Box::new(" + ref.back() + "))");
                }
                else if (to_bit_size == 1) {
                    res.push_back(std::format("{} != 0", ref.back()));
                }
                else if (to_bit_size < from_bit_size) {
                    res.push_back(std::format("{}.try_into()?", ref.back()));
                }
                else {
                    res.push_back(std::format("{}.into()", ref.back()));
                }
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto prev = ctx.on_assign;
                ctx.on_assign = true;
                auto left = eval(ctx.bm.code[left_index], ctx);
                ctx.on_assign = prev;
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                if (left.back().starts_with("(*")) {
                    bool is_input_parameter = false;
                    if (ctx.bm.code[right_index].op == rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER) {
                        is_input_parameter = true;
                    }
                    else if (ctx.bm.code[right_index].op == rebgn::AbstractOp::ASSIGN_CAST) {
                        auto right_ref = ctx.bm.code[right_index].ref().value().value();
                        auto right_ref_index = ctx.ident_index_table[right_ref];
                        if (ctx.bm.code[right_ref_index].op == rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER) {
                            is_input_parameter = true;
                        }
                    }
                    if (is_input_parameter) {
                        auto back = left.back();
                        // remove (*
                        back.erase(0, 2);
                        // remove .as_mut().unwrap())
                        back.erase(back.size() - 19, 19);
                        res.push_back(std::format("{} = {};", back, right.back()));
                    }
                    else {
                        res.push_back(std::format("{} = {};", left.back(), right.back()));
                    }
                }
                else if (left.back().ends_with(")")) {
                    left.back().pop_back();
                    std::string arg;
                    while (left.size() && left.back().back() != '(') {
                        arg.push_back(left.back().back());
                        left.back().pop_back();
                    }
                    if (arg.size()) {
                        std::ranges::reverse(arg);
                        arg = "," + arg;
                    }
                    res.push_back(std::format("{}{}{});", left.back(), right.back(), arg));
                }
                else {
                    res.push_back(std::format("{} = {};", left.back(), right.back()));
                }
                res.push_back(left.back());
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TYPE: {
                res.push_back(type_to_string(ctx, *code.storage()));
                break;
            }
            case rebgn::AbstractOp::ACCESS: {
                auto& left_ident = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_ident], ctx);
                auto& right_ident = ctx.ident_table[code.right_ref().value().value()];
                if (ctx.bm.code[left_ident].op == rebgn::AbstractOp::IMMEDIATE_TYPE) {
                    res.push_back(std::format("{}::{}", left.back(), right_ident));
                }
                else {
                    res.insert(res.end(), left.begin(), left.end() - 1);
                    if (auto prop = find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], rebgn::AbstractOp::DEFINE_PROPERTY)) {
                        auto arg = get_property_getter_argument(ctx, *prop);
                        if (property_has_common_type(ctx, *prop)) {
                            res.push_back(std::format("{}.{}({}).unwrap()", left.back(), right_ident, arg));
                        }
                        else {
                            res.push_back(std::format("(*{}.{}({}).unwrap())", left.back(), right_ident, arg));
                        }
                    }
                    else if (find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        auto dst = std::format("{}.{}()", left.back(), right_ident);
                        if (should_be_bool(ctx, code.right_ref().value().value())) {
                            dst = std::format("if {} {{1}} else {{0}}", dst);
                        }
                        res.push_back(std::move(dst));
                    }
                    else {
                        res.push_back(std::format("{}.{}", left.back(), right_ident));
                    }
                }
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT64:
                res.push_back(std::format("{}", code.int_value64().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_INT:
                res.push_back(std::format("{}", code.int_value().value().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_CHAR: {
                std::string utf8;
                futils::utf::from_utf32(std::uint32_t(code.int_value().value().value()), utf8);
                res.push_back(std::format("{} /*{}*/", code.int_value().value().value(), utf8));
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TRUE:
                res.push_back("true");
                break;
            case rebgn::AbstractOp::IMMEDIATE_FALSE:
                res.push_back("false");
                break;
            case rebgn::AbstractOp::IMMEDIATE_STRING: {
                auto str = ctx.string_table[code.ident().value().value()];
                res.push_back(std::format("b\"{}\"", futils::escape::escape_str<std::string>(
                                                         str,
                                                         futils::escape::EscapeFlag::hex,
                                                         futils::escape::no_escape_set(),
                                                         futils::escape::escape_all())));
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                auto bop = to_string(code.bop().value());
                res.push_back(std::format("({} {} {})", left.back(), bop, right.back()));
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto uop = to_string(code.uop().value());
                if (code.uop().value() == rebgn::UnaryOp::bit_not) {
                    uop = "!";  // rust does not have bit_not(~)
                }
                res.push_back(std::format("({}{})", uop, ref.back()));
                break;
            }
            case rebgn::AbstractOp::ENUM_CAST: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto typ = code.storage().value();
                auto type = type_to_string(ctx, typ);
                if (typ.storages[0].type == rebgn::StorageType::ENUM) {
                    auto srcType = type_to_string(ctx, typ, nullptr, 1);
                    res.push_back(std::format("unsafe {{ std::mem::transmute::<{},{}>({}) }}", srcType, type, ref.back()));
                }
                else {
                    res.push_back(std::format("({} as {})", ref.back(), type));
                }
                break;
            }
            case rebgn::AbstractOp::NEW_OBJECT: {
                auto storage = *code.storage();
                auto type = type_to_string(ctx, storage);
                res.push_back(std::format("<{}>::default()", type));
                break;
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                auto ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(std::move(ident));
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD:
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                auto range = ctx.ident_range_table[code.ident().value().value()];
                bool should_deref = false;
                auto found = find_op(ctx, range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                if (found) {
                    auto st = *ctx.bm.code[*found].storage();
                    if (st.storages.back().type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                        should_deref = true;
                    }
                }
                auto ident = retrieve_ident(ctx, code);
                if (should_deref) {
                    if (ctx.on_assign) {
                        res.push_back(std::format("(*{}.as_mut().unwrap())", ident));
                    }
                    else {
                        res.push_back(std::format("{}.as_ref().unwrap()", ident));
                    }
                }
                else {
                    res.push_back(std::format("{}", ident));
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_ENUM_MEMBER: {
                auto& ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(ident);
                break;
            }
            case rebgn::AbstractOp::DECLARE_VARIABLE: {
                res = eval(ctx.bm.code[ctx.ident_index_table[code.ref().value().value()]], ctx);
                break;
            }
            case rebgn::AbstractOp::EMPTY_PTR: {
                res.push_back("None");
                break;
            }
            case rebgn::AbstractOp::EMPTY_OPTIONAL: {
                res.push_back("None");
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto& ref_index = ctx.ident_index_table[code.ref().value().value()];
                res = eval(ctx.bm.code[ref_index], ctx);
                auto& taken = res.back();
                if (auto found = ctx.ident_table.find(code.ident().value().value()); found != ctx.ident_table.end()) {
                    taken = std::format("let mut {} = {};", found->second, taken);
                    res.push_back(found->second);
                }
                else {
                    taken = std::format("let mut tmp{} = {};", code.ident().value().value(), taken);
                    res.push_back(std::format("tmp{}", code.ident().value().value()));
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                auto& ref_index = ctx.ident_index_table[code.ref().value().value()];
                res = eval(ctx.bm.code[ref_index], ctx);
                auto& taken = res.back();
                if (auto found = ctx.ident_table.find(code.ident().value().value()); found != ctx.ident_table.end()) {
                    taken = std::format("const {}: {} = {};", found->second, type_to_string(ctx, *code.storage()), taken);
                    res.push_back(found->second);
                }
                else {
                    taken = std::format("const tmp{}: {} = {};", code.ident().value().value(), type_to_string(ctx, *code.storage()), taken);
                    res.push_back(std::format("tmp{}", code.ident().value().value()));
                }
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                auto this_as = code.left_ref().value().value();
                if (this_as == 0) {
                    auto cond_expr = eval(ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], ctx);
                    res.insert(res.end(), cond_expr.begin(), cond_expr.end() - 1);
                    res.push_back(std::format("{}", cond_expr.back()));
                }
                else {
                    auto base_expr = eval(ctx.bm.code[ctx.ident_index_table[this_as]], ctx);
                    res.insert(res.end(), base_expr.begin(), base_expr.end() - 1);
                    ctx.this_as.push_back(base_expr.back());
                    auto cond_expr = eval(ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], ctx);
                    res.insert(res.end(), cond_expr.begin(), cond_expr.end() - 1);
                    ctx.this_as.pop_back();
                    res.push_back(cond_expr.back());
                }
                break;
            }
            default:
                res.push_back(std::format("/* Unimplemented op: {} */", to_string(code.op)));
                break;
        }
        std::vector<size_t> index;
        for (size_t i = 0; i < res.size(); i++) {
            index.push_back(i);
        }
        std::ranges::sort(index, [&](size_t a, size_t b) {
            return res[a] < res[b];
        });
        auto uniq = std::ranges::unique(index, [&](size_t a, size_t b) {
            return res[a] == res[b];
        });
        index.erase(uniq.begin(), index.end());
        std::vector<std::string> unique;
        for (size_t i = 0; i < res.size(); i++) {
            if (std::find(index.begin(), index.end(), i) == index.end()) {
                continue;
            }
            unique.push_back(std::move(res[i]));
        }
        return unique;
    }

    void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::DECLARE_FORMAT ||
                code.op == rebgn::AbstractOp::DECLARE_STATE ||
                code.op == rebgn::AbstractOp::DECLARE_BIT_FIELD) {
                auto range = ctx.ident_range_table[code.ref().value().value()];
                inner_block(ctx, w, range);
                continue;
            }
            if (code.op == rebgn::AbstractOp::DECLARE_UNION || code.op == rebgn::AbstractOp::DECLARE_UNION_MEMBER) {
                if (ctx.bm_ctx.inner_bit_operations) {
                    continue;
                }
                auto range = ctx.ident_range_table[code.ref().value().value()];
                TmpCodeWriter w2;
                inner_block(ctx, w2, range);
                ctx.cw.write_unformatted(w2.out());
                continue;
            }
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_UNION: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    defer.push_back(futils::helper::defer_ex([&] {
                        auto& code = ctx.bm.code[range.start];
                        auto name = std::format("Variant{}", code.ident().value().value());
                        w.writeln("#[derive(Debug,Default, Clone, PartialEq, Eq)]");
                        w.writeln("enum ", name, " {");
                        auto scope = w.indent_scope();
                        w.writeln("#[default]");
                        w.writeln("None,");
                        for (size_t i = range.start; i < range.end; i++) {
                            auto& code = ctx.bm.code[i];
                            if (code.op == rebgn::AbstractOp::DECLARE_UNION_MEMBER) {
                                auto ident_num = code.ref().value().value();
                                auto ident = std::format("Variant{}", ident_num);
                                w.writeln(ident, "(", ident, "),");
                            }
                        }
                        scope.execute();
                        w.writeln("}");
                    }));
                    break;
                }
                case rebgn::AbstractOp::END_UNION: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::DECLARE_ENUM: {
                    break;
                }
                case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    w.writeln("#[derive(Debug,Default, Clone, PartialEq, Eq)]");
                    w.writeln(std::format("struct Variant{} {{", code.ident().value().value()));
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_UNION_MEMBER: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    defer.pop_back();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_PROPERTY: {
                    /*
                    auto belong = ctx.bm.code[ctx.ident_index_table[code.ref().value().value()]].belong().value().value();
                    if (belong == 0 || ctx.bm.code[ctx.ident_index_table[belong]].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                        break;  // skip global property
                    }
                    auto belong_ident = ctx.ident_table[belong];
                    auto range = ctx.ident_range_table[code.ref().value().value()];
                    auto ident = ctx.ident_table[code.ref().value().value()];
                    auto getters = find_ops(ctx, range, rebgn::AbstractOp::DEFINE_PROPERTY_GETTER);
                    auto setters = find_ops(ctx, range, rebgn::AbstractOp::DEFINE_PROPERTY_SETTER);
                    for (auto& getter : getters) {
                        auto& m = ctx.bm.code[getter];
                        auto& merged = ctx.bm.code[ctx.ident_index_table[m.left_ref().value().value()]];
                        auto& func = ctx.bm.code[ctx.ident_index_table[m.right_ref().value().value()]];
                        auto mode = merged.merge_mode().value();
                        auto range = ctx.ident_range_table[func.ident().value().value()];
                        if (is_bit_field_property(ctx, range)) {
                            continue;  // skip
                        }
                        auto typ = find_op(ctx, range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                        auto result = type_to_string(ctx, *ctx.bm.code[*typ].storage());
                        auto getter_ident = ctx.ident_table[func.ident().value().value()];
                        w.writeln(result, " ", getter_ident, "();");
                    }
                    for (auto& setter : setters) {
                        auto& m = ctx.bm.code[setter];
                        auto& merged = ctx.bm.code[ctx.ident_index_table[m.left_ref().value().value()]];
                        auto& func = ctx.bm.code[ctx.ident_index_table[m.right_ref().value().value()]];
                        auto mode = merged.merge_mode().value();
                        auto setter_ident = ctx.ident_table[func.ident().value().value()];
                        auto range = ctx.ident_range_table[func.ident().value().value()];
                        if (is_bit_field_property(ctx, range)) {
                            continue;  // skip
                        }
                        w.write("bool ", setter_ident, "(");
                        add_function_parameters(ctx, range);
                        w.writeln(");");
                    }
                    */
                    break;
                }
                case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
                    auto name = ctx.bm.code[range.start].ident().value().value();
                    auto field_name = std::format("field_{}", name);
                    w.write(field_name, " :");
                    defer.push_back(futils::helper::defer_ex([&, field_name] {
                        auto name = ctx.bm.code[range.start].belong().value().value();
                        auto belong_name = ctx.ident_table[name];
                        ctx.bm_ctx.inner_bit_operations = false;
                        auto total_size = find_op(ctx, range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                        if (!total_size) {
                            w.writeln("/* Unimplemented field */");
                            return;
                        }
                        auto storage = *ctx.bm.code[*total_size].storage();
                        size_t bit_size = 0;
                        auto type = type_to_string(ctx, storage, &bit_size);
                        size_t consumed_size = 0;
                        size_t counter = 0;
                        for (size_t i = range.start; i < range.end; i++) {
                            if (ctx.bm.code[i].op == rebgn::AbstractOp::DECLARE_FIELD) {
                                auto ident = ctx.ident_table[ctx.bm.code[i].ref().value().value()];
                                if (ident == "") {
                                    ident = std::format("field_{}", ctx.bm.code[i].ref().value().value());
                                }
                                auto storage = find_op(ctx, ctx.ident_range_table[ctx.bm.code[i].ref().value().value()], rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                                if (!storage) {
                                    w.writeln("/* Unimplemented field *");
                                    continue;
                                }
                                auto storages = *ctx.bm.code[*storage].storage();
                                size_t field_size = 0;
                                auto typ = type_to_string(ctx, storages, &field_size);
                                std::string enum_ident;
                                for (auto& s : storages.storages) {
                                    if (s.type == rebgn::StorageType::ENUM) {
                                        enum_ident = ctx.ident_table[s.ref().value().value()];
                                        break;
                                    }
                                }
                                std::string str = get_uint(field_size);
                                auto get_type = [&]() {
                                    if (enum_ident.size()) {
                                        return enum_ident;
                                    }
                                    if (field_size == 1) {
                                        return std::string("bool");
                                    }
                                    return str;
                                };
                                auto return_type = get_type();
                                TmpCodeWriter w2;
                                w2.writeln("impl ", belong_name, " {");
                                auto scope = w2.indent_scope_ex();
                                w2.writeln("pub fn ", ident, "(&self) -> ", return_type, " {");
                                auto scope2 = w2.indent_scope();
                                consumed_size += field_size;
                                auto mask = std::format("{}", (std::uint64_t(1) << field_size) - 1);
                                if (enum_ident.size()) {
                                    w2.write("unsafe { std::mem::transmute(");
                                }
                                w2.write("((self.", field_name, ">>", std::format("{}", bit_size - consumed_size), ") & ", mask, ") as ", str);
                                if (enum_ident.size()) {
                                    w2.write(") }");
                                }
                                else if (field_size == 1) {
                                    w2.write(" != 0");
                                }
                                w.writeln();
                                scope2.execute();
                                w2.writeln("}");
                                w2.writeln("pub fn set_", ident, "(&mut self, value: ", return_type, ") -> bool {");
                                auto scope3 = w2.indent_scope();
                                std::string value = "value";
                                if (return_type != "bool") {
                                    w2.writeln("if value", enum_ident.size() ? std::format(" as {}", str) : "", " > ", mask, " {");
                                    w2.writeln("return false;");
                                    w2.writeln("}");
                                }
                                else {
                                    value = "if value {1} else {0}";
                                }
                                w2.writeln("self.", field_name, " |= (", value, " as ", type, ") << ", std::format("{}", bit_size - consumed_size), ";");
                                w2.writeln("true");
                                scope3.execute();
                                w2.writeln("}");
                                scope.execute();
                                w2.writeln("}");
                                ctx.cw.write_unformatted(w2.out());
                            }
                        }
                    }));
                    ctx.bm_ctx.inner_bit_operations = true;
                    break;
                }
                case rebgn::AbstractOp::END_BIT_FIELD: {
                    ctx.bm_ctx.inner_bit_operations = false;
                    break;
                }
                case rebgn::AbstractOp::SPECIFY_STORAGE_TYPE: {
                    auto storage = *code.storage();
                    auto type = type_to_string(ctx, storage);
                    w.writeln(type, ",");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FIELD: {
                    auto belong = ctx.bm.code[ctx.ident_index_table[code.ref().value().value()]].belong().value().value();
                    if (belong == 0 || ctx.bm.code[ctx.ident_index_table[belong]].op == rebgn::AbstractOp::DEFINE_PROGRAM) {
                        break;  // skip global field
                    }
                    std::string ident;
                    if (auto found = ctx.ident_table.find(code.ref().value().value()); found != ctx.ident_table.end()) {
                        ident = found->second;
                    }
                    else {
                        ident = std::format("field_{}", code.ref().value().value());
                    }
                    auto range = ctx.ident_range_table[code.ref().value().value()];
                    auto specify = find_op(ctx, range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!specify) {
                        w.writeln("/* Unimplemented field */");
                        break;
                    }
                    auto storage = *ctx.bm.code[*specify].storage();
                    size_t bit_size = 0;
                    auto type = type_to_string(ctx, storage, &bit_size);
                    if (ctx.bm_ctx.inner_bit_operations) {
                        w.writeln(std::format("/*{} :{}*/", ident, bit_size));
                    }
                    else {
                        w.writeln("pub ", ident, ": ", type, ",");
                    }
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FORMAT:
                case rebgn::AbstractOp::DEFINE_STATE: {
                    auto& ident = ctx.ident_table[code.ident().value().value()];
                    w.writeln("#[derive(Debug,Default, Clone, PartialEq, Eq)]");
                    w.writeln("pub struct ", ident, " {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FUNCTION: {
                    /**
                    auto& ident = ctx.ident_table[code.ref().value().value()];
                    auto fn_range = ctx.ident_range_table[code.ref().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!ret) {
                        w.write("void ", ident, "(");
                    }
                    else {
                        auto storage = *ctx.bm.code[*ret].storage();
                        auto type = type_to_string(ctx, storage);
                        w.write(type, " ", ident, "(");
                    }
                    add_function_parameters(ctx, fn_range);
                    w.writeln(");");
                    */
                    break;
                }
                case rebgn::AbstractOp::END_FORMAT:
                case rebgn::AbstractOp::END_STATE: {
                    defer.pop_back();
                    w.writeln("}");
                    break;
                }
                default:
                    w.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    void encode_bit_field(Context& ctx, TmpCodeWriter& w, std::uint64_t bit_size, std::uint64_t ref) {
        auto prev = std::get<0>(ctx.bit_field_ident.back());
        auto total_size = std::get<1>(ctx.bit_field_ident.back());
        auto bit_counter = std::format("bit_counter{}", prev);
        auto tmp = std::format("tmp{}", prev);
        auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
        auto bit_count = bit_size;
        w.writeln(std::format("{} <<= {};", tmp, bit_count));
        auto eval = evaluated.back();
        w.writeln(std::format("{} |= ({} & {}) as u{};", tmp, eval, (std::uint64_t(1) << bit_count) - 1, total_size));
        w.writeln(std::format("{} += {};", bit_counter, bit_count));
    }

    void decode_bit_field(Context& ctx, TmpCodeWriter& w, std::uint64_t bit_size, std::uint64_t ref) {
        auto prev = std::get<0>(ctx.bit_field_ident.back());
        auto total_size = std::get<1>(ctx.bit_field_ident.back());
        auto ptype = std::get<2>(ctx.bit_field_ident.back());
        auto bit_counter = std::format("bit_counter{}", prev);
        auto tmp = std::format("tmp{}", prev);
        ctx.on_assign = true;
        auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
        ctx.on_assign = false;
        w.writeln(std::format("{} -= {};", bit_counter, bit_size));
        if (ptype == rebgn::PackedOpType::VARIABLE) {
            auto read_size = std::format("read_size{}", prev);
            auto consumed_bits = std::format("consumed_bits{}", prev);
            auto array = std::format("tmp_array{}", prev);
            w.writeln(std::format("{} += {};", consumed_bits, bit_size));
            w.writeln("let consumed_byte = (", consumed_bits, " + 7)/8;");
            w.writeln("if  consumed_byte > ", read_size, " {");
            auto scope = w.indent_scope();
            auto belong = ctx.bm.code[ctx.ident_index_table[prev]].belong().value().value();
            auto belong_name = ctx.ident_table[belong];
            w.writeln(std::format("{}.read_exact(&mut {}[{}..consumed_byte]){};",
                                  ctx.r(), array, read_size, map_io_error(ctx, belong_name)));
            w.writeln(std::format("for i in {}..consumed_byte {{", read_size));
            auto scope2 = w.indent_scope();
            w.writeln(tmp, " |= (", array, "[i] as u", std::format("{}", total_size), ") <<", "8 * (", std::format("{}", total_size / 8), " - i - 1);");
            scope2.execute();
            w.writeln("}");
            scope.execute();
            w.writeln(read_size, " = consumed_byte;");
            w.writeln("}");
        }
        auto eval_target = find_op(ctx, ctx.ident_range_table[ref], rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
        if (!eval_target) {
            w.writeln("/* Unimplemented bit field */");
            return;
        }
        auto storage = *ctx.bm.code[*eval_target].storage();
        size_t dst_bit_size = 0;
        auto type = type_to_string(ctx, storage, &dst_bit_size);
        std::string not_zero;
        if (dst_bit_size == 1) {
            not_zero = " != 0";
        }
        if (evaluated.back().back() == ')') {
            evaluated.back().pop_back();  // must be end with )
            if (auto in_union = find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[ref]], rebgn::AbstractOp::DEFINE_UNION);
                in_union && find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[ref]], rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                auto belong = ctx.bm.code[ctx.ident_index_table[*in_union]].belong();
                auto storage_type = find_op(ctx, ctx.ident_range_table[belong.value().value()], rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                if (storage_type) {
                    auto storage = *ctx.bm.code[*storage_type].storage();
                    size_t bit_size = 0;
                    type_to_string(ctx, storage, &bit_size);
                    type = get_uint(bit_size);
                    if (bit_size == 1) {
                        not_zero = " != 0";
                    }
                    else {
                        not_zero = "";
                    }
                }
            }
            w.writeln(std::format("{}(({} >> {}) & {}) as {}{});", evaluated.back(), tmp, bit_counter, (std::uint64_t(1) << bit_size) - 1, type, not_zero));
        }
        else {
            w.writeln(std::format("{} = (({} >> {}) & {}) as {}{};", evaluated.back(), tmp, bit_counter, (std::uint64_t(1) << bit_size) - 1, type, not_zero));
        }
    }

    constexpr bool is_known_size(size_t size) {
        return size == 8 || size == 16 || size == 32 || size == 64;
    }

    void serialize(Context& ctx, TmpCodeWriter& w, size_t bit_size, const std::string& loc, const std::string& target, rebgn::EndianExpr endian) {
        if (is_known_size(bit_size)) {
            w.writeln("w.write_all(&", target, ".to_be_bytes())", map_io_error(ctx, loc), ";");
        }
        else {
            auto target_size = get_uint_size(bit_size) / 8;
            auto byte_size = bit_size / 8;
            auto tmp = std::format("tmp_se{}", bit_size);
            w.writeln("let mut ", tmp, " = <[u8; ", std::format("{}", bit_size / 8), "]>::default();");
            w.writeln(tmp, ".copy_from_slice(&", target, ".to_be_bytes()[", std::format("{}", target_size - byte_size), "..]);");
            w.writeln("w.write_all(&", tmp, ")", map_io_error(ctx, loc), ";");
        }
    }

    void deserialize(Context& ctx, TmpCodeWriter& w, size_t id, size_t bit_size, const std::string& loc, const std::string& target, rebgn::EndianExpr endian) {
        auto tmp = std::format("tmp_de{}", id);
        w.writeln("let mut ", tmp, " = <[u8; ", std::format("{}", bit_size / 8), "]>::default();");
        w.writeln(ctx.r(), ".read_exact(", "&mut ", tmp, ")", map_io_error(ctx, loc), ";");
        if (is_known_size(bit_size)) {
            w.writeln(target, " = u", std::format("{}", bit_size), "::from_be_bytes(", tmp, ");");
        }
        else {
            auto byte_size = bit_size / 8;
            auto target_size = get_uint_size(bit_size) / 8;
            auto tmp_i = std::format("tmp_i{}", id);
            w.writeln("for ", tmp_i, " in 0..", std::format("{}", byte_size), "{");
            auto space = w.indent_scope();
            w.writeln(target, " |= (", tmp, "[", tmp_i, "] as u", std::format("{}", target_size * 8), ") << 8 * (", std::format("{}", target_size - 1), " - ", tmp_i, ");");
            space.execute();
            w.writeln("}");
        }
    }

    void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION:
                case rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION: {
                    ctx.bm_ctx.inner_bit_operations = true;
                    auto bit_field = ctx.ident_range_table[code.belong().value().value()];
                    auto belong_name = ctx.ident_table[code.belong().value().value()];
                    auto typ = find_op(ctx, bit_field, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!typ) {
                        w.writeln("/* Unimplemented bit field */");
                        break;
                    }
                    size_t bit_size = 0;
                    auto type = type_to_string(ctx, *ctx.bm.code[*typ].storage(), &bit_size);
                    auto ident = code.ident().value().value();
                    auto tmp = std::format("tmp{}", ident);
                    auto ptype = code.packed_op_type().value();
                    auto bit_counter = std::format("bit_counter{}", ident);
                    w.writeln("let mut ", tmp, ":", type, " = 0; /* bit field */");
                    if (code.op == rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION) {
                        w.writeln("let mut ", bit_counter, " = 0;");
                        ctx.bit_field_ident.push_back({code.ident()->value(), bit_size, ptype});
                        defer.push_back(futils::helper::defer_ex([=, &w, &ctx]() {
                            if (ptype == rebgn::PackedOpType::FIXED) {
                                serialize(ctx, w, bit_size, tmp, tmp, {.endian = rebgn::Endian::big});
                            }
                            else {
                                auto tmp_array = std::format("tmp_array{}", ident);
                                w.writeln("let mut ", tmp_array, " =  <[u8; ", std::format("{}", bit_size / 8), "]>::default();");
                                auto byte_counter = std::format("byte_counter{}", ident);
                                w.writeln("let mut ", byte_counter, " = (", bit_counter, " + 7) / 8;");
                                auto step = std::format("step{}", ident);
                                w.writeln("for ", step, " in 0..", byte_counter, "{");
                                auto space = w.indent_scope();
                                w.writeln(tmp_array, "[", step, "] = ((", tmp, " >> (", byte_counter, " - ", step, " - 1) * 8) & 0xff) as u8;");
                                space.execute();
                                w.writeln("}");
                                w.writeln("w.write_all(&", tmp_array, "[0..", byte_counter, "])", map_io_error(ctx, belong_name), ";");
                            }
                        }));
                    }
                    else {
                        ctx.bit_field_ident.push_back({code.ident()->value(), bit_size, ptype});
                        w.writeln(std::format("let mut {} = {};", bit_counter, bit_size));
                        if (ptype == rebgn::PackedOpType::FIXED) {
                            deserialize(ctx, w, code.ident()->value(), bit_size, tmp, tmp, {.endian = rebgn::Endian::big});
                        }
                        else {
                            auto read_size = std::format("read_size{}", ident);
                            auto consumed_bits = std::format("consumed_bits{}", ident);
                            w.writeln("let mut ", read_size, " = 0;");
                            w.writeln("let mut ", consumed_bits, " = 0;");
                            auto array = std::format("tmp_array{}", ident);
                            w.writeln("let mut ", array, " = <[u8; ", std::format("{}", bit_size / 8), "]>::default();");
                        }
                    }
                    break;
                }
                case rebgn::AbstractOp::END_DECODE_PACKED_OPERATION: {
                    ctx.bm_ctx.inner_bit_operations = false;
                    break;
                }
                case rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION: {
                    ctx.bm_ctx.inner_bit_operations = false;
                    ctx.bit_field_ident.pop_back();
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::IF: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln("if ", evaluated.back(), " {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELIF: {
                    defer.pop_back();
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln("} else if(", evaluated.back(), ") {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELSE: {
                    defer.pop_back();
                    w.writeln("} else {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::MATCH:
                case rebgn::AbstractOp::EXHAUSTIVE_MATCH: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln("match ", evaluated.back(), " {");
                    defer.push_back([&, s = w.indent_scope_ex(), op = code.op]() mutable {
                        w.writeln("_ => {},");
                        s.execute();
                        w.writeln("}");
                    });
                    break;
                }
                case rebgn::AbstractOp::CASE: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln(evaluated.back(), " => {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_CASE: {
                    defer.pop_back();
                    w.writeln("},");
                    break;
                }
                case rebgn::AbstractOp::DEFAULT_CASE: {
                    w.writeln("_ => {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_MATCH: {
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::ASSERT: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln("if(!", evaluated.back(), ") { ");
                    w.writeln("return Err(Error::AssertError(\"", futils::escape::escape_str<std::string>(evaluated.back()), "\"));");
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FUNCTION: {
                    auto fn_range = ctx.ident_range_table[code.ident().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);

                    auto ident = ctx.ident_table[code.ident().value().value()];
                    futils::helper::DynDefer impl_scope;
                    if (auto bl = code.belong().value().value(); bl) {
                        auto typ = retrieve_union_type(ctx, ctx.bm.code[ctx.ident_index_table[bl]]);
                        w.writeln("impl ", typ, " {");
                        impl_scope = w.indent_scope_ex();
                    }
                    TmpCodeWriter params;
                    NeedTrait trait = NeedTrait::none;
                    rebgn::EncodeParamFlags encode_flags;
                    rebgn::DecodeParamFlags decode_flags;
                    auto add_write_trait = [&] {
                        if (encode_flags.has_seek()) {
                            w.write(" + ", ctx.Seek);
                        }

                        if (ctx.use_async) {
                            w.write(" + std::marker::Unpin");
                        }
                    };
                    auto add_read_trait = [&] {
                        if (decode_flags.has_seek()) {
                            w.write(" + ", ctx.Seek);
                        }
                        if (decode_flags.has_eof() || (decode_flags.has_peek() && !ctx.use_buf_read_peek)) {
                            w.write(" + ", ctx.BufRead);
                        }
                        if (ctx.use_async) {
                            w.write(" + std::marker::Unpin");
                        }
                    };
                    auto may_insert_async = [&] {
                        if (ctx.use_async &&
                            (trait == NeedTrait::encoder || trait == NeedTrait::decoder)) {
                            w.write("async ");
                        }
                    };
                    add_function_parameters(ctx, params, fn_range, trait, encode_flags, decode_flags);
                    if (trait == NeedTrait::encoder) {
                        w.write("pub ");
                        may_insert_async();
                        w.writeln("fn encode_to_vec(&self) -> std::result::Result<Vec<u8>, Error> {");
                        auto scope = w.indent_scope();
                        std::string new_cursor = "std::io::Cursor::new(Vec::new())";
                        w.writeln("let mut w = ", new_cursor, ";");
                        std::string pass = "w";
                        if (ctx.use_async) {
                            pass = "std::pin::Pin::new(&mut w)";
                        }
                        w.writeln("self.encode(&mut ", pass, ")", may_insert_await(ctx), "?;");
                        w.writeln("Ok(w.into_inner())");
                        scope.execute();
                        w.writeln("}");
                    }
                    else if (trait == NeedTrait::decoder) {
                        w.write("pub ");
                        may_insert_async();
                        w.writeln("fn decode_exact");
                        w.write("(data :&[u8]) -> Result<Self, Error> {");
                        auto scope = w.indent_scope();
                        if (decode_flags.has_peek() && ctx.use_buf_read_peek) {  // use BufReader
                            w.writeln("let mut r = ", ctx.BufReader, "::new(std::io::Cursor::new(data));");
                        }
                        else {
                            w.writeln("let mut r = std::io::Cursor::new(data);");
                        }
                        w.writeln("let mut result = Self::default();");
                        std::string pass = "&mut " + may_wrap_pin(ctx, "r");
                        w.writeln("result.decode(", pass, ")", may_insert_await(ctx), "?;");
                        w.writeln("Ok(result)");
                        scope.execute();
                        w.writeln("}");
                    }
                    w.write("pub ");
                    may_insert_async();
                    w.write("fn ");
                    if (trait == NeedTrait::property_setter) {
                        w.write("set_");
                    }
                    w.write(ident);
                    if (trait == NeedTrait::encoder) {
                        w.write("<W: ", ctx.Write);
                        add_write_trait();
                        w.write(">");
                    }
                    else if (trait == NeedTrait::decoder) {
                        w.write("<R: ", ctx.Read);
                        add_read_trait();
                        w.write(">");
                    }
                    w.write("(");
                    w.write(params.out());
                    w.write(") ");
                    if (ret) {
                        auto storage = *ctx.bm.code[*ret].storage();
                        auto type = type_to_string(ctx, storage);
                        w.write("-> ", type);
                    }
                    w.writeln(" {");
                    if (trait == NeedTrait::decoder) {
                        if (decode_flags.has_peek() && ctx.use_buf_read_peek) {
                            w.writeln("use ", ctx.Read, ";");
                            if (decode_flags.has_seek() || decode_flags.has_sub_range()) {
                                w.writeln("use ", ctx.Seek, ";");
                            }
                            if (decode_flags.has_eof() || decode_flags.has_sub_range()) {
                                w.writeln("use ", ctx.BufRead, ";");
                            }
                        }
                        else if (decode_flags.has_sub_range()) {
                            w.writeln("use ", ctx.Read, ";");
                            w.writeln("use ", ctx.Seek, ";");
                            w.writeln("use ", ctx.BufRead, ";");
                        }
                    }
                    defer.push_back(futils::helper::defer_ex([&, f = w.indent_scope_ex(), s = std::move(impl_scope)]() mutable {
                        f.execute();
                        w.writeln("}");
                        if (s) {
                            s.execute();
                            w.writeln("}");
                        }
                    }));
                    break;
                }
                case rebgn::AbstractOp::END_FUNCTION: {
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::END_IF:
                case rebgn::AbstractOp::END_LOOP: {
                    defer.pop_back();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::ASSIGN: {
                    auto s = eval(code, ctx);
                    w.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::APPEND: {
                    auto s = eval(code, ctx);
                    w.writeln(s.back());
                    break;
                }
                case rebgn::AbstractOp::INC: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln(evaluated.back(), "+= 1;");
                    break;
                }
                case rebgn::AbstractOp::CALL_ENCODE:
                case rebgn::AbstractOp::CALL_DECODE: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        if (code.op == rebgn::AbstractOp::CALL_ENCODE) {
                            encode_bit_field(ctx, w, code.bit_size_plus()->value() - 1, code.right_ref().value().value());
                        }
                        else {
                            decode_bit_field(ctx, w, code.bit_size_plus()->value() - 1, code.right_ref().value().value());
                        }
                        break;
                    }
                    auto ref = code.right_ref().value().value();
                    ctx.on_assign = code.op == rebgn::AbstractOp::CALL_DECODE;
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.on_assign = false;
                    auto name = ctx.ident_table[code.left_ref().value().value()];
                    w.write(evaluated.back(), ".", name, "(");
                    auto range = ctx.ident_range_table[code.left_ref().value().value()];
                    code_call_parameter(ctx, w, range);
                    w.writeln(")", may_insert_await(ctx), "?;");
                    break;
                }
                case rebgn::AbstractOp::BREAK: {
                    w.writeln("break;");
                    break;
                }
                case rebgn::AbstractOp::CONTINUE: {
                    w.writeln("continue;");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_VARIABLE: {
                    auto s = eval(code, ctx);
                    w.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::DECLARE_VARIABLE: {
                    auto s = eval(code, ctx);
                    w.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::BACKWARD_INPUT:
                case rebgn::AbstractOp::BACKWARD_OUTPUT: {
                    auto target = code.op == rebgn::AbstractOp::BACKWARD_INPUT ? "r" : "w";
                    auto ref = code.ref().value().value();
                    auto count = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    auto tmp = std::format("tmp{}", ref);
                    auto belong = code.op == rebgn::AbstractOp::BACKWARD_INPUT ? "input" : "output";
                    w.writeln("let ", tmp, " = ", target, ".stream_position()", map_io_error(ctx, belong), ";");
                    w.writeln(target, ".seek(std::io::SeekFrom::Start((", tmp, " - ", count.back(), ") as u64))", map_io_error(ctx, belong), ";");
                    auto tmp2 = std::format("tmp2{}", ref);
                    w.writeln("let ", tmp2, " = ", target, ".stream_position()", map_io_error(ctx, belong), ";");
                    w.writeln("if(", tmp2, " != (", tmp, " - ", count.back(), ") as u64) {");
                    auto scope = w.indent_scope();
                    w.writeln("return Err(", ctx.error_type, "::BackwardError(", tmp2, " as usize, (", tmp, " - ", count.back(), ") as usize));");
                    scope.execute();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        encode_bit_field(ctx, w, code.bit_size()->value(), code.ref().value().value());
                    }
                    else {
                        auto ref = code.ref().value().value();
                        auto& ident = ctx.ident_index_table[ref];
                        auto s = eval(ctx.bm.code[ident], ctx);
                        auto endian = code.endian().value();
                        auto is_big = endian.endian == rebgn::Endian::little ? false : true;
                        auto belong_name = get_belong_name(ctx, code);
                        serialize(ctx, w, code.bit_size()->value(), belong_name, s.back(), endian);
                    }
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        decode_bit_field(ctx, w, code.bit_size()->value(), code.ref().value().value());
                    }
                    else {
                        auto ref = code.ref().value().value();
                        auto& ident = ctx.ident_index_table[ref];
                        ctx.on_assign = true;
                        auto s = eval(ctx.bm.code[ident], ctx);
                        ctx.on_assign = false;
                        auto endian = code.endian().value();
                        auto belong_name = get_belong_name(ctx, code);
                        deserialize(ctx, w, ref, code.bit_size()->value(), belong_name, s.back(), endian);
                    }
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT_VECTOR: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    auto belong_name = get_belong_name(ctx, code);
                    w.writeln(std::format("if({}.len() != {} as usize) {{", vec.back(), len.back()));
                    auto scope = w.indent_scope();
                    w.writeln("return Err(", ctx.error_type, "::ArrayLengthMismatch(\"encode ", belong_name, "\",", vec.back(), ".len(),", len.back(), " as usize));");
                    scope.execute();
                    w.writeln("}");
                    if (bit_size == 8) {
                        w.writeln("w.write_all(&", vec.back(), ")", map_io_error(ctx, belong_name), ";");
                    }
                    else {
                        auto tmp = std::format("i_{}", ref_to_vec);
                        w.writeln("for & ", tmp, " in ", vec.back(), " {");
                        auto scope = w.indent_scope();
                        serialize(ctx, w, bit_size, tmp, belong_name, code.endian().value());
                        scope.execute();
                        w.writeln("}");
                    }
                    break;
                }
                case rebgn::AbstractOp::PEEK_INT_VECTOR: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    ctx.on_assign = true;
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    ctx.on_assign = false;
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    auto tmp = std::format("tmp_{}", ref_to_vec);
                    if (bit_size == 8) {
                        if (ctx.current_r.empty()) {
                            if (ctx.use_buf_read_peek) {
                                w.writeln("let ", tmp, " = ", ctx.r(), ".peek(", len.back(), " as usize)", map_io_error(ctx, get_belong_name(ctx, code)), ";");
                            }
                            else {
                                w.writeln("//WARNING: fill_buf may be returns less than expected even if more data is available when some scenario");
                                w.writeln("//         so it may cause infinite block in some case. use `config.rust.buf_reader_peek = true` to enable use 'peek()' instead if unstable feature is enabled");
                                w.writeln("let ", tmp, " = ", ctx.r(), ".fill_buf()", map_io_error(ctx, get_belong_name(ctx, code)), ";");
                            }
                        }
                        else {
                            w.writeln("let ", tmp, " = ", ctx.r(), ".fill_buf()", map_io_error(ctx, get_belong_name(ctx, code)), ";");
                        }
                        w.writeln("if ", tmp, ".len() < ", len.back(), " as usize {");
                        auto scope = w.indent_scope();
                        w.writeln("return Err(", ctx.error_type, "::ArrayLengthMismatch(\"peek ", get_belong_name(ctx, code), "\",", tmp, ".len(),", len.back(), " as usize));");
                        scope.execute();
                        w.writeln("}");
                        w.writeln(vec.back(), ".copy_from_slice(&", tmp, "[..", len.back(), " as usize]);");
                    }
                    else {
                        w.writeln("/* Unimplemented peek int vector */");
                    }
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.ref().value().value();
                    ctx.on_assign = true;
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    ctx.on_assign = false;
                    if (bit_size == 8) {
                        w.writeln(ctx.r(), ".read_to_end(", "&mut ", vec.back(), ")", map_io_error(ctx, get_belong_name(ctx, code)), ";");
                    }
                    else {
                        auto tmp = std::format("tmp_hold_{}", ref_to_vec);
                        w.writeln("let mut ", tmp, " = Vec::new();");
                        w.writeln(ctx.r(), ".read_to_end(", ",&mut ", tmp, ")", map_io_error(ctx, get_belong_name(ctx, code)), ";");
                        w.writeln("if ", tmp, ".len() % ", std::format("{}", bit_size / 8), " != 0 {");
                        auto scope = w.indent_scope();
                        w.writeln("return Err(", ctx.error_type, "::ArrayLengthMismatch(\"decode ", get_belong_name(ctx, code), "\",", tmp, ".len(),", std::format("{}", bit_size / 8), "));");
                        scope.execute();
                        auto tmp_i = std::format("tmp_i{}", ref_to_vec);
                        w.writeln("for ", tmp_i, " in 0..", tmp, ".len() / ", std::format("{}", bit_size / 8), "{");
                        auto scope2 = w.indent_scope();
                        w.writeln("let mut ", tmp, " = 0;");
                        w.writeln("for i in 0..", std::format("{}", bit_size / 8), "{");
                        auto scope3 = w.indent_scope();
                        w.writeln(tmp, " |= (", tmp, "[", tmp_i, " * ", std::format("{}", bit_size / 8), " + i] as u", std::format("{}", bit_size), ") << 8 * (", std::format("{}", bit_size / 8 - 1), " - i);");
                        scope3.execute();
                        w.writeln("}");
                        w.writeln(vec.back(), ".push_back(", tmp, ");");
                        scope2.execute();
                        w.writeln("}");
                    }
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR:
                case rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    ctx.on_assign = true;
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    ctx.on_assign = false;
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    auto belong_name = get_belong_name(ctx, code);
                    bool is_fixed = code.op == rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED;
                    if (is_fixed) {
                        auto& imm = ctx.bm.code[ctx.ident_index_table[ref_to_len]];
                        if (imm.op == rebgn::AbstractOp::IMMEDIATE_INT64) {
                            is_fixed = false;
                        }
                        else if (imm.int_value()->value() > rust_impl_Default_threshold) {
                            is_fixed = false;
                        }
                    }
                    if (bit_size == 8) {
                        if (is_fixed) {
                            w.writeln(std::format("{}.read_exact(&mut {}){};", ctx.r(), vec.back(), map_io_error(ctx, belong_name)));
                        }
                        else {
                            w.writeln(std::format("{}.resize({} as usize,0);", vec.back(), len.back()));
                            w.writeln(std::format("{}.read_exact(&mut {}){};", ctx.r(), vec.back(), map_io_error(ctx, belong_name)));
                        }
                    }
                    else {
                        auto endian = code.endian().value();
                        auto is_big = endian.endian == rebgn::Endian::little ? false : true;
                        auto tmp_i = std::format("i_{}", ref_to_vec);
                        w.writeln("for ", tmp_i, " in 0..", len.back(), "{");
                        auto scope = w.indent_scope();
                        if (is_fixed) {
                            deserialize(ctx, w, ref_to_vec, bit_size, belong_name, std::format("{}[{}]", vec.back(), tmp_i), endian);
                        }
                        else {
                            auto tmp = std::format("tmp_hold_{}", ref_to_vec);
                            w.writeln(std::format("std::uint{}_t {};", bit_size, tmp));
                            deserialize(ctx, w, ref_to_vec, bit_size, belong_name, tmp, endian);
                            w.writeln(std::format("{}.push_back({});", vec.back(), tmp));
                        }
                        scope.execute();
                        w.writeln("}");
                    }
                    break;
                }
                case rebgn::AbstractOp::RET: {
                    auto ref = code.ref().value().value();
                    if (ref == 0) {
                        w.writeln("return;");
                    }
                    else {
                        auto& ident = ctx.ident_index_table[ref];
                        auto s = eval(ctx.bm.code[ident], ctx);
                        w.writeln("return ", s.back(), ";");
                    }
                    break;
                }
                case rebgn::AbstractOp::RET_SUCCESS: {
                    w.writeln("return Ok(());");
                    break;
                }
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_OK: {
                    w.writeln("return Ok(());");
                    break;
                }
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_FAIL: {
                    auto belong = code.belong().value().value();
                    auto belong_name = ctx.ident_table[belong];
                    w.writeln("return Err(", ctx.error_type, "::PropertySetterError(\"", belong_name, "\"));");
                    break;
                }
                case rebgn::AbstractOp::LOOP_INFINITE: {
                    w.writeln("loop {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::LOOP_CONDITION: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    w.writeln("while(", evaluated.back(), ") {");
                    defer.push_back(w.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::CHECK_UNION:
                case rebgn::AbstractOp::SWITCH_UNION: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    auto ref = code.ref().value().value();  // ref to UNION_MEMBER
                    if (find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[ref]], rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        break;
                    }
                    auto variant_name = retrieve_union_type(ctx, ctx.bm.code[ctx.ident_index_table[ref]]);
                    // ref to DEFINE_UNION
                    auto union_belong = ctx.bm.code[ctx.ident_index_table[ref]].belong().value().value();
                    auto belong_name = std::format("Variant{}", union_belong);
                    // ref to FIELD
                    auto field_ref = ctx.bm.code[ctx.ident_index_table[union_belong]].belong().value().value();
                    auto val = eval(ctx.bm.code[ctx.ident_index_table[field_ref]], ctx);
                    w.writeln(std::format("if !matches!({},{}::{}(_)) {{", val.back(), belong_name, variant_name));
                    auto scope = w.indent_scope();
                    if (code.op == rebgn::AbstractOp::CHECK_UNION) {
                        auto check_at = code.check_at().value();
                        if (check_at == rebgn::UnionCheckAt::ENCODER) {
                            w.writeln("return Err(Error::InvalidUnionVariant(\"", belong_name, "::", variant_name, "\"));");
                        }
                        else {
                            w.writeln("return None;");
                        }
                    }
                    else {
                        w.writeln(std::format("{} = {}::{}({}::default());", val.back(), belong_name, variant_name, variant_name));
                    }
                    scope.execute();
                    w.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::BEGIN_ENCODE_SUB_RANGE: {
                    auto ref = code.ref().value().value();
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    auto belong_name = get_belong_name(ctx, code);
                    auto tmp_array = std::format("tmp_array{}", ref);
                    std::string new_vec = may_wrap_pin(ctx, "Vec::with_capacity(" + len.back() + " as usize)");
                    w.writeln("let mut ", tmp_array, " = ", new_vec, ";");
                    ctx.current_w.push_back(std::move(tmp_array));
                    defer.push_back(futils::helper::defer_ex([&, ref = ref, len]() mutable {
                        auto tmp_array = std::move(ctx.current_w.back());
                        ctx.current_w.pop_back();
                        w.writeln("if ", tmp_array, ".len() != ", len.back(), " as usize {");
                        auto scope = w.indent_scope();
                        w.writeln("return Err(", ctx.error_type, "::ArrayLengthMismatch(\"encode ", tmp_array, "\",", len.back(), " as usize,", tmp_array, ".len()));");
                        scope.execute();
                        w.writeln("}");
                        w.writeln("w.write_all(&", tmp_array, ")", map_io_error(ctx, belong_name), ";");
                    }));
                    break;
                }
                case rebgn::AbstractOp::BEGIN_DECODE_SUB_RANGE: {
                    auto ref = code.ref().value().value();
                    auto belong_name = get_belong_name(ctx, code);
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    auto tmp_array = std::format("tmp_array{}", ref);
                    w.writeln("let mut ", tmp_array, " :Vec<u8> = vec![0; ", len.back(), " as usize];");
                    w.writeln(ctx.r(), ".read_exact(", "&mut ", tmp_array, ")", map_io_error(ctx, belong_name), ";");
                    auto tmp_cursor = std::format("cursor{}", ref);
                    std::string new_cursor = may_wrap_pin(ctx, std::format("std::io::Cursor::new(&{})", tmp_array));
                    w.writeln("let mut ", tmp_cursor, " = ", new_cursor, ";");
                    ctx.current_r.push_back(std::move(tmp_cursor));
                    defer.push_back(futils::helper::defer_ex([&, ref = ref, len]() mutable {
                        auto tmp_cursor = std::move(ctx.current_r.back());
                        ctx.current_r.pop_back();
                        w.writeln("if ", tmp_cursor, ".position() != ", len.back(), " as u64 {");
                        auto scope = w.indent_scope();
                        w.writeln("return Err(", ctx.error_type, "::ArrayLengthMismatch(\"encode ", belong_name, "\",", len.back(), " as usize,", tmp_cursor, ".position() as usize));");
                        scope.execute();
                        w.writeln("}");
                    }));
                    break;
                }
                case rebgn::AbstractOp::END_ENCODE_SUB_RANGE: {
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::END_DECODE_SUB_RANGE: {
                    defer.pop_back();
                    break;
                }
                default:
                    w.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    std::string escape_rust_keyword(const std::string& keyword) {
        if (keyword == "as" ||
            keyword == "break" ||
            keyword == "const" ||
            keyword == "continue" ||
            keyword == "crate" ||
            keyword == "else" ||
            keyword == "enum" ||
            keyword == "extern" ||
            keyword == "false" ||
            keyword == "fn" ||
            keyword == "for" ||
            keyword == "if" ||
            keyword == "impl" ||
            keyword == "in" ||
            keyword == "let" ||
            keyword == "loop" ||
            keyword == "match" ||
            keyword == "mod" ||
            keyword == "move" ||
            keyword == "mut" ||
            keyword == "pub" ||
            keyword == "ref" ||
            keyword == "return" ||
            keyword == "self" ||
            keyword == "Self" ||
            keyword == "static" ||
            keyword == "struct" ||
            keyword == "super" ||
            keyword == "trait" ||
            keyword == "true" ||
            keyword == "type" ||
            keyword == "unsafe" ||
            keyword == "use" ||
            keyword == "where" ||
            keyword == "while" ||
            keyword == "async" ||
            keyword == "await" ||
            keyword == "dyn" ||
            keyword == "override") {
            return std::format("{}_", keyword);
        }
        return keyword;
    }

    void write_error_type(Context& ctx, auto&& w, const std::string& ident) {
        w.writeln("#[derive(Debug)]");
        w.writeln("pub enum ", ident, " {");
        auto scope = w.indent_scope();
        w.writeln("PropertySetterError(&'static str),");
        w.writeln("IOError(&'static str, std::io::Error),");
        w.writeln("TryFromIntError(std::num::TryFromIntError),");
        w.writeln("ArrayLengthMismatch(&'static str,usize /*expected*/,usize /*actual*/),");
        w.writeln("AssertError(&'static str),");
        w.writeln("InvalidUnionVariant(&'static str),");
        w.writeln("BackwardError(usize,usize),");
        scope.execute();
        w.writeln("}");

        w.writeln("impl std::fmt::Display for ", ident, " {");
        auto scope1 = w.indent_scope();
        w.writeln("fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {");
        auto scope12 = w.indent_scope();
        w.writeln("match self {");
        auto scope13 = w.indent_scope();
        w.writeln(ident, "::PropertySetterError(s) => write!(f, \"PropertySetterError: {}\", s),");
        w.writeln(ident, "::IOError(s,e) => write!(f, \"IOError: {} {}\", s,e),");
        w.writeln(ident, "::TryFromIntError(e) => write!(f, \"TryFromIntError: {}\", e),");
        w.writeln(ident, "::ArrayLengthMismatch(s,expected,actual) => write!(f, \"ArrayLengthMismatch: {} expected:{} actual:{}\", s,expected,actual),");
        w.writeln(ident, "::AssertError(s) => write!(f, \"AssertError: {}\", s),");
        w.writeln(ident, "::InvalidUnionVariant(s) => write!(f, \"InvalidUnionVariant: {}\", s),");
        w.writeln(ident, "::BackwardError(expected,actual) => write!(f, \"BackwardError: expected:{} actual:{}\", expected,actual),");
        scope13.execute();
        w.writeln("}");
        scope12.execute();
        w.writeln("}");
        scope1.execute();
        w.writeln("}");

        w.writeln("impl From<std::num::TryFromIntError> for ", ident, " {");
        auto scope4 = w.indent_scope();
        w.writeln("fn from(e: std::num::TryFromIntError) -> Self {");
        auto scope5 = w.indent_scope();
        w.writeln(ident, "::TryFromIntError(e)");
        scope5.execute();
        w.writeln("}");
        scope4.execute();
        w.writeln("}");

        w.writeln("impl From<std::convert::Infallible> for ", ident, " {");
        auto scope6 = w.indent_scope();
        w.writeln("fn from(_: std::convert::Infallible) -> Self {");
        auto scope7 = w.indent_scope();
        w.writeln("unreachable!()");
        scope7.execute();
        w.writeln("}");
        scope6.execute();
        w.writeln("}");
    }

    void to_rust(futils::binary::writer& w, const rebgn::BinaryModule& bm, bool enable_async) {
        Context ctx(w, bm);
        bool has_error = false;
        for (auto& sr : bm.strings.refs) {
            ctx.string_table[sr.code.value()] = sr.string.data;
        }
        for (auto& ir : bm.identifiers.refs) {
            auto escaped = escape_rust_keyword(ir.string.data);
            if (escaped == "Error") {
                has_error = true;
            }
            ctx.ident_table[ir.code.value()] = std::move(escaped);
        }
        for (auto& id : bm.ident_indexes.refs) {
            ctx.ident_index_table[id.ident.value()] = id.index.value();
        }

        for (auto& id : bm.ident_ranges.ranges) {
            ctx.ident_range_table[id.ident.value()] = rebgn::Range{.start = id.range.start.value(), .end = id.range.end.value()};
            auto& code = bm.code[id.range.start.value()];
        }
        for (auto& md : bm.metadata.refs) {
            ctx.metadata_table[md.code.value()] = md.string.data;
        }

        bool has_union = false;
        bool has_vector = false;
        bool has_recursive = false;
        bool has_bit_field = false;
        bool has_array = false;
        bool has_optional = false;
        bool has_peek = false;
        for (auto& code : bm.code) {
            if (code.op == rebgn::AbstractOp::DEFINE_UNION) {
                has_union = true;
            }
            if (code.op == rebgn::AbstractOp::DECLARE_BIT_FIELD) {
                has_bit_field = true;
            }
            if (code.op == rebgn::AbstractOp::MERGED_CONDITIONAL_FIELD &&
                code.merge_mode().value() == rebgn::MergeMode::COMMON_TYPE) {
                has_optional = true;
            }
            if (code.op == rebgn::AbstractOp::PEEK_INT_VECTOR) {
                has_peek = true;
            }
            if (auto s = code.storage()) {
                for (auto& storage : s->storages) {
                    if (storage.type == rebgn::StorageType::VECTOR) {
                        has_vector = true;
                        break;
                    }
                    if (storage.type == rebgn::StorageType::ARRAY) {
                        has_array = true;
                        break;
                    }
                    if (storage.type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                        has_recursive = true;
                        break;
                    }
                }
            }
            if (has_vector && has_union && has_recursive && has_array && has_optional && has_peek) {
                break;
            }
        }
        ctx.cw.writeln("// Code generated by bm2rust of https://github.com/on-keyday/rebrgen");
        if (has_error) {
            write_error_type(ctx, ctx.cw, "Error_");
            ctx.error_type = "Error_";
        }
        else {
            write_error_type(ctx, ctx.cw, "Error");
        }
        if (enable_async) {
            ctx.enable_async();
        }
        std::vector<futils::helper::DynDefer> defer;
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::METADATA: {
                        auto meta = code.metadata().value();
                        auto meta_str = ctx.metadata_table[meta.name.value()];
                        auto check_bool = [&](auto&& action) {
                            if (meta.expr_refs.size() == 0) {
                                return;
                            }
                            auto expr = meta.expr_refs[0].value();
                            auto& expr_code = bm.code[ctx.ident_index_table[expr]];
                            if (expr_code.op == rebgn::AbstractOp::IMMEDIATE_TRUE) {
                                action();
                            }
                        };
                        if (meta_str == "config.rust.async") {
                            check_bool([&] {
                                ctx.enable_async();
                            });
                        }
                        if (meta_str == "config.rust.buf_reader_peek") {
                            check_bool([&] {
                                ctx.use_buf_read_peek = true;
                            });
                        }
                        break;
                    }
                    case rebgn::AbstractOp::DEFINE_CONSTANT: {
                        auto right = code.ref().value().value();
                        auto s = eval(ctx.bm.code[ctx.ident_index_table[right]], ctx);
                        auto type = type_to_string(ctx, *code.storage());
                        ctx.cw.writeln("pub const ", ctx.ident_table[code.ident().value().value()], ": ", type, " = ", s.back(), ";");
                        break;
                    }
                    case rebgn::AbstractOp::DECLARE_ENUM: {
                        auto& ident = ctx.ident_table[code.ref().value().value()];
                        TmpCodeWriter tmp;
                        std::string base_type;
                        auto def = ctx.ident_index_table[code.ref().value().value()];
                        size_t count = 0;
                        std::vector<std::string> evaluated;
                        for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM; j++) {
                            if (bm.code[j].op == rebgn::AbstractOp::SPECIFY_STORAGE_TYPE) {
                                base_type = type_to_string(ctx, *bm.code[j].storage());
                                continue;
                            }
                            if (bm.code[j].op == rebgn::AbstractOp::DEFINE_ENUM_MEMBER) {
                                auto ident = ctx.ident_table[bm.code[j].ident().value().value()];
                                auto init = ctx.ident_index_table[bm.code[j].left_ref().value().value()];
                                auto ev = eval(bm.code[init], ctx);
                                if (count == 0) {
                                    tmp.writeln("#[default]");
                                }
                                count++;
                                tmp.writeln(ident, " = ", ev.back(), ",");
                                evaluated.push_back(std::move(ev.back()));
                            }
                        }
                        ctx.cw.writeln("#[derive(Debug,Default, Clone, Copy, PartialEq, Eq)]");
                        if (base_type.size() > 0) {
                            ctx.cw.writeln("#[repr(", base_type, ")]");
                        }
                        else {
                            base_type = "usize";
                        }
                        ctx.cw.write("pub enum ", ident);
                        ctx.cw.writeln(" {");
                        auto d = ctx.cw.indent_scope();
                        ctx.cw.write_unformatted(tmp.out());
                        d.execute();
                        ctx.cw.writeln("}");
                        ctx.cw.writeln("impl std::fmt::Display for ", ident, " {");
                        auto scope = ctx.cw.indent_scope();
                        ctx.cw.writeln("fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {");
                        auto scope1 = ctx.cw.indent_scope();
                        auto cast_to = base_type.size() ? base_type : "usize";
                        ctx.cw.writeln("match *self as ", cast_to, " {");
                        auto scope2 = ctx.cw.indent_scope();
                        count = 0;
                        for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM; j++) {
                            if (bm.code[j].op == rebgn::AbstractOp::DEFINE_ENUM_MEMBER) {
                                auto member = ctx.ident_table[bm.code[j].ident().value().value()];
                                auto str_ref = bm.code[j].right_ref().value().value();
                                if (str_ref != 0) {
                                    auto str = ctx.string_table[str_ref];
                                    ctx.cw.writeln(evaluated[count], " => write!(f, \"{}\", \"", str, "\"),");
                                }
                                else {
                                    ctx.cw.writeln(evaluated[count], " => write!(f, \"{}\", \"", member, "\"),");
                                }
                                count++;
                            }
                        }
                        ctx.cw.writeln("_ => write!(f, \"", ident, "({})\",*self as ", cast_to, "),");
                        scope2.execute();
                        ctx.cw.writeln("}");
                        scope1.execute();
                        ctx.cw.writeln("}");
                        scope.execute();
                        ctx.cw.writeln("}");

                        ctx.cw.writeln("impl ", ident, " {");
                        auto scope3 = ctx.cw.indent_scope();
                        ctx.cw.writeln("pub fn is_known(&self) -> bool {");
                        auto scope4 = ctx.cw.indent_scope();
                        ctx.cw.writeln("match *self as ", cast_to, " {");
                        auto scope5 = ctx.cw.indent_scope();
                        count = 0;
                        for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM; j++) {
                            if (bm.code[j].op == rebgn::AbstractOp::DEFINE_ENUM_MEMBER) {
                                auto member = ctx.ident_table[bm.code[j].ident().value().value()];
                                ctx.cw.writeln(evaluated[count], " => true,");
                                count++;
                            }
                        }
                        ctx.cw.writeln("_ => false,");
                        scope5.execute();
                        ctx.cw.writeln("}");
                        scope4.execute();
                        ctx.cw.writeln("}");
                        scope3.execute();
                        ctx.cw.writeln("}");
                        break;
                    }
                    default:
                        ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                        break;
                }
            }
        }
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            TmpCodeWriter tmp;
            inner_block(ctx, tmp, {.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});
            ctx.cw.write_unformatted(tmp.out());
        }
        for (auto& def : ctx.on_functions) {
            def.execute();
        }
        for (size_t j = 0; j < bm.ident_ranges.ranges.size(); j++) {
            auto range = ctx.ident_range_table[bm.ident_ranges.ranges[j].ident.value()];
            if (bm.code[range.start].op != rebgn::AbstractOp::DEFINE_FUNCTION) {
                continue;
            }
            if (is_bit_field_property(ctx, range)) {
                continue;
            }
            TmpCodeWriter tmp;
            inner_function(ctx, tmp, range);
            ctx.cw.write_unformatted(tmp.out());
        }
        for (auto& def : defer) {
            def.execute();
            ctx.cw.writeln("}");
        }
    }
}  // namespace bm2rust
