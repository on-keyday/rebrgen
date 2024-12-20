/*license*/
#include "convert.hpp"

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
            default:
                return unexpect_error("Invalid op: {}", to_string(op));
        }
    }

    Error flatten_ranges(Module& m, NestedRanges& r, std::vector<Code>& flat, std::vector<Code>& outer_nested) {
        size_t outer_range = 0;
        for (size_t i = r.range.start; i < r.range.end; i++) {
            if (outer_range < r.nested.size() &&
                r.nested[outer_range].range.start == i) {
                auto& r2 = r.nested[outer_range];
                size_t inner_nested = 0;
                for (size_t j = r2.range.start; j < r2.range.end; j++) {
                    if (inner_nested < r2.nested.size() &&
                        r2.nested[inner_nested].range.start == j) {
                        auto decl = define_to_declare(m.code[j].op);
                        if (!decl) {
                            return decl.error();
                        }
                        Code declare;
                        declare.op = decl.value();
                        declare.ref(m.code[j].ident().value());
                        flat.push_back(std::move(declare));
                        outer_nested.push_back(std::move(m.code[j]));
                        std::vector<Code> tmp;
                        auto err = flatten_ranges(m, r2.nested[inner_nested], outer_nested, tmp);
                        if (err) {
                            return err;
                        }
                        auto last = r2.nested[inner_nested].range.end - 1;
                        outer_nested.push_back(std::move(m.code[last]));
                        outer_nested.insert(outer_nested.end(), tmp.begin(), tmp.end());
                        j = last;
                        inner_nested++;
                    }
                    else {
                        flat.push_back(std::move(m.code[j]));
                    }
                }
                i = r2.range.end;
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

    Error optimize(Module& m) {
        auto err = flatten(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        return none;
    }
}  // namespace rebgn
