/*license*/
#include "convert.hpp"
#include <core/ast/tool/sort.h>
#include <view/span.h>

namespace rebgn {
    void rebind_ident_index(Module& mod) {
        mod.ident_index_table.clear();
        for (auto& i : mod.code) {
            if (auto ident = i.ident(); ident) {
                mod.ident_index_table[ident->value()] = &i - mod.code.data();
            }
        }
    }

    struct RangeN {
        std::uint64_t start;
        std::uint64_t end;
    };

    struct NestedRanges {
        RangeN range;
        std::vector<NestedRanges> nested;
    };

    Error extract_ranges(Module& mod, RangeN range, NestedRanges& current) {
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

    bool is_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DECLARE_FUNCTION:
            case AbstractOp::DECLARE_ENUM:
            case AbstractOp::DECLARE_FORMAT:
            case AbstractOp::DECLARE_UNION:
            case AbstractOp::DECLARE_FIELD:
            case AbstractOp::DECLARE_ENUM_MEMBER:
            case AbstractOp::DECLARE_UNION_MEMBER:
            case AbstractOp::DECLARE_PROGRAM:
                return true;
            default:
                return false;
        }
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

    Error sort_formats(Module& m, const std::shared_ptr<ast::Node>& node) {
        ast::tool::FormatSorter sorter;
        auto sorted_formats = sorter.topological_sort(node);
        std::unordered_map<ObjectID, std::shared_ptr<ast::Format>> sorted;
        for (auto& f : sorted_formats) {
            auto ident = m.lookup_ident(f->ident);
            if (!ident) {
                return ident.error();
            }
            sorted[ident->value()] = f;
        }
        std::vector<RangeN> programs;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROGRAM) {
                auto start = i;
                for (; i < m.code.size(); i++) {
                    if (m.code[i].op == AbstractOp::END_PROGRAM) {
                        break;
                    }
                }
                programs.push_back({start, i + 1});
            }
        }
        struct RangeOrder {
            RangeN range;
            size_t order;
        };
        std::vector<RangeOrder> programs_order;
        for (auto p : programs) {
            std::vector<Code> reordered;
            std::vector<Code> formats;
            for (size_t i = p.start; i < p.end; i++) {
                if (m.code[i].op == AbstractOp::DECLARE_FORMAT) {
                    formats.push_back(std::move(m.code[i]));
                }
                else {
                    reordered.push_back(std::move(m.code[i]));
                }
            }
            size_t earliest = -1;
            std::ranges::sort(formats, [&](auto& a, auto& b) {
                auto ident_a = a.ref().value().value();
                auto ident_b = b.ref().value().value();
                auto found_a = sorted.find(ident_a);
                auto found_b = sorted.find(ident_b);
                if (found_a == sorted.end() || found_b == sorted.end()) {
                    return false;
                }
                auto index_a = std::find(sorted_formats.begin(), sorted_formats.end(), found_a->second);
                auto index_b = std::find(sorted_formats.begin(), sorted_formats.end(), found_b->second);
                if (earliest > index_a - sorted_formats.begin()) {
                    earliest = index_a - sorted_formats.begin();
                }
                if (earliest > index_b - sorted_formats.begin()) {
                    earliest = index_b - sorted_formats.begin();
                }
                return index_a < index_b;
            });
            auto end_program = std::move(reordered.back());
            reordered.pop_back();
            reordered.insert(reordered.end(), formats.begin(), formats.end());
            reordered.push_back(std::move(end_program));
            for (size_t i = p.start; i < p.end; i++) {
                m.code[i] = std::move(reordered[i - p.start]);
            }
            programs_order.push_back({
                .range = p,
                .order = earliest,
            });
        }
        std::ranges::sort(programs_order, [](auto& a, auto& b) {
            return a.order < b.order;
        });
        std::vector<Code> reordered;
        // first, append programs
        for (auto& p : programs_order) {
            auto start = reordered.size();
            for (size_t i = p.range.start; i < p.range.end; i++) {
                reordered.push_back(std::move(m.code[i]));
            }
            auto end = reordered.size();
            auto startV = varint(start);
            auto endV = varint(end);
            if (!startV || !endV) {
                return error("Failed to convert varint");
            }
            m.ranges.push_back({*startV, *endV});
        }
        // then, append the rest
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROGRAM) {
                for (; i < m.code.size(); i++) {
                    if (m.code[i].op == AbstractOp::END_PROGRAM) {
                        break;
                    }
                }
                continue;
            }
            if (m.code[i].op == AbstractOp::DECLARE_PROGRAM) {  // this is needless so remove
                continue;
            }
            reordered.push_back(std::move(m.code[i]));
        }
        m.code = std::move(reordered);
        return none;
    }

    Error search_encode_decode(Module& m, std::vector<size_t>& index_of_operation_and_loop, Varint fn) {
        auto& start = m.ident_index_table[fn.value()];
        index_of_operation_and_loop.push_back(start);  // for define function
        size_t i = start;
        for (; m.code[i].op != AbstractOp::END_FUNCTION; i++) {
            switch (m.code[i].op) {
                case AbstractOp::IF:
                case AbstractOp::ELIF:
                case AbstractOp::ELSE:
                case AbstractOp::END_IF:
                case AbstractOp::MATCH:
                case AbstractOp::EXHAUSTIVE_MATCH:
                case AbstractOp::CASE:
                case AbstractOp::DEFAULT_CASE:
                case AbstractOp::END_CASE:
                case AbstractOp::END_MATCH:
                case AbstractOp::LOOP_INFINITE:
                case AbstractOp::LOOP_CONDITION:
                case AbstractOp::END_LOOP:
                case AbstractOp::BREAK:
                case AbstractOp::CONTINUE:
                case AbstractOp::RET:
                case AbstractOp::ENCODE_INT:
                case AbstractOp::DECODE_INT:
                case AbstractOp::CALL_ENCODE:
                case AbstractOp::CALL_DECODE:
                    index_of_operation_and_loop.push_back(i);
                    break;
                default:
                    break;
            }
        }
        index_of_operation_and_loop.push_back(i);  // for end function
        return none;
    }

    void write_cfg_dot(futils::binary::writer& w, Module& m, std::shared_ptr<CFG>& cfg, std::uint64_t& id) {
    }

    void write_cfg(futils::binary::writer& w, Module& m) {
        w.write("digraph G {\n");
        std::uint64_t id = 0;
        std::map<std::shared_ptr<CFG>, std::uint64_t> node_id;
        std::map<std::uint64_t, std::shared_ptr<CFG>> fn_to_cfg;
        std::function<void(std::shared_ptr<CFG>&)> write_node = [&](std::shared_ptr<CFG>& cfg) {
            if (node_id.find(cfg) != node_id.end()) {
                return;
            }
            node_id[cfg] = id++;
            w.write(std::format("  {} [label=\"", node_id[cfg]));
            std::vector<std::shared_ptr<CFG>> callee;
            for (auto& i : cfg->basic_block) {
                w.write(std::format("{}", to_string(m.code[i].op)));
                if (m.code[i].op == AbstractOp::ENCODE_INT || m.code[i].op == AbstractOp::DECODE_INT) {
                    w.write(std::format(" {}", m.code[i].bit_size().value().value()));
                }
                if (m.code[i].op == AbstractOp::CALL_ENCODE || m.code[i].op == AbstractOp::CALL_DECODE) {
                    auto fmt = m.ident_index_table[m.code[i].left_ref().value().value()];
                    for (size_t j = fmt; m.code[j].op != AbstractOp::END_FORMAT; j++) {
                        if ((m.code[i].op == AbstractOp::CALL_ENCODE && m.code[j].op == AbstractOp::DEFINE_ENCODER) ||
                            (m.code[i].op == AbstractOp::CALL_DECODE && m.code[j].op == AbstractOp::DEFINE_DECODER)) {
                            auto ident = m.code[j].right_ref().value().value();
                            auto& call = fn_to_cfg[ident];
                            callee.push_back(call);
                            break;
                        }
                    }
                }
                w.write("\\n");
            }
            w.write("\"];\n");
            for (auto& c : callee) {
                write_node(c);
                w.write(std::format("  {} -> {} [style=dotted];\n", node_id[cfg], node_id[c]));
            }
            for (auto& n : cfg->next) {
                write_node(n);
                w.write(std::format("  {} -> {};\n", node_id[cfg], node_id[n]));
            }
        };
        for (auto& cfg : m.cfgs) {
            auto ident = m.code[cfg->basic_block[0]].ident().value().value();
            fn_to_cfg[ident] = cfg;
        }
        for (auto& cfg : m.cfgs) {
            write_node(cfg);
        }
        w.write("}\n");
    }

    struct ReservedBlock {
        std::shared_ptr<CFG> block;
        std::shared_ptr<CFG> end;
    };

    expected<std::shared_ptr<CFG>> make_control_flow_graph(Module& m, futils::view::rspan<size_t> index_of_operation_and_loop) {
        std::shared_ptr<CFG> root, current;
        root = current = std::make_shared<CFG>(CFG{});
        auto fn_end = std::make_shared<CFG>(CFG{});
        std::vector<ReservedBlock> loops;
        std::vector<ReservedBlock> ifs;
        std::vector<ReservedBlock> matchs;
        size_t i = 0;
        for (; i < index_of_operation_and_loop.size(); i++) {
            switch (m.code[index_of_operation_and_loop[i]].op) {
                case AbstractOp::DEFINE_FUNCTION: {
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_FUNCTION: {
                    current->next.push_back(fn_end);
                    fn_end->prev.push_back(current);
                    fn_end->basic_block.push_back(index_of_operation_and_loop[i]);
                    current = fn_end;
                    break;
                }
                case AbstractOp::ENCODE_INT:
                case AbstractOp::DECODE_INT:
                case AbstractOp::CALL_ENCODE:
                case AbstractOp::CALL_DECODE:
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                case AbstractOp::LOOP_INFINITE:
                case AbstractOp::LOOP_CONDITION: {
                    auto loop = std::make_shared<CFG>(CFG{});
                    current->next.push_back(loop);
                    loop->prev.push_back(current);
                    current = loop;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    current->next.push_back(end);
                    end->prev.push_back(current);
                    loops.push_back({loop, end});
                    auto block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(block);
                    block->prev.push_back(current);
                    current = block;
                    break;
                }
                case AbstractOp::END_LOOP: {
                    if (loops.size() == 0) {
                        return unexpect_error("Invalid loop");
                    }
                    auto loop = loops.back();
                    loops.pop_back();
                    auto next_block = loop.end;
                    current->next.push_back(loop.block);
                    loop.block->prev.push_back(current);
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::IF: {
                    auto if_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(if_block);
                    if_block->prev.push_back(current);
                    current = if_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    ifs.push_back({if_block, end});
                    auto then_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(then_block);
                    then_block->prev.push_back(current);
                    current = then_block;
                    break;
                }
                case AbstractOp::ELIF:
                case AbstractOp::ELSE: {
                    if (ifs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->next.push_back(ifs.back().end);
                    ifs.back().end->prev.push_back(current);
                    auto if_block = std::make_shared<CFG>(CFG{});
                    ifs.back().block->next.push_back(if_block);
                    if_block->prev.push_back(ifs.back().block);
                    current = if_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_IF: {
                    if (ifs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto if_block = ifs.back();
                    ifs.pop_back();
                    auto next_block = if_block.end;
                    auto has_else = false;
                    for (auto& n : if_block.block->next) {
                        if (n->basic_block.size() && m.code[n->basic_block[0]].op == AbstractOp::ELSE) {
                            has_else = true;
                            break;
                        }
                    }
                    if (!has_else) {
                        if_block.block->next.push_back(next_block);
                        next_block->prev.push_back(if_block.block);
                    }
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::MATCH:
                case AbstractOp::EXHAUSTIVE_MATCH: {
                    auto match_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(match_block);
                    match_block->prev.push_back(current);
                    current = match_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    if (m.code[index_of_operation_and_loop[i]].op == AbstractOp::MATCH) {
                        current->next.push_back(end);
                        end->prev.push_back(current);
                    }
                    matchs.push_back({match_block, end});
                    break;
                }
                case AbstractOp::CASE:
                case AbstractOp::DEFAULT_CASE: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto case_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(case_block);
                    case_block->prev.push_back(current);
                    current = case_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_CASE: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(matchs.back().end);
                    matchs.back().end->prev.push_back(current);
                    current = matchs.back().block;
                    break;
                }
                case AbstractOp::END_MATCH: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto match_block = matchs.back();
                    matchs.pop_back();
                    auto next_block = match_block.end;
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::RET: {
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(fn_end);
                    fn_end->prev.push_back(current);
                    break;
                }
                case AbstractOp::BREAK:
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(loops.back().end);
                    break;
                case AbstractOp::CONTINUE:
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(loops.back().block);
                    break;
                default:
                    break;
            }
        }
        return root;
    }

    Error insert_packed_operation(Module& m) {
        for (size_t i = 0; i < m.code.size(); i++) {
            auto& c = m.code[i];
            if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                auto fn = c.right_ref().value();
                std::vector<size_t> index_of_operation_and_loop;
                auto err = search_encode_decode(m, index_of_operation_and_loop, fn);
                if (err) {
                    return err;
                }
                auto cfg = make_control_flow_graph(m, index_of_operation_and_loop);
                if (!cfg) {
                    return cfg.error();
                }
                m.cfgs.push_back(cfg.value());
            }
        }
        return none;
    }

    Error optimize(Module& m, const std::shared_ptr<ast::Node>& node) {
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
        err = sort_formats(m, node);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = insert_packed_operation(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        return none;
    }
}  // namespace rebgn
