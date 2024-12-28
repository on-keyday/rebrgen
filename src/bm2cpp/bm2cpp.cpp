/*license*/
#include "bm2cpp.hpp"
#include <code/code_writer.h>
#include <unordered_map>
#include <format>
#include <bm/helper.hpp>
#include <algorithm>
#include <unicode/utf/convert.h>

namespace bm2cpp {
    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;
        std::unordered_map<std::uint64_t, std::uint64_t> ident_index_table;
        std::unordered_map<std::uint64_t, rebgn::Range> ident_range_table;
        std::string ptr_type;
        rebgn::BMContext bm_ctx;

        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm)
            : cw(w), bm(bm) {
        }
    };

    std::string type_to_string(Context& ctx, const rebgn::Storages& s, size_t index = 0) {
        if (s.storages.size() <= index) {
            return "void";
        }
        auto& storage = s.storages[index];
        switch (storage.type) {
            case rebgn::StorageType::CODER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::UINT: {
                auto size = storage.size().value().value();
                if (size <= 8) {
                    return "std::uint8_t";
                }
                else if (size <= 16) {
                    return "std::uint16_t";
                }
                else if (size <= 32) {
                    return "std::uint32_t";
                }
                else {
                    return "std::uint64_t";
                }
            }
            case rebgn::StorageType::INT: {
                auto size = storage.size().value().value();
                if (size <= 8) {
                    return "std::int8_t";
                }
                else if (size <= 16) {
                    return "std::int16_t";
                }
                else if (size <= 32) {
                    return "std::int32_t";
                }
                else {
                    return "std::int64_t";
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
                if (ctx.bm.code[idx].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                    return std::format("variant_{}", ref);
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
                return std::format("std::unique_ptr<{}>", ident);
            }
            case rebgn::StorageType::ENUM: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::VARIANT: {
                std::string variant = "std::variant<std::monostate";
                for (index++; index < s.storages.size(); index++) {
                    auto& storage = s.storages[index];
                    auto inner = type_to_string(ctx, s, index);
                    variant += ", " + inner;
                }
                variant += ">";
                return variant;
            }
            case rebgn::StorageType::ARRAY: {
                auto size = storage.size().value().value();
                auto inner = type_to_string(ctx, s, index + 1);
                return std::format("std::array<{}, {}>", inner, size);
            }
            case rebgn::StorageType::VECTOR: {
                auto inner = type_to_string(ctx, s, index + 1);
                return std::format("std::vector<{}>", inner);
            }
            case rebgn::StorageType::BOOL:
                return "bool";
        }
        return "void";
    }

    std::optional<size_t> find_op(Context& ctx, rebgn::Range& range, rebgn::AbstractOp op, size_t from = 0) {
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

    std::string retrieve_union_ident(Context& ctx, const rebgn::Code& code) {
        auto belong = ctx.ident_index_table[code.belong().value().value()];
        if (ctx.bm.code[belong].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
            auto union_range = ctx.ident_range_table[ctx.bm.code[belong].belong().value().value()];
            size_t get_index = 0;
            for (size_t i = union_range.start; i < union_range.end; i++) {
                if (ctx.bm.code[i].op == rebgn::AbstractOp::DECLARE_UNION_MEMBER) {
                    get_index++;
                    if (ctx.bm.code[i].ref().value().value() == code.belong().value().value()) {
                        break;
                    }
                }
            }
            auto& upper = ctx.bm.code[union_range.start];
            auto base = retrieve_union_ident(ctx, upper);
            return std::format("std::get<{}>({})", get_index, base);
        }
        if (ctx.bm.code[belong].op == rebgn::AbstractOp::DEFINE_FIELD) {
            return std::format("this->field_{}", ctx.bm.code[belong].ident().value().value());
        }
        return "/* Unimplemented union ident */";
    }

    std::vector<std::string> eval(const rebgn::Code& code, Context& ctx) {
        std::vector<std::string> res;
        switch (code.op) {
            case rebgn::AbstractOp::ASSIGN: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("{} = {};", left.back(), right.back()));
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
                    res.push_back(std::format("{}.{}", left.back(), right_ident));
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
                res.push_back(std::format("{}", str));
                break;
            }
            case rebgn::AbstractOp::STATIC_CAST: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto typ = code.storage().value();
                auto type = type_to_string(ctx, typ);
                res.push_back(std::format("static_cast<{}>({})", type, ref.back()));
                break;
            }
            case rebgn::AbstractOp::NEW_OBJECT: {
                auto storage = *code.storage();
                auto type = type_to_string(ctx, storage);
                res.push_back(std::format("{}{{}}", type));
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD: {
                auto ident = ctx.ident_table[code.ident().value().value()];
                auto range = ctx.ident_range_table[code.ident().value().value()];
                bool should_deref = false;
                auto found = find_op(ctx, range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                if (found) {
                    auto st = *ctx.bm.code[*found].storage();
                    if (st.storages.back().type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                        should_deref = true;
                    }
                }
                auto b = code.belong();
                auto belong_op = ctx.bm.code[ctx.ident_index_table[b->value()]].op;
                if (belong_op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                    ident = retrieve_union_ident(ctx, code) + "." + ident;
                }
                else if (belong_op == rebgn::AbstractOp::DEFINE_FORMAT) {
                    ident = "this->" + ident;
                }
                if (should_deref) {
                    res.push_back(std::format("(*{})", ident));
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
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto& ref_index = ctx.ident_index_table[code.ref().value().value()];
                res = eval(ctx.bm.code[ref_index], ctx);
                auto& taken = res.back();
                if (auto found = ctx.ident_table.find(code.ident().value().value()); found != ctx.ident_table.end()) {
                    taken = std::format("auto {} = {};", found->second, taken);
                    res.push_back(found->second);
                }
                else {
                    taken = std::format("auto tmp{} = {};", code.ident().value().value(), taken);
                    res.push_back(std::format("tmp{}", code.ident().value().value()));
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

    void add_function_parameters(Context& ctx, rebgn::Range range) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::ENCODER_PARAMETER) {
                ctx.cw.write("::futils::binary::writer& w");
                params++;
            }
            if (code.op == rebgn::AbstractOp::DECODER_PARAMETER) {
                ctx.cw.write("::futils::binary::reader& r");
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
                    ctx.cw.write(", ");
                }
                ctx.cw.write(type, "& ", ident);
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
                    ctx.cw.write(", ");
                }
                ctx.cw.write(type, " ", ident);
            }
        }
    }

    void bit_field(Context& ctx, rebgn::Range range) {
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
                    ctx.cw.writeln("::futils::binary::flags_t<");
                    break;
                }
                case rebgn::AbstractOp::END_BIT_FIELD: {
                    ctx.cw.write("> ");
                    auto name = ctx.bm.code[range.start].ident().value().value();
                    ctx.cw.writeln(std::format("field_{};", name));
                    break;
                }
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    void inner_block(Context& ctx, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::DECLARE_FORMAT ||
                code.op == rebgn::AbstractOp::DECLARE_UNION ||
                code.op == rebgn::AbstractOp::DECLARE_UNION_MEMBER ||
                code.op == rebgn::AbstractOp::DECLARE_STATE) {
                auto range = ctx.ident_range_table[code.ref().value().value()];
                inner_block(ctx, range);
                continue;
            }
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_UNION:
                case rebgn::AbstractOp::END_UNION:
                case rebgn::AbstractOp::DECLARE_ENUM: {
                    break;
                }
                case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
                    ctx.cw.writeln(std::format("struct variant_{} {{", code.ident().value().value()));
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_UNION_MEMBER: {
                    defer.pop_back();
                    ctx.cw.writeln("};");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_BIT_FIELD: {
                    bit_field(ctx, ctx.ident_range_table[code.ref().value().value()]);
                    break;
                }
                case rebgn::AbstractOp::DECLARE_PROPERTY: {
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
                        ctx.cw.writeln("/* Unimplemented field */");
                        break;
                    }
                    auto storage = *ctx.bm.code[*specify].storage();
                    auto type = type_to_string(ctx, storage);
                    ctx.cw.writeln(type, " ", ident, "{};");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FORMAT:
                case rebgn::AbstractOp::DEFINE_STATE: {
                    auto& ident = ctx.ident_table[code.ident().value().value()];
                    ctx.cw.writeln("struct ", ident, " {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FUNCTION: {
                    auto& ident = ctx.ident_table[code.ref().value().value()];
                    auto fn_range = ctx.ident_range_table[code.ref().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!ret) {
                        ctx.cw.write("void ", ident, "(");
                    }
                    else {
                        auto storage = *ctx.bm.code[*ret].storage();
                        auto type = type_to_string(ctx, storage);
                        ctx.cw.write(type, " ", ident, "(");
                    }
                    add_function_parameters(ctx, fn_range);
                    ctx.cw.writeln(");");
                    break;
                }
                case rebgn::AbstractOp::END_FORMAT:
                case rebgn::AbstractOp::END_STATE: {
                    defer.pop_back();
                    ctx.cw.writeln("};");
                    break;
                }
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    void inner_function(Context& ctx, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_FUNCTION: {
                    auto fn_range = ctx.ident_range_table[code.ident().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!ret) {
                        ctx.cw.write("void ");
                    }
                    else {
                        auto storage = *ctx.bm.code[*ret].storage();
                        auto type = type_to_string(ctx, storage);
                        ctx.cw.write(type, " ");
                    }
                    auto ident = ctx.ident_table[code.ident().value().value()];
                    if (auto bl = code.belong().value().value(); bl) {
                        auto parent_ident = ctx.ident_table[bl];
                        ident = std::format("{}::{}", parent_ident, ident);
                    }
                    ctx.cw.write(ident, "(");
                    add_function_parameters(ctx, fn_range);
                    ctx.cw.writeln(") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_FUNCTION: {
                    defer.pop_back();
                    ctx.cw.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::ASSIGN: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s.back());
                    break;
                }
                case rebgn::AbstractOp::DEFINE_VARIABLE: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT: {
                    auto ref = code.ref().value().value();
                    auto& ident = ctx.ident_index_table[ref];
                    auto s = eval(ctx.bm.code[ident], ctx);
                    auto endian = code.endian().value();
                    auto is_big = endian == rebgn::Endian::little ? false : true;
                    ctx.cw.writeln("if(!::futils::binary::write_num(w,", s.back(), ",", is_big ? "true" : "false", ")) { return false; }");
                    break;
                }
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    void to_cpp(futils::binary::writer& w, const rebgn::BinaryModule& bm) {
        Context ctx(w, bm);
        for (auto& sr : bm.strings.refs) {
            ctx.string_table[sr.code.value()] = sr.string.data;
        }
        for (auto& ir : bm.identifiers.refs) {
            ctx.ident_table[ir.code.value()] = ir.string.data;
        }
        for (auto& id : bm.ident_indexes.refs) {
            ctx.ident_index_table[id.ident.value()] = id.index.value();
        }

        for (auto& id : bm.ident_ranges.ranges) {
            ctx.ident_range_table[id.ident.value()] = rebgn::Range{.start = id.range.start.value(), .end = id.range.end.value()};
            auto& code = bm.code[id.range.start.value()];
        }
        bool has_union = false;
        bool has_vector = false;
        bool has_recursive = false;
        for (auto& code : bm.code) {
            if (code.op == rebgn::AbstractOp::DEFINE_UNION) {
                has_union = true;
            }
            if (auto s = code.storage()) {
                for (auto& storage : s->storages) {
                    if (storage.type == rebgn::StorageType::VECTOR) {
                        has_vector = true;
                        break;
                    }
                    if (storage.type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                        has_recursive = true;
                        break;
                    }
                }
            }
            if (has_vector && has_union && has_recursive) {
                break;
            }
        }
        ctx.cw.writeln("// Code generated by bm2cpp of https://github.com/on-keyday/rebrgen");
        ctx.cw.writeln("#pragma once");
        ctx.cw.writeln("#include <cstdint>");
        if (has_union) {
            ctx.cw.writeln("#include <variant>");
        }
        if (has_vector) {
            ctx.cw.writeln("#include <vector>");
        }
        if (has_recursive) {
            ctx.cw.writeln("#include <memory>");
        }
        ctx.cw.writeln("#include <binary/number.h>");
        std::vector<futils::helper::DynDefer> defer;
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::METADATA: {
                        auto meta = code.metadata();
                        auto str = ctx.string_table[meta->name.value()];
                        if (str == "config.cpp.namespace" && meta->expr_refs.size() == 1) {
                            if (auto found = ctx.string_table.find(meta->expr_refs[0].value()); found != ctx.string_table.end()) {
                                auto copy = found->second;
                                copy.erase(copy.begin());
                                copy.pop_back();
                                ctx.cw.writeln("namespace ", copy, " {");
                                defer.push_back(ctx.cw.indent_scope_ex());
                            }
                        }
                        break;
                    }
                    case rebgn::AbstractOp::DECLARE_FORMAT:
                    case rebgn::AbstractOp::DECLARE_STATE: {
                        auto& ident = ctx.ident_table[code.ref().value().value()];
                        ctx.cw.writeln("struct ", ident, ";");
                        break;
                    }
                    case rebgn::AbstractOp::DECLARE_ENUM: {
                        auto& ident = ctx.ident_table[code.ref().value().value()];
                        ctx.cw.write("enum class ", ident);
                        TmpCodeWriter tmp;
                        std::string base_type;
                        auto def = ctx.ident_index_table[code.ref().value().value()];
                        for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM; j++) {
                            if (bm.code[j].op == rebgn::AbstractOp::SPECIFY_STORAGE_TYPE) {
                                base_type = type_to_string(ctx, *bm.code[j].storage());
                                continue;
                            }
                            if (bm.code[j].op == rebgn::AbstractOp::DECLARE_ENUM_MEMBER) {
                                auto def = ctx.ident_index_table[bm.code[j].ref().value().value()];
                                for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM_MEMBER; j++) {
                                    if (bm.code[j].op == rebgn::AbstractOp::ASSIGN) {
                                        auto ev = eval(bm.code[j], ctx);
                                        ev.back().pop_back();  // remove ;
                                        ev.back().push_back(',');
                                        tmp.writeln(ev.back());
                                    }
                                }
                            }
                        }
                        if (base_type.size() > 0) {
                            ctx.cw.write(" : ", base_type);
                        }
                        ctx.cw.writeln(" {");
                        auto d = ctx.cw.indent_scope();
                        ctx.cw.write_unformatted(tmp.out());
                        d.execute();
                        ctx.cw.writeln("};");
                        break;
                    }
                    default:
                        ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                        break;
                }
            }
        }
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            inner_block(ctx, {.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});
        }
        for (size_t j = 0; j < bm.ident_ranges.ranges.size(); j++) {
            auto range = ctx.ident_range_table[bm.ident_ranges.ranges[j].ident.value()];
            if (bm.code[range.start].op != rebgn::AbstractOp::DEFINE_FUNCTION) {
                continue;
            }
            inner_function(ctx, range);
        }
        for (auto& def : defer) {
            def.execute();
            ctx.cw.writeln("}");
        }
    }
}  // namespace bm2cpp
