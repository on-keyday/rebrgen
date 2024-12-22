/*license*/
#include "bm2cpp.hpp"
#include <code/code_writer.h>
#include <unordered_map>
#include <format>

namespace bm2cpp {
    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;
        std::unordered_map<std::uint64_t, std::uint64_t> ident_index_table;

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
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::ENUM: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
        }
        return "void";
    }

    std::vector<std::string> eval_eval(const rebgn::Code& code, Context& ctx) {
        std::vector<std::string> res;
        switch (code.op) {
            case rebgn::AbstractOp::ASSIGN: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval_eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval_eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("{} = {};", left.back(), right.back()));
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT64:
                res.push_back(std::format("{}", code.int_value64().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_INT:
                res.push_back(std::format("{}", code.int_value().value().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_TRUE:
                res.push_back("true");
                break;
            case rebgn::AbstractOp::IMMEDIATE_FALSE:
                res.push_back("false");
                break;
            case rebgn::AbstractOp::DEFINE_FIELD: {
                auto& ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(ident);
                break;
            }
            case rebgn::AbstractOp::DEFINE_ENUM_MEMBER: {
                auto& ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(ident);
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto& ref_index = ctx.ident_index_table[code.ref().value().value()];
                res = eval_eval(ctx.bm.code[ref_index], ctx);
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
        return res;
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

        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value(); i < bm.programs.ranges[j].end.value(); i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::DECLARE_FORMAT: {
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
                                        auto ev = eval_eval(bm.code[j], ctx);
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
    }
}  // namespace bm2cpp
