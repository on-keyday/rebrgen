/*license*/
#include "convert.hpp"
#include <core/ast/tool/sort.h>

namespace rebgn {
    void rebind_ident_index(Module& mod) {
        mod.ident_index_table.clear();
        for (auto& i : mod.code) {
            if (auto ident = i.ident(); ident) {
                mod.ident_index_table[ident->value()] = &i - mod.code.data();
            }
        }
    }

    struct Range {
        std::uint64_t start;
        std::uint64_t end;
    };

    using Ranges = std::vector<Range>;

    struct NestedRanges {
        Range range;
        std::vector<NestedRanges> nested;
    };

    Error extract_ranges(Module& mod, Range range, NestedRanges& current) {
        for (auto i = range.start; i < range.end; i++) {
            auto& c = mod.code[i];
            auto do_extract = [&](AbstractOp begin_op, AbstractOp end_op) {
                auto start = i;
                auto end = i + 1;
                auto nested = 0;
                for (auto j = i + 1; j < range.end; j++) {
                    if (mod.code[j].op == begin_op) {
                        nested++;
                        continue;
                    }
                    if (mod.code[j].op == end_op) {
                        if (nested) {
                            nested--;
                            continue;
                        }
                        end = j + 1;
                        break;
                    }
                }
                NestedRanges r = {{start, end}};
                // inner extract_ranges
                auto err = extract_ranges(mod, {start + 1, end - 1}, r);
                if (err) {
                    return err;
                }
                current.nested.push_back(std::move(r));
                i = end - 1;
                return none;
            };
            switch (c.op) {
                case AbstractOp::DEFINE_FUNCTION: {
                    auto err = do_extract(AbstractOp::DEFINE_FUNCTION, AbstractOp::END_FUNCTION);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_ENUM: {
                    auto err = do_extract(AbstractOp::DEFINE_ENUM, AbstractOp::END_ENUM);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_FORMAT: {
                    auto err = do_extract(AbstractOp::DEFINE_FORMAT, AbstractOp::END_FORMAT);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_FIELD: {
                    auto err = do_extract(AbstractOp::DEFINE_FIELD, AbstractOp::END_FIELD);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_ENUM_MEMBER: {
                    auto err = do_extract(AbstractOp::DEFINE_ENUM_MEMBER, AbstractOp::END_ENUM_MEMBER);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_UNION: {
                    auto err = do_extract(AbstractOp::DEFINE_UNION, AbstractOp::END_UNION);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_UNION_MEMBER: {
                    auto err = do_extract(AbstractOp::DEFINE_UNION_MEMBER, AbstractOp::END_UNION_MEMBER);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_PROGRAM: {
                    auto err = do_extract(AbstractOp::DEFINE_PROGRAM, AbstractOp::END_PROGRAM);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return none;
    }

    expected<AbstractOp> define_to_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DEFINE_FUNCTION:
                return AbstractOp::DECLARE_FUNCTION;
            case AbstractOp::DEFINE_ENUM:
                return AbstractOp::DECLARE_ENUM;
            case AbstractOp::DEFINE_FORMAT:
                return AbstractOp::DECLARE_FORMAT;
            case AbstractOp::DEFINE_UNION:
                return AbstractOp::DECLARE_UNION;
            case AbstractOp::DEFINE_FIELD:
                return AbstractOp::DECLARE_FIELD;
            case AbstractOp::DEFINE_ENUM_MEMBER:
                return AbstractOp::DECLARE_ENUM_MEMBER;
            case AbstractOp::DEFINE_UNION_MEMBER:
                return AbstractOp::DECLARE_UNION_MEMBER;
            case AbstractOp::DEFINE_PROGRAM:
                return AbstractOp::DECLARE_PROGRAM;
            default:
                return unexpect_error("Invalid op: {}", to_string(op));
        }
    }

    Error flatten_ranges(Module& m, NestedRanges& r, std::vector<Code>& flat, std::vector<Code>& outer_nested) {
        size_t outer_range = 0;
        for (size_t i = r.range.start; i < r.range.end; i++) {
            if (outer_range < r.nested.size() &&
                r.nested[outer_range].range.start == i) {
                auto decl = define_to_declare(m.code[i].op);
                if (!decl) {
                    return decl.error();
                }
                Code declare;
                declare.op = decl.value();
                declare.ref(m.code[i].ident().value());
                flat.push_back(std::move(declare));
                std::vector<Code> tmp;
                auto err = flatten_ranges(m, r.nested[outer_range], tmp, outer_nested);
                if (err) {
                    return err;
                }
                auto last = r.nested[outer_range].range.end - 1;
                outer_nested.insert(outer_nested.end(), tmp.begin(), tmp.end());
                i = last;
                outer_range++;
            }
            else {
                flat.push_back(std::move(m.code[i]));
            }
        }
        return none;
    }

    Error flatten(Module& m) {
        NestedRanges ranges = {{0, m.code.size()}};
        auto err = extract_ranges(m, ranges.range, ranges);
        if (err) {
            return err;
        }
        std::vector<Code> flat;
        std::vector<Code> outer_nested;
        flatten_ranges(m, ranges, flat, outer_nested);
        m.code = std::move(flat);
        m.code.insert(m.code.end(), outer_nested.begin(), outer_nested.end());
        return none;
    }

    Error bind_encoder_and_decoder(Module& m) {
        std::vector<Code> rebound;
        std::map<std::uint64_t, std::uint64_t> should_bind_encoder;
        std::map<std::uint64_t, std::uint64_t> should_bind_decoder;
        std::map<std::uint64_t, std::uint64_t> fmt_to_encoder;
        std::map<std::uint64_t, std::uint64_t> fmt_to_decoder;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                auto fmt_begin = c.left_ref().value().value();
                auto found = m.ident_index_table.find(fmt_begin);
                if (found == m.ident_index_table.end()) {
                    return error("Invalid format ident: {}", fmt_begin);
                }
                bool has_declare_in_fmt = false;
                for (auto fmt = found->second; fmt < m.code.size(); fmt++) {
                    if (m.code[fmt].op == AbstractOp::END_FORMAT) {
                        break;
                    }
                    if (m.code[fmt].op == AbstractOp::DECLARE_FUNCTION &&
                        m.code[fmt].ref().value().value() == c.right_ref().value().value()) {
                        has_declare_in_fmt = true;
                        break;
                    }
                }
                if (c.op == AbstractOp::DEFINE_ENCODER) {
                    if (!has_declare_in_fmt) {
                        should_bind_encoder[c.right_ref().value().value()] = c.left_ref().value().value();
                    }
                    fmt_to_encoder[c.left_ref().value().value()] = c.right_ref().value().value();
                }
                else {
                    if (!has_declare_in_fmt) {
                        should_bind_decoder[c.right_ref().value().value()] = c.left_ref().value().value();
                    }
                    fmt_to_decoder[c.left_ref().value().value()] = c.right_ref().value().value();
                }
            }
        }

        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_FORMAT) {
                auto ident = c.ident().value().value();
                rebound.push_back(std::move(c));
                auto found_encoder = fmt_to_encoder.find(ident);
                auto found_decoder = fmt_to_decoder.find(ident);
                if (found_encoder != fmt_to_encoder.end()) {
                    Code bind;
                    bind.op = AbstractOp::DECLARE_FUNCTION;
                    bind.ref(*varint(found_encoder->second));
                    rebound.push_back(std::move(bind));
                    bind.op = AbstractOp::DEFINE_ENCODER;
                    bind.left_ref(*varint(ident));
                    bind.right_ref(*varint(found_encoder->second));
                    rebound.push_back(std::move(bind));
                }
                if (found_decoder != fmt_to_decoder.end()) {
                    Code bind;
                    bind.op = AbstractOp::DECLARE_FUNCTION;
                    bind.ref(*varint(found_decoder->second));
                    rebound.push_back(std::move(bind));
                    bind.op = AbstractOp::DEFINE_DECODER;
                    bind.left_ref(*varint(ident));
                    bind.right_ref(*varint(found_decoder->second));
                    rebound.push_back(std::move(bind));
                }
            }
            else if (c.op == AbstractOp::DECLARE_FUNCTION) {
                if (auto found = should_bind_encoder.find(c.ref().value().value()); found != should_bind_encoder.end()) {
                    continue;  // skip; declaration is moved into DEFINE_FORMAT
                }
                if (auto found = should_bind_decoder.find(c.ref().value().value()); found != should_bind_decoder.end()) {
                    continue;  // skip; declaration is moved into DEFINE_FORMAT
                }
                rebound.push_back(std::move(c));
            }
            else if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                // skip
            }
            else {
                rebound.push_back(std::move(c));
            }
        }
        m.code = std::move(rebound);
        return none;
    }

    Error sort_formats(Module& m, const std::shared_ptr<ast::Program>& node) {
        ast::tool::FormatSorter sorter;
        auto sorted_formats = sorter.topological_sort(node);
    }

    Error optimize(Module& m, const std::shared_ptr<ast::Program>& node) {
        auto err = flatten(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = bind_encoder_and_decoder(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        return none;
    }
}  // namespace rebgn
