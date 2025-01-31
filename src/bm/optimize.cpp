/*license*/
#include "convert.hpp"
#include <core/ast/tool/sort.h>
#include <view/span.h>
#include "helper.hpp"
#include "internal.hpp"

namespace rebgn {
    void rebind_ident_index(Module& mod) {
        mod.ident_index_table.clear();
        for (auto& i : mod.code) {
            if (auto ident = i.ident(); ident) {
                mod.ident_index_table[ident->value()] = &i - mod.code.data();
            }
        }
    }

    Code make_code(AbstractOp op, auto&& set) {
        Code c;
        c.op = op;
        set(c);
        return c;
    }

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
                case AbstractOp::DEFINE_STATE: {
                    auto err = do_extract(AbstractOp::DEFINE_STATE, AbstractOp::END_STATE);
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
                case AbstractOp::DEFINE_BIT_FIELD: {
                    auto err = do_extract(AbstractOp::DEFINE_BIT_FIELD, AbstractOp::END_BIT_FIELD);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_PROPERTY: {
                    auto err = do_extract(AbstractOp::DEFINE_PROPERTY, AbstractOp::END_PROPERTY);
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
            case AbstractOp::DEFINE_UNION_MEMBER:
                return AbstractOp::DECLARE_UNION_MEMBER;
            case AbstractOp::DEFINE_PROGRAM:
                return AbstractOp::DECLARE_PROGRAM;
            case AbstractOp::DEFINE_STATE:
                return AbstractOp::DECLARE_STATE;
            case AbstractOp::DEFINE_BIT_FIELD:
                return AbstractOp::DECLARE_BIT_FIELD;
            case AbstractOp::DEFINE_PROPERTY:
                return AbstractOp::DECLARE_PROPERTY;
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
                flat.push_back(make_code(decl.value(), [&](auto& c) {
                    c.ref(m.code[i].ident().value());
                }));
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
        std::map<std::uint64_t, std::uint64_t> encoder_to_fmt;
        std::map<std::uint64_t, std::uint64_t> decoder_to_fmt;
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
                    encoder_to_fmt[c.right_ref().value().value()] = c.left_ref().value().value();
                }
                else {
                    if (!has_declare_in_fmt) {
                        should_bind_decoder[c.right_ref().value().value()] = c.left_ref().value().value();
                    }
                    fmt_to_decoder[c.left_ref().value().value()] = c.right_ref().value().value();
                    decoder_to_fmt[c.right_ref().value().value()] = c.left_ref().value().value();
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
                    rebound.push_back(make_code(AbstractOp::DEFINE_ENCODER, [&](auto& c) {
                        c.left_ref(*varint(ident));
                        c.right_ref(*varint(found_encoder->second));
                    }));
                    if (should_bind_encoder.find(found_encoder->second) != should_bind_encoder.end()) {
                        rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                            c.ref(*varint(found_encoder->second));
                        }));
                    }
                }
                if (found_decoder != fmt_to_decoder.end()) {
                    rebound.push_back(make_code(AbstractOp::DEFINE_DECODER, [&](auto& c) {
                        c.left_ref(*varint(ident));
                        c.right_ref(*varint(found_decoder->second));
                    }));
                    if (should_bind_decoder.find(found_decoder->second) != should_bind_decoder.end()) {
                        rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                            c.ref(*varint(found_decoder->second));
                        }));
                    }
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
            else if (c.op == AbstractOp::DEFINE_FUNCTION) {
                auto add_state_variables = [&](ObjectID fmt) {
                    auto ident = m.ident_table_rev.find(fmt);
                    if (ident == m.ident_table_rev.end()) {
                        return error("Invalid format ident: {}", fmt);
                    }
                    auto base = ast::as<ast::Format>(ident->second->base.lock());
                    if (!base) {
                        return error("Invalid format ident: {}", fmt);
                    }
                    for (auto& s : base->state_variables) {
                        auto lock = s.lock();
                        if (!lock) {
                            return error("Invalid state variable");
                        }
                        auto ident = m.lookup_ident(lock->ident);
                        if (!ident) {
                            return ident.error();
                        }
                        rebound.push_back(make_code(AbstractOp::STATE_VARIABLE_PARAMETER, [&](auto& c) {
                            c.ref(*ident);
                        }));
                    }
                    return none;
                };
                auto ident = c.ident().value();
                rebound.push_back(std::move(c));
                if (auto found = encoder_to_fmt.find(ident.value()); found != encoder_to_fmt.end()) {
                    rebound.push_back(make_code(AbstractOp::ENCODER_PARAMETER, [&](auto& c) {
                        c.left_ref(*varint(found->second));
                        c.right_ref(ident);
                    }));
                    add_state_variables(found->second);
                }
                if (auto found = decoder_to_fmt.find(ident.value()); found != decoder_to_fmt.end()) {
                    rebound.push_back(make_code(AbstractOp::DECODER_PARAMETER, [&](auto& c) {
                        c.left_ref(*varint(found->second));
                        c.right_ref(ident);
                    }));
                    add_state_variables(found->second);
                }
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
        std::vector<Range> programs;
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
            Range range;
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
            m.programs.push_back({*startV, *endV});
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

    Error remap_programs(Module& m) {
        m.programs.clear();
        for (auto& range : m.ident_to_ranges) {
            auto index = range.range.start.value();
            if (m.code[index].op == AbstractOp::DEFINE_PROGRAM) {
                m.programs.push_back(range.range);
            }
        }
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
                    index_of_operation_and_loop.push_back(i);  // for other operation
                    break;
            }
        }
        index_of_operation_and_loop.push_back(i);  // for end function
        return none;
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
            w.write(std::format("{} bit\\n", cfg->sum_bits));
            for (auto& i : cfg->basic_block) {
                w.write(std::format("{}", to_string(m.code[i].op)));
                if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                    if (auto found = m.ident_table_rev.find(m.code[i].ident().value().value());
                        found != m.ident_table_rev.end()) {
                        w.write(" ");
                        if (auto bs = m.ident_table_rev.find(m.code[i].belong().value().value());
                            bs != m.ident_table_rev.end()) {
                            w.write(bs->second->ident);
                            w.write(".");
                        }
                        w.write(found->second->ident);
                    }
                }
                if (m.code[i].op == AbstractOp::ENCODE_INT || m.code[i].op == AbstractOp::DECODE_INT) {
                    w.write(std::format(" {}", m.code[i].bit_size().value().value()));
                }
                if (m.code[i].op == AbstractOp::CALL_ENCODE || m.code[i].op == AbstractOp::CALL_DECODE) {
                    auto& call = fn_to_cfg[m.code[i].left_ref().value().value()];
                    callee.push_back(call);
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
                    current->sum_bits += m.code[index_of_operation_and_loop[i]].bit_size().value().value();
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                case AbstractOp::CALL_ENCODE:
                case AbstractOp::CALL_DECODE: {
                    auto plus = m.code[index_of_operation_and_loop[i]].bit_size_plus().value().value();
                    if (plus) {
                        current->sum_bits += plus - 1;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::LOOP_INFINITE:
                case AbstractOp::LOOP_CONDITION: {
                    if (current->basic_block.size()) {
                        auto loop = std::make_shared<CFG>(CFG{});
                        current->next.push_back(loop);
                        loop->prev.push_back(current);
                        current = loop;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    if (m.code[index_of_operation_and_loop[i]].op == AbstractOp::LOOP_CONDITION) {
                        current->next.push_back(end);
                        end->prev.push_back(current);
                    }
                    loops.push_back({current, end});
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
                    if (current->basic_block.size()) {
                        auto if_block = std::make_shared<CFG>(CFG{});
                        current->next.push_back(if_block);
                        if_block->prev.push_back(current);
                        current = if_block;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    ifs.push_back({current, end});
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
                    current->next.push_back(next_block);
                    next_block->prev.push_back(current);
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
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                case AbstractOp::BREAK: {
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    loops.back().end->prev.push_back(current);
                    current->next.push_back(loops.back().end);
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                case AbstractOp::CONTINUE: {
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    loops.back().block->next.push_back(loops.back().end);
                    loops.back().end->prev.push_back(loops.back().block);
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                default:
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
            }
        }
        return root;
    }

    Error generate_cfg1(Module& m) {
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

    Error add_packed_operation_for_cfg(Module& m) {
        std::unordered_map<size_t, std::shared_ptr<CFG>> index_to_cfg;
        std::set<std::shared_ptr<CFG>> visited;
        std::set<size_t> insert_define_packed_operation;
        std::set<size_t> insert_end_packed_operation;
        auto&& f = [&](auto&& f, std::shared_ptr<CFG>& cfg) -> Error {
            if (visited.find(cfg) != visited.end()) {
                return none;
            }
            visited.insert(cfg);
            cfg->sum_bits;
            return none;
        };
        for (auto& cfg : m.cfgs) {
            auto err = f(f, cfg);
            if (err) {
                return err;
            }
        }
        return none;
    }

    Error add_ident_ranges(Module& m) {
        for (size_t i = 0; i < m.code.size(); i++) {
            AbstractOp end_op;
            switch (m.code[i].op) {
                case AbstractOp::DEFINE_FUNCTION:
                    end_op = AbstractOp::END_FUNCTION;
                    break;
                case AbstractOp::DEFINE_ENUM:
                    end_op = AbstractOp::END_ENUM;
                    break;
                case AbstractOp::DEFINE_FORMAT:
                    end_op = AbstractOp::END_FORMAT;
                    break;
                case AbstractOp::DEFINE_STATE:
                    end_op = AbstractOp::END_STATE;
                    break;
                case AbstractOp::DEFINE_UNION:
                    end_op = AbstractOp::END_UNION;
                    break;
                case AbstractOp::DEFINE_UNION_MEMBER:
                    end_op = AbstractOp::END_UNION_MEMBER;
                    break;
                case AbstractOp::DEFINE_PROGRAM:
                    end_op = AbstractOp::END_PROGRAM;
                    break;
                case AbstractOp::DEFINE_BIT_FIELD:
                    end_op = AbstractOp::END_BIT_FIELD;
                    break;
                case AbstractOp::DEFINE_PROPERTY:
                    end_op = AbstractOp::END_PROPERTY;
                    break;
                default:
                    continue;
            }
            auto ident = m.code[i].ident().value();
            auto start = i;
            for (i = i + 1; i < m.code.size(); i++) {
                if (m.code[i].op == end_op) {
                    break;
                }
            }
            IdentRange range = {.ident = ident, .range = {.start = *varint(start), .end = *varint(i + 1)}};
            m.ident_to_ranges.push_back(std::move(range));
        }
        return none;
    }

    void replace_call_encode_decode_ref(Module& m) {
        for (auto& c : m.code) {
            if (c.op == AbstractOp::CALL_ENCODE || c.op == AbstractOp::CALL_DECODE) {
                auto fmt = m.ident_index_table[c.left_ref().value().value()];  // currently this refers to DEFINE_FORMAT
                for (size_t j = fmt; m.code[j].op != AbstractOp::END_FORMAT; j++) {
                    if ((c.op == AbstractOp::CALL_ENCODE && m.code[j].op == AbstractOp::DEFINE_ENCODER) ||
                        (c.op == AbstractOp::CALL_DECODE && m.code[j].op == AbstractOp::DEFINE_DECODER)) {
                        auto ident = m.code[j].right_ref().value();
                        c.left_ref(ident);  // replace to DEFINE_FUNCTION. this DEFINE_FUNCTION holds belong which is original DEFINE_FORMAT
                        break;
                    }
                }
            }
        }
    }

    struct InfiniteError {
        std::string message;

        void error(auto&& pb) {
            futils::strutil::append(pb, message);
        }
    };

    expected<size_t> decide_maximum_bit_field_size(Module& m, std::set<ObjectID>& searched, AbstractOp end_op, size_t index);

    expected<size_t> get_from_type(Module& m, const Storages& storage, std::set<ObjectID>& searched) {
        size_t factor = 1;
        for (size_t j = 0; j < storage.storages.size(); j++) {
            auto& s = storage.storages[j];
            if (s.type == StorageType::ARRAY) {
                factor *= s.size().value().value();
            }
            else if (s.type == StorageType::UINT || s.type == StorageType::INT) {
                factor *= s.size().value().value();
            }
            else if (s.type == StorageType::STRUCT_REF) {
                auto ref = s.ref().value().value();
                if (searched.find(ref) != searched.end()) {
                    return unexpect_error(InfiniteError{"Infinite: STRUCT_REF"});
                }
                searched.insert(ref);
                auto idx = m.ident_index_table[ref];
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_FORMAT, idx);
                searched.erase(ref);
                if (!size) {
                    return size.error();
                }
                factor *= size.value();
            }
            else if (s.type == StorageType::ENUM) {
                // skip
            }
            else if (s.type == StorageType::VARIANT) {
                size_t candidate = 0;
                for (j++; j < storage.storages.size(); j++) {
                    if (storage.storages[j].type != StorageType::STRUCT_REF) {
                        return unexpect_error("Invalid storage type: {}", to_string(storage.storages[j].type));
                    }
                    auto ref = storage.storages[j].ref().value().value();
                    if (searched.find(ref) != searched.end()) {
                        return unexpect_error(InfiniteError{"Infinite: VARIANT"});
                    }
                    searched.insert(ref);
                    auto idx = m.ident_index_table[ref];
                    auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_UNION_MEMBER, idx);
                    searched.erase(ref);
                    if (!size) {
                        return size.error();
                    }
                    candidate = std::max(candidate, size.value());
                }
                factor *= candidate;
            }
            else {
                return unexpect_error(InfiniteError{std::format("Invalid storage type: {}", to_string(s.type))});
            }
        }
        return factor;
    }

    expected<size_t> decide_maximum_bit_field_size(Module& m, std::set<ObjectID>& searched, AbstractOp end_op, size_t index) {
        size_t bit_size = 0;
        for (size_t i = index; m.code[i].op != end_op; i++) {
            /*
            if (m.code[i].op == AbstractOp::SPECIFY_STORAGE_TYPE) {
                auto ref = *m.code[i].type();
                auto got = m.get_storage(ref);
                if (!got) {
                    return unexpect_error(std::move(got.error()));
                }
                auto& storage = got.value();
                auto size = get_from_type(m, storage, searched);
                if (!size) {
                    return size.error();
                }
                bit_size += size.value();
            }
            */
            if (m.code[i].op == AbstractOp::DEFINE_FIELD) {
                auto ref = *m.code[i].type();
                auto got = m.get_storage(ref);
                if (!got) {
                    return unexpect_error(std::move(got.error()));
                }
                auto size = get_from_type(m, got.value(), searched);
                if (!size) {
                    return size.error();
                }
                bit_size += size.value();
            }
            if (m.code[i].op == AbstractOp::DECLARE_BIT_FIELD) {
                auto index = m.ident_index_table[m.code[i].ref().value().value()];
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_BIT_FIELD, index);
                if (!size) {
                    return size.error();
                }
                bit_size += size.value();
            }
        }
        return bit_size;
    }

    Error decide_bit_field_size(Module& m) {
        std::set<ObjectID> searched;
        std::unordered_map<ObjectID, size_t> bit_fields;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_BIT_FIELD) {
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_BIT_FIELD, i);
                if (!size) {
                    if (!size.error().as<InfiniteError>()) {
                        return size.error();
                    }
                    continue;
                }
                bit_fields[m.code[i].ident().value().value()] = size.value();
            }
        }
        std::vector<Code> rebound;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_BIT_FIELD) {
                auto ident = c.ident().value().value();
                auto found = bit_fields.find(ident);
                if (found != bit_fields.end()) {
                    Storages storage;
                    Storage s;
                    s.type = StorageType::UINT;
                    s.size(*varint(found->second));
                    storage.storages.push_back(std::move(s));
                    storage.length = *varint(1);
                    auto ref = m.get_storage_ref(storage, nullptr);
                    if (!ref) {
                        return ref.error();
                    }
                    c.type(*ref);
                }
            }
            rebound.push_back(std::move(c));
        }
        m.code = std::move(rebound);
        return none;
    }

    struct MergeCtx {
        std::unordered_map<std::string, Storages> type_to_storage;

        std::unordered_map<ObjectID, std::pair<std::string, std::vector<Varint>>> merged_conditional_fields;
        std::unordered_map<ObjectID, std::vector<ObjectID>> properties_to_merged;

        std::unordered_map<ObjectID, std::vector<std::pair<ObjectID /*new id*/, ObjectID /*merged conditional field*/>>> conditional_properties;

        std::set<ObjectID> reached;
        std::set<ObjectID> exists;
    };

    Error merge_conditional_inner(Module& m, MergeCtx& ctx, size_t start, AbstractOp end_op) {
        if (ctx.reached.find(start) != ctx.reached.end()) {
            return none;
        }
        ctx.reached.insert(start);
        std::unordered_map<ObjectID, std::unordered_map<std::string, std::vector<Varint>>> conditional_fields;
        std::set<ObjectID> exists;
        for (size_t i = start; m.code[i].op != end_op; i++) {
            auto& c = m.code[i];
            if (c.op == AbstractOp::CONDITIONAL_FIELD) {
                auto parent = c.belong().value().value();
                auto child = c.right_ref().value().value();
                auto target = c.ident().value();
                auto idx = m.ident_index_table[child];
                if (m.code[idx].op == AbstractOp::DEFINE_FIELD) {
                    auto type = m.code[idx].type().value().ref.value();
                    auto found = m.storage_key_table_rev.find(type);
                    if (found == m.storage_key_table_rev.end()) {
                        return error("Invalid storage key");
                    }
                    auto& key = found->second;
                    auto& map = conditional_fields[parent];
                    map[key].push_back(target);
                }
                else if (m.code[idx].op == AbstractOp::DEFINE_PROPERTY) {
                    auto err = merge_conditional_inner(m, ctx, idx + 1, AbstractOp::END_PROPERTY);
                    if (err) {
                        return err;
                    }
                    auto& merged = ctx.properties_to_merged[child];
                    for (auto& mc : merged) {
                        auto& mf = ctx.merged_conditional_fields[mc];
                        auto& map = conditional_fields[parent];
                        auto& key = mf.first;
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return new_id.error();
                        }
                        ctx.conditional_properties[target.value()].push_back({new_id->value(), mc});
                        map[key].push_back(*new_id);
                    }
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                exists.insert(c.ident().value().value());
                ctx.exists.insert(c.ident().value().value());
                auto param = c.param().value();
                auto found = m.storage_key_table_rev.find(c.type().value().ref.value());
                if (found == m.storage_key_table_rev.end()) {
                    return error("Invalid storage key");
                }
                auto& key = found->second;
                for (auto& p : param.expr_refs) {
                    auto& code = m.code[m.ident_index_table[p.value()]];
                    if (auto found = ctx.conditional_properties.find(code.ident()->value()); found != ctx.conditional_properties.end()) {
                        for (auto& mf : found->second) {
                            if (auto found2 = ctx.merged_conditional_fields.find(mf.second); found2 != ctx.merged_conditional_fields.end()) {
                                if (found2->second.first == key) {
                                    p = *varint(mf.first);
                                    break;
                                }
                            }
                        }
                    }
                }
                c.param(std::move(param));
            }
        }
        for (auto& cfield : conditional_fields) {
            for (auto& [key, value] : cfield.second) {
                bool exist = false;
                for (auto& e : exists) {
                    auto& c = m.code[m.ident_index_table[e]];
                    auto found = m.storage_key_table_rev.find(c.type().value().ref.value());
                    if (found == m.storage_key_table_rev.end()) {
                        return error("Invalid storage key");
                    }
                    auto& common_type_key = found->second;
                    auto param = c.param().value().expr_refs;
                    if (common_type_key == key && std::ranges::equal(param, value, [](auto& a, auto& b) {
                            return a.value() == b.value();
                        })) {
                        c.merge_mode(MergeMode::STRICT_COMMON_TYPE);
                        exist = true;
                        ctx.merged_conditional_fields[e] = {key, std::move(value)};
                        ctx.properties_to_merged[cfield.first].push_back(e);
                        break;
                    }
                }
                if (exist) {
                    continue;
                }
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return new_id.error();
                }
                ctx.merged_conditional_fields[new_id->value()] = {key, std::move(value)};
                ctx.properties_to_merged[cfield.first].push_back(new_id->value());
            }
        }
        return none;
    }

    Error merge_conditional_field(Module& m) {
        MergeCtx ctx;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DECLARE_PROPERTY) {
                auto index = m.ident_index_table[m.code[i].ref().value().value()];
                auto err = merge_conditional_inner(m, ctx, index + 1, AbstractOp::END_PROPERTY);
                if (err) {
                    return err;
                }
            }
        }
        std::vector<Code> rebound;
        std::optional<Varint> target;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROPERTY) {
                target = m.code[i].ident();
            }
            else if (m.code[i].op == AbstractOp::END_PROPERTY) {
                if (!target) {
                    return error("Invalid target");
                }
                auto found = ctx.properties_to_merged.find(target->value());
                if (found != ctx.properties_to_merged.end()) {
                    for (auto& mr : found->second) {
                        if (ctx.exists.find(mr) != ctx.exists.end()) {
                            continue;
                        }
                        auto& merged = ctx.merged_conditional_fields[mr];
                        Param param;
                        param.expr_refs = std::move(merged.second);
                        param.len_exprs = *varint(param.expr_refs.size());
                        auto found2 = m.storage_key_table.find(merged.first);
                        if (found2 == m.storage_key_table.end()) {
                            return error("Invalid storage key");
                        }
                        rebound.push_back(make_code(AbstractOp::MERGED_CONDITIONAL_FIELD, [&](auto& c) {
                            c.ident(*varint(mr));
                            c.type(StorageRef{.ref = *varint(found2->second)});
                            c.param(std::move(param));
                            c.belong(*target);
                            c.merge_mode(MergeMode::STRICT_TYPE);
                        }));
                    }
                }
                target.reset();
            }
            else if (m.code[i].op == AbstractOp::CONDITIONAL_FIELD) {
                if (auto found = ctx.conditional_properties.find(m.code[i].ident()->value());
                    found != ctx.conditional_properties.end()) {
                    auto left_ref = m.code[i].left_ref().value();  // condition expr
                    auto belong = m.code[i].belong().value();
                    for (auto& c : found->second) {
                        rebound.push_back(make_code(AbstractOp::CONDITIONAL_PROPERTY, [&](auto& code) {
                            code.ident(*varint(c.first));
                            code.left_ref(left_ref);
                            code.right_ref(*varint(c.second));
                            code.belong(belong);
                        }));
                    }
                    continue;  // remove original conditional field
                }
            }
            rebound.push_back(std::move(m.code[i]));
        }
        m.code = std::move(rebound);
        return none;
    }

    std::string property_name_suffix(Module& m, const Storages& s) {
        std::string suffix;
        for (auto& storage : s.storages) {
            if (storage.type == StorageType::ARRAY) {
                suffix += "array_";
            }
            else if (storage.type == StorageType::OPTIONAL) {
                suffix += "optional_";
            }
            else if (storage.type == StorageType::PTR) {
                suffix += "ptr_";
            }
            else if (storage.type == StorageType::STRUCT_REF) {
                if (auto found = m.ident_table_rev.find(storage.ref().value().value());
                    found != m.ident_table_rev.end()) {
                    suffix += found->second->ident;
                }
                else {
                    suffix += std::format("struct_{}", storage.ref().value().value());
                }
            }
            else if (storage.type == StorageType::ENUM) {
                if (auto found = m.ident_table_rev.find(storage.ref().value().value());
                    found != m.ident_table_rev.end()) {
                    suffix += found->second->ident;
                    break;
                }
                else {
                    suffix += std::format("enum_{}", storage.ref().value().value());
                }
            }
            else if (storage.type == StorageType::VARIANT) {
                suffix += "Variant";
            }
            else if (storage.type == StorageType::UINT) {
                suffix += std::format("uint{}", storage.size().value().value());
            }
            else if (storage.type == StorageType::INT) {
                suffix += std::format("int{}", storage.size().value().value());
            }
            else {
                suffix += "unknown";
            }
        }
        return suffix;
    }

    struct RetrieveVarCtx {
        std::set<ObjectID> variables;
        std::vector<ObjectID> ordered_variables;
    };

    expected<ast::Field*> get_field(Module& m, ObjectID id) {
        auto base = m.ident_table_rev.find(id);
        if (base == m.ident_table_rev.end()) {
            return unexpect_error("Invalid ident");
        }
        auto field_ptr = ast::as<ast::Field>(base->second->base.lock());
        if (!field_ptr) {
            return unexpect_error("Invalid field");
        }
        return field_ptr;
    }

    Error retrieve_var(Module& m, auto&& op, const rebgn::Code& code, RetrieveVarCtx& variables) {
        switch (code.op) {
            case rebgn::AbstractOp::NOT_PREV_THEN: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::CAST: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ARRAY_SIZE: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::CALL_CAST: {
                auto param = code.param().value();
                for (auto&& p : param.expr_refs) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::APPEND: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto ref = code.ref().value().value();
                if (ref != 0) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[ref]], variables);
                    if (err) {
                        return err;
                    }
                }
                else {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ACCESS: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                err = retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                // variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD: {
                auto ident = code.ident().value().value();
                auto got = get_field(m, ident);
                if (got) {
                    auto field = *got;
                    if (field->is_state_variable) {
                        if (!variables.variables.contains(ident)) {
                            op(AbstractOp::STATE_VARIABLE_PARAMETER, [&](Code& m) {
                                m.ref(code.ident().value());
                            });
                            variables.variables.insert(ident);
                            variables.ordered_variables.push_back(ident);
                        }
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                // variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                break;  // TODO(on-keyday): check constant origin
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                if (!variables.variables.contains(code.ident().value().value())) {
                    variables.variables.insert(code.ident().value().value());
                    variables.ordered_variables.push_back(code.ident().value().value());
                    op(AbstractOp::DECLARE_VARIABLE, [&](Code& m) {
                        m.ref(code.ident().value());
                    });
                }
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                if (auto left_ref = code.left_ref(); left_ref->value() != 0) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[left_ref.value().value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::CALL: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                auto params = code.param().value();
                for (auto& p : params.expr_refs) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                break;
            }
            case rebgn::AbstractOp::PHI: {
                if (variables.variables.contains(code.ident().value().value())) {
                    return none;
                }
                variables.variables.insert(code.ident().value().value());
                auto phis = code.phi_params().value();
                auto ref = code.ref().value().value();
                for (auto& p : phis.params) {
                    if (p.condition.value() != 0) {
                        auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.condition.value()]], variables);
                        if (err) {
                            return err;
                        }
                    }
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.assign.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                bool first = true;
                for (auto& p : phis.params) {
                    auto tmp_id = m.new_id(nullptr);
                    if (!tmp_id) {
                        return tmp_id.error();
                    }
                    auto idx = m.ident_index_table[p.assign.value()];
                    if (p.condition.value() == 0) {
                        if (!first) {
                            op(AbstractOp::ELSE, [&](Code& m) {});
                        }
                    }
                    else {
                        if (first) {
                            op(AbstractOp::IF, [&](Code& m) {
                                m.ref(*varint(p.condition.value()));
                            });
                            first = false;
                        }
                        else {
                            op(AbstractOp::ELIF, [&](Code& m) {
                                m.ref(*varint(p.condition.value()));
                            });
                        }
                    }
                    if (m.code[idx].op == AbstractOp::ASSIGN) {
                        auto right_ref = m.code[idx].right_ref().value();
                        op(AbstractOp::ASSIGN, [&](Code& m) {
                            m.ident(*tmp_id);
                            m.left_ref(*varint(ref));
                            m.right_ref(right_ref);
                        });
                    }
                }
                if (!first) {
                    op(AbstractOp::END_IF, [&](Code& m) {});
                }
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_CHAR:
            case rebgn::AbstractOp::IMMEDIATE_INT:
            case rebgn::AbstractOp::IMMEDIATE_STRING:
            case rebgn::AbstractOp::IMMEDIATE_TRUE:
            case rebgn::AbstractOp::IMMEDIATE_FALSE:
            case rebgn::AbstractOp::IMMEDIATE_INT64:
            case rebgn::AbstractOp::IMMEDIATE_TYPE:
            case rebgn::AbstractOp::NEW_OBJECT:
                break;
            default:
                return error("Invalid op: {}", to_string(code.op));
        }
        return none;
    }

    bool can_set_ident(const std::shared_ptr<ast::Node>& node) {
        if (auto ident = ast::as<ast::Ident>(node); ident) {
            auto [base, _] = *ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (auto f = ast::as<ast::Field>(base->base.lock())) {
                return true;
            }
        }
        else if (auto member = ast::as<ast::MemberAccess>(node); member) {
            return can_set_ident(member->target);
        }
        return false;
    }

    bool can_set_array_length(ast::Field* field_ptr) {
        if (auto array = ast::as<ast::ArrayType>(field_ptr->field_type);
            array && array->length && !array->length_value && can_set_ident(array->length) &&
            !ast::as<ast::UnionType>(array->length->expr_type)) { /*TODO(on-keyday): support union*/
            return true;
        }
        return false;
    }

    expected<Varint> access_array_length(Module& m, RetrieveVarCtx& rvar, const std::shared_ptr<ast::Node>& len, auto&& op) {
        if (auto ident = ast::as<ast::Ident>(len); ident) {
            auto l = m.lookup_ident(ast::cast_to<ast::Ident>(len));
            if (!l) {
                return l;
            }
            auto got = get_field(m, l->value());
            if (got) {
                auto field_ptr = *got;
                if (field_ptr->is_state_variable) {
                    if (!rvar.variables.contains(l->value())) {
                        op(AbstractOp::STATE_VARIABLE_PARAMETER, [&](Code& m) {
                            m.ref(*l);
                        });
                        rvar.variables.insert(l->value());
                        rvar.ordered_variables.push_back(l->value());
                    }
                }
            }
            return l;
        }
        else if (auto member = ast::as<ast::MemberAccess>(len); member) {
            auto target = access_array_length(m, rvar, member->target, op);
            if (!target) {
                return target;
            }
            auto member_id = m.lookup_ident(member->member);
            if (!member_id) {
                return member_id;
            }
            auto id = m.new_node_id(len);
            if (!id) {
                return id;
            }
            op(AbstractOp::ACCESS, [&](Code& m) {
                m.ident(*id);
                m.left_ref(*target);
                m.right_ref(*member_id);
            });
            return *id;
        }
        return unexpect_error("Invalid node");
    }

    Error add_array_length_setter(Module& m, RetrieveVarCtx& rvar, ast::Field* field_ptr, Varint array_ref, Varint function, auto&& op) {
        auto array = ast::as<ast::ArrayType>(field_ptr->field_type);
        if (!array) {
            return error("Invalid field type");
        }
        auto target_id = access_array_length(m, rvar, array->length, op);
        if (!target_id) {
            return target_id.error();
        }
        auto bit_size = array->length->expr_type->bit_size;
        if (auto u = ast::as<ast::UnionType>(array->length->expr_type); u && u->common_type) {
            bit_size = u->common_type->bit_size;
        }
        if (!bit_size) {
            return error("Invalid bit size");
        }
        auto max_value = (std::uint64_t(1) << *bit_size) - 1;
        Varint value_id;
        if (auto found = m.immediate_table.find(max_value); found != m.immediate_table.end()) {
            value_id = *varint(found->second);
        }
        else {
            auto new_value_id = m.new_id(nullptr);
            if (!new_value_id) {
                return new_value_id.error();
            }
            value_id = *new_value_id;
            if (*bit_size >= 63) {
                op(AbstractOp::IMMEDIATE_INT64, [&](Code& m) {
                    m.ident(value_id);
                    m.int_value64(max_value);
                });
            }
            else {
                op(AbstractOp::IMMEDIATE_INT, [&](Code& m) {
                    m.ident(value_id);
                    m.int_value(*varint(max_value));
                });
            }
        }
        auto length_id = m.new_id(nullptr);
        if (!length_id) {
            return length_id.error();
        }
        op(AbstractOp::ARRAY_SIZE, [&](Code& m) {
            m.ident(*length_id);
            m.ref(array_ref);
        });
        auto cmp_id = m.new_id(nullptr);
        if (!cmp_id) {
            return cmp_id.error();
        }
        op(AbstractOp::BINARY, [&](Code& m) {
            m.ident(*cmp_id);
            m.left_ref(*length_id);
            m.right_ref(value_id);
            m.bop(BinaryOp::less_or_eq);
        });
        op(AbstractOp::ASSERT, [&](Code& m) {
            m.ref(*cmp_id);
            m.belong(function);
        });
        auto dst = Storages{.length = *varint(1), .storages = {Storage{.type = StorageType::UINT}}};
        auto src = dst;
        dst.storages[0].size(*varint(*bit_size));
        src.storages[0].size(*varint(*bit_size + 1));  // to force insert cast
        auto cast = add_assign_cast(m, op, &dst, &src, *length_id, false);
        if (!cast) {
            return cast.error();
        }
        if (!*cast) {
            return error("Failed to add cast");
        }
        auto assign_id = m.new_id(nullptr);
        if (!assign_id) {
            return assign_id.error();
        }
        op(AbstractOp::ASSIGN, [&](Code& m) {
            m.ident(*assign_id);
            m.left_ref(*target_id);
            m.right_ref(**cast);
        });
        return none;
    }

    Error derive_property_getter_setter(Module& m, Code& base, std::vector<Code>& funcs, std::unordered_map<ObjectID, std::pair<Varint, Varint>>& merged_fields) {
        auto op = [&](AbstractOp op, auto&& set) {
            funcs.push_back(make_code(op, set));
        };
        auto belong_of_belong = m.code[m.ident_index_table[base.belong().value().value()]].belong().value();
        auto merged_ident = base.ident().value();
        auto getter_setter = merged_fields[merged_ident.value()];
        auto getter_ident = getter_setter.first;
        auto setter_ident = getter_setter.second;
        auto mode = base.merge_mode().value();
        auto type_found = m.get_storage(base.type().value());
        if (!type_found) {
            return type_found.error();
        }
        auto type = type_found.value();
        auto originalType = type;
        auto originalTypeRef = base.type().value();
        auto param = base.param().value();
        auto do_foreach = [&](auto&& action, auto&& ret_empty) {
            RetrieveVarCtx variables;
            for (auto& c : param.expr_refs) {
                auto& conditional_field = m.code[m.ident_index_table[c.value()]];
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[conditional_field.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
            }
            std::optional<ObjectID> prev_cond;
            for (auto& c : param.expr_refs) {
                // get CONDITIONAL_FIELD or CONDITIONAL_PROPERTY
                auto& code = m.code[m.ident_index_table[c.value()]];
                // get condition expr
                auto expr = &m.code[m.ident_index_table[code.left_ref()->value()]];
                auto expr_ref = expr->ident().value();
                auto orig = expr;
                if (expr->op == AbstractOp::NOT_PREV_THEN) {
                    std::vector<ObjectID> conditions;

                    while (!prev_cond || prev_cond != expr->left_ref()->value()) {
                        auto prev_expr = &m.code[m.ident_index_table[expr->left_ref()->value()]];
                        if (prev_expr->op == AbstractOp::NOT_PREV_THEN) {
                            conditions.push_back(prev_expr->right_ref()->value());
                        }
                        else {
                            conditions.push_back(prev_expr->ident()->value());
                            break;
                        }
                        expr = prev_expr;
                    }
                    ObjectID last = 0;
                    for (auto& c : conditions) {
                        if (last) {
                            auto and_ = m.new_id(nullptr);
                            if (!and_) {
                                return and_.error();
                            }
                            op(AbstractOp::BINARY, [&](Code& m) {
                                m.ident(*and_);
                                m.left_ref(*varint(c));
                                m.right_ref(*varint(last));
                                m.bop(BinaryOp::logical_or);
                            });
                            last = and_->value();
                        }
                        else {
                            last = c;
                        }
                    }
                    if (last != 0) {
                        op(AbstractOp::IF, [&](Code& m) {
                            m.ref(*varint(last));
                        });
                        auto err = ret_empty();
                        if (err) {
                            return err;
                        }
                        op(AbstractOp::END_IF, [&](Code& m) {});
                    }
                    expr_ref = orig->right_ref().value();
                }
                op(AbstractOp::IF, [&](Code& m) {
                    m.ref(expr_ref);
                });
                auto err = action(code, variables);
                if (err) {
                    return err;
                }
                op(AbstractOp::END_IF, [&](Code& m) {});
                prev_cond = orig->ident()->value();
            }
            return none;
        };

        // getter function
        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(getter_ident);
            n.belong(belong_of_belong);
        });

        type.length = *varint(type.storages.size() + 1);
        if (mode == MergeMode::COMMON_TYPE) {
            type.storages.insert(type.storages.begin(), Storage{.type = StorageType::OPTIONAL});
        }
        else {
            type.storages.insert(type.storages.begin(), Storage{.type = StorageType::PTR});
        }
        auto ret_type_ref = m.get_storage_ref(type, nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& n) {
            n.ref(merged_ident);
            n.func_type(PropertyFunctionType::UNION_GETTER);
        });
        auto ret_empty = [&]() {
            auto empty_ident = m.new_id(nullptr);
            if (!empty_ident) {
                return empty_ident.error();
            }
            if (mode == MergeMode::COMMON_TYPE) {
                op(AbstractOp::EMPTY_OPTIONAL, [&](Code& m) {
                    m.ident(*empty_ident);
                });
            }
            else {
                op(AbstractOp::EMPTY_PTR, [&](Code& m) {
                    m.ident(*empty_ident);
                });
            }
            op(AbstractOp::RET, [&](Code& m) {
                m.ref(*empty_ident);
                m.belong(getter_ident);
            });
            return none;
        };
        auto err = do_foreach([&](const Code& code, RetrieveVarCtx& rvar) {
            auto expr_ref = code.right_ref().value();
            auto ident = m.new_id(nullptr);
            if (!ident) {
                return ident.error();
            }
            if (code.op == AbstractOp::CONDITIONAL_PROPERTY) {
                auto found = merged_fields.find(expr_ref.value());
                if (found == merged_fields.end()) {
                    return error("Invalid conditional property");
                }
                auto getter = found->second.first;
                op(AbstractOp::CALL, [&](Code& code) {
                    code.ident(*ident);
                    code.ref(getter);
                });
            }
            else {
                auto& field = m.code[m.ident_index_table[expr_ref.value()]];
                auto belong = field.belong().value();
                op(AbstractOp::CHECK_UNION, [&](Code& m) {
                    m.ref(belong);
                    m.check_at(mode == MergeMode::COMMON_TYPE
                                   ? UnionCheckAt::PROPERTY_GETTER_OPTIONAL
                                   : UnionCheckAt::PROPERTY_GETTER_PTR);
                });
                if (mode == MergeMode::COMMON_TYPE) {
                    op(AbstractOp::OPTIONAL_OF, [&](Code& m) {
                        m.ident(*ident);
                        m.ref(expr_ref);
                        m.type(originalTypeRef);
                    });
                }
                else {
                    op(AbstractOp::ADDRESS_OF, [&](Code& m) {
                        m.ident(*ident);
                        m.ref(expr_ref);
                    });
                }
            }
            op(AbstractOp::RET, [&](Code& m) {
                m.ref(*ident);
                m.belong(getter_ident);
            });
            return none;
        },
                              ret_empty);
        if (err) {
            return err;
        }
        ret_empty();
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});

        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(setter_ident);
            n.belong(belong_of_belong);
        });
        auto prop = m.new_id(nullptr);
        if (!prop) {
            return prop.error();
        }
        op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& n) {
            n.ident(*prop);
            n.left_ref(merged_ident);
            n.right_ref(setter_ident);
            n.type(originalTypeRef);
        });
        ret_type_ref = m.get_storage_ref(Storages{
                                             .length = *varint(1),
                                             .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
                                         },
                                         nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& n) {
            n.ref(merged_ident);
            n.func_type(PropertyFunctionType::UNION_SETTER);
        });
        auto bool_ident = m.new_id(nullptr);
        if (!bool_ident) {
            return bool_ident.error();
        }
        auto ret_false = [&]() {
            op(AbstractOp::RET_PROPERTY_SETTER_FAIL, [&](Code& m) {
                m.belong(setter_ident);
            });
            return none;
        };
        err = do_foreach([&](const Code& code, RetrieveVarCtx& rvar) {
            auto expr_ref = code.right_ref().value();
            if (code.op == AbstractOp::CONDITIONAL_PROPERTY) {
                auto found = merged_fields.find(expr_ref.value());
                if (found == merged_fields.end()) {
                    return error("Invalid conditional property");
                }
                auto setter = found->second.second;
                op(AbstractOp::PROPERTY_ASSIGN, [&](Code& code) {
                    code.left_ref(setter);
                    code.right_ref(*prop);
                });
            }
            else {
                auto& field = m.code[m.ident_index_table[expr_ref.value()]];
                auto belong = field.belong().value();
                op(AbstractOp::SWITCH_UNION, [&](Code& m) {
                    m.ref(belong);
                });
                auto ident = m.new_id(nullptr);
                if (!ident) {
                    return ident.error();
                }
                auto field_ptr = get_field(m, expr_ref.value());
                if (!field_ptr) {
                    return field_ptr.error();
                }
                auto storage = define_storage(m, (*field_ptr)->field_type, true);
                if (!storage) {
                    return storage.error();
                }
                if (can_set_array_length(*field_ptr)) {
                    auto err = add_array_length_setter(m, rvar, *field_ptr, expr_ref, setter_ident, op);
                    if (err) {
                        return err;
                    }
                }
                auto to_type = m.get_storage(*storage);
                if (!to_type) {
                    return to_type.error();
                }
                auto right = *prop;
                auto maybe_cast = add_assign_cast(m, op, &*to_type, &originalType, right);
                if (!maybe_cast) {
                    return maybe_cast.error();
                }
                if (*maybe_cast) {
                    right = **maybe_cast;
                }
                op(AbstractOp::ASSIGN, [&](Code& m) {
                    m.ident(*ident);
                    m.left_ref(expr_ref);
                    m.right_ref(right);
                });
            }
            op(AbstractOp::RET_PROPERTY_SETTER_OK, [&](Code& m) {
                m.belong(setter_ident);
            });
            return none;
        },
                         ret_false);
        if (err) {
            return err;
        }
        ret_false();
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});
        return none;
    }

    Error derive_set_array_function(Module& m, Varint setter_ident, ObjectID ident, ast::Field* field_ptr, auto&& op) {
        auto belong = m.code[m.ident_index_table[ident]].belong().value();
        auto originalType = define_storage(m, field_ptr->field_type);
        if (!originalType) {
            return originalType.error();
        }
        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(setter_ident);
            n.belong(belong);
        });
        auto prop = m.new_id(nullptr);
        if (!prop) {
            return prop.error();
        }
        auto ident_ref = varint(ident);
        if (!ident_ref) {
            return ident_ref.error();
        }
        op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& n) {
            n.ident(*prop);
            n.left_ref(*ident_ref);
            n.right_ref(setter_ident);
            n.type(*originalType);
        });
        auto ret_type_ref = m.get_storage_ref(
            Storages{
                .length = *varint(1),
                .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
            },
            nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& n) {
            n.ref(*varint(ident));
            n.func_type(PropertyFunctionType::VECTOR_SETTER);
        });
        RetrieveVarCtx rvar;
        auto err = add_array_length_setter(m, rvar, field_ptr, *ident_ref, setter_ident, op);
        if (err) {
            return err;
        }
        auto assign_id = m.new_id(nullptr);
        if (!assign_id) {
            return assign_id.error();
        }
        op(AbstractOp::ASSIGN, [&](Code& m) {
            m.ident(*assign_id);
            m.left_ref(*ident_ref);
            m.right_ref(*prop);
        });
        op(AbstractOp::RET_PROPERTY_SETTER_OK, [&](Code& m) {
            m.belong(setter_ident);
        });
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});
        return none;
    }

    // derive union property getter and setter
    // also derive dynamic array setter if simple array length
    Error derive_property_functions(Module& m) {
        std::vector<Code> funcs;
        auto op = [&](AbstractOp o, auto&& set) {
            funcs.push_back(make_code(o, set));
        };
        std::unordered_map<ObjectID, std::pair<Varint, Varint>> merged_fields;
        std::unordered_map<ObjectID, ObjectID> properties_to_merged;
        std::unordered_map<ObjectID, std::pair<ast::Field*, Varint>> set_array_length;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_FIELD) {
                auto belong = c.belong().value().value();
                if (belong == 0) {
                    continue;
                }
                auto belong_idx = m.ident_index_table.find(belong);
                if (belong_idx == m.ident_index_table.end()) {
                    return error("Invalid belong");
                }
                if (m.code[belong_idx->second].op != AbstractOp::DEFINE_FORMAT) {
                    continue;
                }
                auto field_ptr = get_field(m, c.ident().value().value());
                if (!field_ptr) {
                    continue;  // best effort
                }
                if (can_set_array_length(*field_ptr)) {
                    expected<Varint> func_ident;
                    if (!(*field_ptr)->ident) {
                        func_ident = m.new_id(&(*field_ptr)->loc);
                        if (!func_ident) {
                            return func_ident.error();
                        }
                    }
                    else {
                        auto temporary_setter_ident = std::make_shared<ast::Ident>((*field_ptr)->loc, (*field_ptr)->ident->ident);
                        temporary_setter_ident->base = (*field_ptr)->ident->base;
                        func_ident = m.lookup_ident(temporary_setter_ident);
                        if (!func_ident) {
                            return func_ident.error();
                        }
                    }
                    set_array_length.emplace(c.ident()->value(), std::make_pair(*field_ptr, *func_ident));
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto belong = c.belong().value();
                auto mode = c.merge_mode().value();
                auto merged_ident = c.ident().value();
                auto base = m.ident_table_rev[belong.value()];
                std::string ident = base->ident;
                if (mode == MergeMode::COMMON_TYPE ||
                    mode == MergeMode::STRICT_COMMON_TYPE) {
                    properties_to_merged[belong.value()] = merged_ident.value();
                }
                else {
                    auto found = m.get_storage(c.type().value());
                    if (!found) {
                        return found.error();
                    }
                    ident += "_" + property_name_suffix(m, found.value());
                }
                auto temporary_getter_ident = std::make_shared<ast::Ident>(base->loc, ident);
                temporary_getter_ident->base = base->base;
                auto getter_ident = m.lookup_ident(temporary_getter_ident);
                if (!getter_ident) {
                    return getter_ident.error();
                }
                merged_fields[merged_ident.value()].first = *getter_ident;
                auto temporary_setter_ident = std::make_shared<ast::Ident>(base->loc, ident);
                temporary_setter_ident->base = base->base;
                // setter function
                auto setter_ident = m.lookup_ident(temporary_setter_ident);
                if (!setter_ident) {
                    return setter_ident.error();
                }
                merged_fields[merged_ident.value()].second = *setter_ident;
            }
        }
        for (auto& c : m.code) {
            if (c.op == AbstractOp::ASSIGN) {
                auto left_ref = c.left_ref().value();
                if (auto found = properties_to_merged.find(left_ref.value()); found != properties_to_merged.end()) {
                    if (auto found2 = merged_fields.find(found->second); found2 != merged_fields.end()) {
                        auto right_ref = c.right_ref().value();
                        c.op = AbstractOp::PROPERTY_ASSIGN;
                        c.left_ref(found2->second.second);
                        c.right_ref(right_ref);
                    }
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto err = derive_property_getter_setter(m, c, funcs, merged_fields);
                if (err) {
                    return err;
                }
            }
            else if (c.op == AbstractOp::DEFINE_FIELD) {
                auto ident = c.ident().value();
                if (auto found = set_array_length.find(ident.value());
                    found != set_array_length.end()) {
                    auto err = derive_set_array_function(m, found->second.second, ident.value(), found->second.first, op);
                    if (err) {
                        return err;
                    }
                }
            }
        }
        std::vector<Code> rebound;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto ident = c.ident().value();
                auto found = merged_fields.find(ident.value());
                if (found != merged_fields.end()) {
                    rebound.push_back(std::move(c));
                    rebound.push_back(make_code(AbstractOp::DEFINE_PROPERTY_GETTER, [&](auto& c) {
                        c.left_ref(ident);
                        c.right_ref(*varint(found->second.first.value()));
                    }));
                    rebound.push_back(make_code(AbstractOp::DEFINE_PROPERTY_SETTER, [&](auto& c) {
                        c.left_ref(ident);
                        c.right_ref(*varint(found->second.second.value()));
                    }));
                }
                continue;
            }
            else if (c.op == AbstractOp::DEFINE_FIELD) {
                auto ident = c.ident().value();
                if (auto found = set_array_length.find(ident.value());
                    found != set_array_length.end()) {
                    rebound.push_back(std::move(c));
                    rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                        c.ref(found->second.second);
                    }));
                    continue;
                }
            }
            rebound.push_back(std::move(c));
        }
        m.code = std::move(rebound);
        m.code.insert(m.code.end(), funcs.begin(), funcs.end());
        return none;
    }

    struct FunctionStack {
        size_t current_function_index = 0;
        std::optional<size_t> decoder_parameter_index;
        std::optional<size_t> encoder_parameter_index;
        size_t inner_subrange = 0;

        bool is_decoder() const {
            return decoder_parameter_index.has_value() && !inner_subrange;
        }

        bool is_encoder() const {
            return encoder_parameter_index.has_value() && !inner_subrange;
        }
    };

    void analyze_encoder_decoder_traits(Module& m) {
        std::unordered_map<ObjectID, ObjectID> function_to_coder;
        std::vector<FunctionStack> stack;
        auto common_process = [&](size_t i) {
            if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                stack.push_back(FunctionStack{.current_function_index = i, .inner_subrange = 0});
            }
            if (m.code[i].op == AbstractOp::DECODER_PARAMETER) {
                stack.back().decoder_parameter_index = i;
            }
            if (m.code[i].op == AbstractOp::ENCODER_PARAMETER) {
                stack.back().encoder_parameter_index = i;
            }
            if (m.code[i].op == AbstractOp::END_FUNCTION) {
                stack.pop_back();
            }
            if (m.code[i].op == AbstractOp::BEGIN_DECODE_SUB_RANGE ||
                m.code[i].op == AbstractOp::BEGIN_ENCODE_SUB_RANGE) {
                stack.back().inner_subrange++;
            }
            if (m.code[i].op == AbstractOp::END_DECODE_SUB_RANGE ||
                m.code[i].op == AbstractOp::END_ENCODE_SUB_RANGE) {
                stack.back().inner_subrange--;
            }
        };
        for (size_t i = 0; i < m.code.size(); i++) {
            common_process(i);
            if (stack.empty()) {
                continue;
            }
            auto set_decode_flag = [&](auto&& set) {
                auto idx = stack.back().decoder_parameter_index.value();
                auto flags = m.code[idx].decode_flags().value();
                set(flags);
                m.code[idx].decode_flags(flags);
            };
            auto set_encode_flag = [&](auto&& set) {
                auto idx = stack.back().encoder_parameter_index.value();
                auto flags = m.code[idx].encode_flags().value();
                set(flags);
                m.code[idx].encode_flags(flags);
            };
            if (stack.back().is_decoder()) {
                function_to_coder[m.code[stack.back().current_function_index].ident().value().value()] = *stack.back().decoder_parameter_index;
                if (m.code[i].op == AbstractOp::CAN_READ) {
                    set_decode_flag([](auto& flags) { flags.has_eof(true); });
                }
                if (m.code[i].op == AbstractOp::REMAIN_BYTES) {
                    set_decode_flag([](auto& flags) { flags.has_remain_bytes(true); });
                }
                if (m.code[i].op == AbstractOp::PEEK_INT_VECTOR) {
                    set_decode_flag([](auto& flags) { flags.has_peek(true); });
                }
                if (m.code[i].op == AbstractOp::BACKWARD_INPUT || m.code[i].op == AbstractOp::INPUT_BYTE_OFFSET) {
                    set_decode_flag([](auto& flags) { flags.has_seek(true); });
                }
            }
            else if (stack.back().decoder_parameter_index.has_value()) {
                set_decode_flag([](auto& flags) { flags.has_sub_range(true); });  // sub range will not be propagated by call
            }
            if (stack.back().is_encoder()) {
                function_to_coder[m.code[stack.back().current_function_index].ident().value().value()] = *stack.back().encoder_parameter_index;
                if (m.code[i].op == AbstractOp::BACKWARD_OUTPUT || m.code[i].op == AbstractOp::OUTPUT_BYTE_OFFSET) {
                    set_encode_flag([](auto& flags) { flags.has_seek(true); });
                }
            }
            else if (stack.back().encoder_parameter_index.has_value()) {
                set_encode_flag([](auto& flags) { flags.has_sub_range(true); });
            }
        }
        std::set<ObjectID> reached;
        bool has_diff = false;
        auto recursive_process = [&](auto&& f, size_t start) -> void {
            if (reached.contains(start)) {
                return;
            }
            reached.insert(start);
            for (size_t i = start; i < m.code.size(); i++) {
                common_process(i);
                if (m.code[i].op == AbstractOp::END_FUNCTION) {
                    return;
                }
                if (stack.back().is_decoder()) {
                    if (m.code[i].op == AbstractOp::CALL_DECODE) {
                        auto found = function_to_coder.find(m.code[i].left_ref().value().value());
                        if (found != function_to_coder.end()) {
                            f(f, m.ident_index_table[found->first]);
                            // copy traits
                            auto idx = stack.back().decoder_parameter_index.value();
                            auto dst = m.code[idx].decode_flags().value();
                            auto src = m.code[found->second].decode_flags().value();
                            auto prev_diff = has_diff;
                            has_diff = has_diff ||
                                       !dst.has_eof() && src.has_eof() ||
                                       !dst.has_remain_bytes() && src.has_remain_bytes() ||
                                       !dst.has_peek() && src.has_peek() ||
                                       !dst.has_seek() && src.has_seek();
                            dst.has_eof(dst.has_eof() || src.has_eof());
                            dst.has_remain_bytes(dst.has_remain_bytes() || src.has_remain_bytes());
                            dst.has_peek(dst.has_peek() || src.has_peek());
                            dst.has_seek(dst.has_seek() || src.has_seek());
                            m.code[idx].decode_flags(dst);
                        }
                    }
                }
                if (stack.back().is_encoder()) {
                    if (m.code[i].op == AbstractOp::CALL_ENCODE) {
                        auto found = function_to_coder.find(m.code[i].left_ref().value().value());
                        if (found != function_to_coder.end()) {
                            f(f, m.ident_index_table[found->first]);
                            // copy traits
                            auto idx = stack.back().encoder_parameter_index.value();
                            auto dst = m.code[idx].encode_flags().value();
                            auto src = m.code[found->second].encode_flags().value();
                            auto prev_diff = has_diff;
                            has_diff = has_diff || !dst.has_seek() && src.has_seek();
                            dst.has_seek(dst.has_seek() || src.has_seek());
                            m.code[idx].encode_flags(dst);
                        }
                    }
                }
            }
        };
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                recursive_process(recursive_process, i);
            }
        }
        while (has_diff) {  // TODO(on-keyday): optimize
            has_diff = false;
            reached.clear();
            for (size_t i = 0; i < m.code.size(); i++) {
                if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                    recursive_process(recursive_process, i);
                }
            }
        }
    }

    Error optimize_type_usage(Module& m) {
        std::unordered_map<ObjectID, std::uint64_t> type_usage;
        std::set<ObjectID> reached;
        for (auto& c : m.code) {
            if (auto s = c.type()) {
                if (s->ref.value() == 0) {  // null, skip
                    continue;
                }
                reached.insert(s.value().ref.value());
                type_usage[s.value().ref.value()]++;
            }
            if (auto f = c.from_type()) {
                if (f->ref.value() == 0) {  // null, skip
                    continue;
                }
                reached.insert(f.value().ref.value());
                type_usage[f.value().ref.value()]++;
            }
        }
        std::vector<std::tuple<ObjectID /*original*/, std::uint64_t /*usage*/, ObjectID /*map to*/>> sorted;
        for (auto& [k, v] : type_usage) {
            sorted.emplace_back(k, v, 0);
        }
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return std::get<1>(a) > std::get<1>(b); });
        if (reached.size() != sorted.size()) {
            return error("Invalid type usage");
        }
        size_t i = 0;
        for (auto& r : reached) {
            std::get<2>(sorted[i]) = r;
            i++;
        }
        std::unordered_map<ObjectID, ObjectID> mapping;
        for (auto& [k, _, v] : sorted) {
            mapping[k] = v;
        }
        std::unordered_map<ObjectID, std::string> new_storage_key_table_rev;
        std::unordered_map<std::string, ObjectID> new_storage_key_table;
        for (auto& mp : mapping) {
            auto old = m.storage_key_table_rev.find(mp.first);
            if (old == m.storage_key_table_rev.end()) {
                return error("Invalid storage key");
            }
            new_storage_key_table_rev[mp.second] = old->second;
            new_storage_key_table[old->second] = mp.second;
        }
        for (auto& c : m.code) {
            if (auto s = c.type()) {
                if (s->ref.value() == 0) {
                    continue;
                }
                c.type(StorageRef{.ref = *varint(mapping[s.value().ref.value()])});
            }
            if (auto f = c.from_type()) {
                if (f->ref.value() == 0) {
                    continue;
                }
                c.from_type(StorageRef{.ref = *varint(mapping[f.value().ref.value()])});
            }
        }
        m.storage_key_table = std::move(new_storage_key_table);
        m.storage_key_table_rev = std::move(new_storage_key_table_rev);
        return none;
    }

    Error add_endian_specific(Module& m, EndianExpr endian, auto&& op, auto&& on_little_endian, auto&& on_big_endian) {
        auto is_native_or_dynamic = endian.endian() == Endian::native || endian.endian() == Endian::dynamic;
        if (is_native_or_dynamic) {
            auto is_little = m.new_id(nullptr);
            if (!is_little) {
                return is_little.error();
            }
            op(AbstractOp::IS_LITTLE_ENDIAN, [&](Code& m) {
                m.ident(*is_little);
                m.ref(endian.dynamic_ref);  // dynamic endian or null for native
            });
            op(AbstractOp::IF, [&](Code& m) {
                m.ref(*is_little);
            });
        }
        if (endian.endian() == Endian::little || is_native_or_dynamic) {
            auto err = on_little_endian();
            if (err) {
                return err;
            }
        }
        if (is_native_or_dynamic) {
            op(AbstractOp::ELSE, [&](Code& m) {});
        }
        if (endian.endian() == Endian::big || is_native_or_dynamic) {
            auto err = on_big_endian();
            if (err) {
                return err;
            }
        }
        if (is_native_or_dynamic) {
            op(AbstractOp::END_IF, [&](Code& m) {});
        }
        return none;
    }

    Error expand_bit_operation(Module& m) {
        std::vector<Code> new_code;
        auto op = [&](AbstractOp o, auto&& set) {
            new_code.push_back(make_code(o, set));
        };
        auto immediate = [&](std::uint64_t value) {
            if (auto found = m.immediate_table.find(value); found != m.immediate_table.end()) {
                return varint(found->second);
            }
            auto new_id = m.new_id(nullptr);
            if (!new_id) {
                return new_id;
            }
            if (auto vin = varint(value); vin) {
                op(AbstractOp::IMMEDIATE_INT, [&](Code& m) {
                    m.ident(*new_id);
                    m.int_value(*vin);
                });
            }
            else {
                op(AbstractOp::IMMEDIATE_INT64, [&](Code& m) {
                    m.ident(*new_id);
                    m.int_value64(value);
                });
            }
            return new_id;
        };
        auto new_var = [&](StorageRef ref, Varint new_id) {
            auto new_var = m.new_id(nullptr);
            if (!new_var) {
                return new_var;
            }
            op(AbstractOp::DEFINE_VARIABLE, [&](Code& m) {
                m.ident(*new_var);
                m.ref(new_id);
                m.type(ref);
            });
            return new_var;
        };
        auto new_default_var = [&](StorageRef ref) {
            auto new_id = m.new_id(nullptr);
            if (!new_id) {
                return new_id;
            }
            op(AbstractOp::NEW_OBJECT, [&](Code& m) {
                m.ident(*new_id);
                m.type(ref);
            });
            return new_var(ref, *new_id);
        };
        auto new_array = [&](std::uint64_t len) {
            Storages storage{.length = *varint(2), .storages = {Storage{.type = StorageType::ARRAY}, Storage{.type = StorageType::UINT}}};
            storage.storages[0].size(*varint(len));
            storage.storages[1].size(*varint(8));
            auto ref = m.get_storage_ref(storage, nullptr);
            if (!ref) {
                return ref.transform([](auto&&) { return Varint(); });
            }
            return new_default_var(*ref);
        };
        bool on_bit_operation = false;
        EndianExpr endian{};
        PackedOpType packed_op = PackedOpType::FIXED;
        size_t bit_size = 0;
        Varint counter, target, tmp_array, read_bytes, belong;
        StorageRef target_type, nbit_typ;
        std::optional<StorageRef> u8_typ;

        auto get_nbit_typ = [&](size_t nbit_typ, bool sign) -> expected<StorageRef> {
            if (nbit_typ == 8 && !sign && u8_typ) {
                return *u8_typ;
            }
            Storage storage{.type = sign ? StorageType::INT : StorageType::UINT};
            storage.size(*varint(nbit_typ));
            auto res = m.get_storage_ref(Storages{.length = *varint(1), .storages = {storage}}, nullptr);
            if (!res) {
                return res;
            }
            if (nbit_typ == 8 && !sign) {
                u8_typ = *res;
            }
            return *res;
        };
        // target = target | (target_type(value) << shift_index)
        auto assign_to_target = [&](Varint ref, Varint shift_index, auto&& src_type, auto&& src_type_storage) {
            // cast to target type
            auto dst_storage = m.get_storage(target_type);
            if (!dst_storage) {
                return dst_storage.error();
            }
            auto casted = add_assign_cast(m, op, &*dst_storage, &*src_type_storage, ref);
            if (!casted) {
                return casted.error();
            }
            auto shift = m.new_id(nullptr);
            if (!shift) {
                return shift.error();
            }
            op(AbstractOp::BINARY, [&](Code& m) {
                m.ident(*shift);
                m.left_ref(*casted ? **casted : ref);
                m.right_ref(shift_index);
                m.bop(BinaryOp::left_logical_shift);
            });
            auto bit_or = m.new_id(nullptr);
            if (!bit_or) {
                return bit_or.error();
            }
            op(AbstractOp::BINARY, [&](Code& m) {
                m.ident(*bit_or);
                m.left_ref(target);
                m.right_ref(*shift);
                m.bop(BinaryOp::bit_or);
            });
            auto assign = m.new_id(nullptr);
            if (!assign) {
                return assign.error();
            }
            op(AbstractOp::ASSIGN, [&](Code& m) {
                m.ident(*assign);
                m.left_ref(target);
                m.right_ref(*bit_or);
            });
            return none;
        };

        auto add_counter = [&](size_t i) {
            auto add = m.new_id(nullptr);
            if (!add) {
                return add.error();
            }
            auto imm = immediate(i);
            if (!imm) {
                return imm.error();
            }
            op(AbstractOp::BINARY, [&](Code& m) {
                m.ident(*add);
                m.left_ref(counter);
                m.right_ref(*imm);
                m.bop(BinaryOp::add);
            });
            auto assign = m.new_id(nullptr);
            if (!assign) {
                return assign.error();
            }
            op(AbstractOp::ASSIGN, [&](Code& m) {
                m.ident(*assign);
                m.left_ref(counter);
                m.right_ref(*add);
            });
            return none;
        };

        for (auto& code : m.code) {
            if (code.op == rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION ||
                code.op == rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION) {
                on_bit_operation = true;
                endian = code.endian().value();
                packed_op = code.packed_op_type().value();
                belong = code.belong().value();  // refer to bit field
                auto& maybe_type = m.code[m.ident_index_table[code.belong().value().value()]];
                if (maybe_type.op != rebgn::AbstractOp::DEFINE_BIT_FIELD) {
                    return error("Invalid packed operation");
                }
                auto type = m.get_storage(maybe_type.type().value());
                if (!type) {
                    return type.error();
                }

                auto new_target_type = maybe_type.type().value();
                auto new_bit_size = type.value().storages[0].size()->value();

                code.bit_size(*varint(new_bit_size));
                new_code.push_back(code);  // copy
                auto new_target_var = new_default_var(new_target_type);
                if (!new_target_var) {
                    return new_target_var.error();
                }

                auto n_bit = futils::binary::log2i(new_bit_size);
                Storage storage{.type = StorageType::UINT};
                storage.size(*varint(n_bit));
                auto new_nbit_typ = m.get_storage_ref(Storages{.length = *varint(1), .storages = {storage}}, nullptr);
                if (!new_nbit_typ) {
                    return new_nbit_typ.error();
                }
                auto zero = immediate(0);
                if (!zero) {
                    return zero.error();
                }
                auto new_counter_var = new_var(*new_nbit_typ, *zero);
                if (!new_counter_var) {
                    return new_counter_var.error();
                }
                Varint new_tmp_array, new_read_bytes;
                if (packed_op == PackedOpType::VARIABLE) {
                    auto new_array_var = new_array(new_bit_size / 8);
                    if (!new_array_var) {
                        return new_array_var.error();
                    }
                    new_tmp_array = *new_array_var;
                }
                if (code.op == rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION) {
                    if (packed_op == PackedOpType::FIXED) {
                        op(AbstractOp::DECODE_INT, [&](Code& m) {
                            m.ref(*new_target_var);
                            m.endian(endian);
                            m.bit_size(*varint(new_bit_size));
                            m.belong(belong);  // refer to bit field
                        });
                    }
                    else {
                        auto read_bytes_var = new_var(*new_nbit_typ, *zero);
                        if (!read_bytes_var) {
                            return read_bytes_var.error();
                        }
                        new_read_bytes = *read_bytes_var;
                    }
                }

                nbit_typ = *new_nbit_typ;
                counter = *new_counter_var;
                target_type = new_target_type;
                target = *new_target_var;
                tmp_array = new_tmp_array;
                read_bytes = new_read_bytes;
                bit_size = new_bit_size;
            }
            else if (code.op == rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION ||
                     code.op == rebgn::AbstractOp::END_DECODE_PACKED_OPERATION) {
                if (code.op == rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION) {
                    if (packed_op == PackedOpType::FIXED) {
                        op(AbstractOp::ENCODE_INT, [&](Code& m) {
                            m.ref(target);
                            m.endian(endian);
                            m.bit_size(*varint(bit_size));
                            m.belong(belong);  // refer to bit field
                        });
                    }
                    else {
                        // counter / 8
                        auto div = m.new_id(nullptr);
                        if (!div) {
                            return div.error();
                        }
                        auto eight = immediate(8);
                        if (!eight) {
                            return eight.error();
                        }
                        op(AbstractOp::BINARY, [&](Code& m) {
                            m.ident(*div);
                            m.left_ref(counter);
                            m.right_ref(*eight);
                            m.bop(BinaryOp::div);
                        });
                        auto zero = immediate(0);
                        if (!zero) {
                            return zero.error();
                        }
                        // TODO(on-keyday): for now, only support power of 8 bit
                        auto result_byte_count = new_var(nbit_typ, *div);
                        auto i_ = new_var(nbit_typ, *zero);
                        // i_ < result_byte_count
                        auto less = m.new_id(nullptr);
                        if (!less) {
                            return less.error();
                        }
                        op(AbstractOp::BINARY, [&](Code& m) {
                            m.ident(*less);
                            m.left_ref(*i_);
                            m.right_ref(*result_byte_count);
                            m.bop(BinaryOp::less);
                        });
                        // while (i_ < result_byte_count)
                        op(AbstractOp::LOOP_CONDITION, [&](Code& m) {
                            m.ref(*less);
                        });
                        {
                            // on each loop
                            // on little endian:
                            //    tmp_array[i_] = u8((target >> (i_ * 8)) & 0xff)
                            // on big endian:
                            //    tmp_array[i_] = u8((target >> ((bit_size / 8 - 1 - i_) * 8)) & 0xff)
                            // on native endian, platform dependent
                            // on dynamic endian, dynamic variable dependent
                            auto assign_to_array = [&](Varint shift_index) {
                                auto mul = m.new_id(nullptr);
                                if (!mul) {
                                    return mul.error();
                                }
                                op(AbstractOp::BINARY, [&](Code& m) {
                                    m.ident(*mul);
                                    m.left_ref(shift_index);
                                    m.right_ref(*eight);
                                    m.bop(BinaryOp::mul);
                                });
                                auto shift = m.new_id(nullptr);
                                if (!shift) {
                                    return shift.error();
                                }
                                op(AbstractOp::BINARY, [&](Code& m) {
                                    m.ident(*shift);
                                    m.left_ref(target);
                                    m.right_ref(*mul);
                                    m.bop(BinaryOp::right_logical_shift);
                                });
                                auto and_ = m.new_id(nullptr);
                                if (!and_) {
                                    return and_.error();
                                }
                                auto immFF = immediate(0xff);
                                if (!immFF) {
                                    return immFF.error();
                                }
                                op(AbstractOp::BINARY, [&](Code& m) {
                                    m.ident(*and_);
                                    m.left_ref(*shift);
                                    m.right_ref(*immFF);
                                    m.bop(BinaryOp::bit_and);
                                });
                                auto cast = m.new_id(nullptr);
                                if (!cast) {
                                    return cast.error();
                                }
                                auto u8_typ = get_nbit_typ(8, false);
                                if (!u8_typ) {
                                    return u8_typ.error();
                                }
                                op(AbstractOp::CAST, [&](Code& m) {
                                    m.ident(*cast);
                                    m.ref(*and_);
                                    m.type(*u8_typ);
                                    m.from_type(target_type);
                                    m.cast_type(CastType::SMALL_INT_TO_LARGE_INT);
                                });
                                auto array_index = m.new_id(nullptr);
                                if (!array_index) {
                                    return array_index.error();
                                }
                                op(AbstractOp::INDEX, [&](Code& m) {
                                    m.ident(*array_index);
                                    m.left_ref(tmp_array);
                                    m.right_ref(*i_);
                                });
                                auto assign = m.new_id(nullptr);
                                if (!assign) {
                                    return assign.error();
                                }
                                op(AbstractOp::ASSIGN, [&](Code& m) {
                                    m.ident(*assign);
                                    m.left_ref(*array_index);
                                    m.right_ref(*cast);
                                });
                                return none;
                            };
                            auto err = add_endian_specific(
                                m, endian, op,
                                [&]() {
                                    return assign_to_array(*i_);
                                },
                                [&]() {
                                    auto bit_size_div_8_minus_1 = immediate(bit_size / 8 - 1);
                                    if (!bit_size_div_8_minus_1) {
                                        return bit_size_div_8_minus_1.error();
                                    }
                                    auto sub = m.new_id(nullptr);
                                    if (!sub) {
                                        return sub.error();
                                    }
                                    op(AbstractOp::BINARY, [&](Code& m) {
                                        m.ident(*sub);
                                        m.left_ref(*bit_size_div_8_minus_1);
                                        m.right_ref(*i_);
                                        m.bop(BinaryOp::sub);
                                    });
                                    return assign_to_array(*sub);
                                });
                            if (err) {
                                return err;
                            }
                            op(AbstractOp::INC, [&](Code& m) {
                                m.ref(*i_);
                            });
                        }
                        op(AbstractOp::END_LOOP, [&](Code& m) {});
                        op(AbstractOp::ENCODE_INT_VECTOR_FIXED, [&](Code& m) {
                            m.left_ref(tmp_array);
                            m.right_ref(*result_byte_count);
                            m.endian(endian);
                            m.bit_size(*varint(8));
                            m.belong(belong);  // refer to bit field
                            m.array_length(*result_byte_count);
                        });
                    }
                }
                new_code.push_back(code);  // copy
                on_bit_operation = false;
            }
            else if (on_bit_operation && code.op == rebgn::AbstractOp::ENCODE_INT) {
                auto enc_bit_size = code.bit_size()->value();
                auto enc_endian = code.endian().value();
                auto src_type = get_nbit_typ(enc_bit_size, enc_endian.sign());
                if (!src_type) {
                    return src_type.error();
                }
                auto src_type_storage = m.get_storage(*src_type);
                if (!src_type_storage) {
                    return src_type_storage.error();
                }
                // on little endian:
                //   // fill from lsb
                //   target = target | (target_type(value) << counter)
                //   counter += bit_size
                // on big endian:
                //   // fill from msb
                //   counter += bit_size
                //   target = target | (target_type(value) << (nbit - counter))
                // on native endian, platform dependent
                // on dynamic endian, dynamic variable dependent
                auto err = add_endian_specific(
                    m, endian, op,
                    [&]() {
                        auto err = assign_to_target(code.ref().value(), counter, src_type, src_type_storage);
                        if (err) {
                            return err;
                        }
                        return add_counter(enc_bit_size);
                    },
                    [&]() {
                        auto err = add_counter(enc_bit_size);
                        if (err) {
                            return err;
                        }
                        auto bit_size_ = immediate(bit_size);
                        if (!bit_size_) {
                            return bit_size_.error();
                        }
                        auto sub = m.new_id(nullptr);
                        if (!sub) {
                            return sub.error();
                        }
                        op(AbstractOp::BINARY, [&](Code& m) {
                            m.ident(*sub);
                            m.left_ref(*bit_size_);
                            m.right_ref(counter);
                            m.bop(BinaryOp::sub);
                        });
                        return assign_to_target(code.ref().value(), *sub, src_type, src_type_storage);
                    });
                if (err) {
                    return err;
                }
            }
            else if (on_bit_operation && code.op == rebgn::AbstractOp::DECODE_INT) {
                auto dec_bit_size = code.bit_size()->value();
                auto dec_endian = code.endian().value();
                auto src_type = get_nbit_typ(dec_bit_size, dec_endian.sign());
                if (!src_type) {
                    return src_type.error();
                }
                auto src_type_storage = m.get_storage(*src_type);
                if (!src_type_storage) {
                    return src_type_storage.error();
                }
                if (packed_op == PackedOpType::VARIABLE) {
                    // consumed_bytes = (counter + dec_bit_size + 7) / 8
                    // if (read_bytes < consumed_bytes)
                    //   for (i_ = read_bytes; i_ < consumed_bytes; i_++)
                    //     tmp_array[i_] = decode_u8()
                    // on little endian:
                    //     target = target | (target_type(tmp_array[i_]) << (i_ * 8))
                    // on big endian:
                    //     target = target | (target_type(tmp_array[i_]) << ((bit_size / 8 - i_ - 1) * 8))
                    // on native endian, platform dependents
                    // on dynamic endian, dynamic variable dependent
                    //   read_bytes = consumed_bytes
                    auto add = m.new_id(nullptr);
                    if (!add) {
                        return add.error();
                    }
                    auto imm = immediate(7);
                    if (!imm) {
                        return imm.error();
                    }
                    auto imm_size = immediate(dec_bit_size);
                    if (!imm_size) {
                        return imm_size.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*add);
                        m.left_ref(counter);
                        m.right_ref(*imm_size);
                        m.bop(BinaryOp::add);
                    });
                    auto add2 = m.new_id(nullptr);
                    if (!add2) {
                        return add2.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*add2);
                        m.left_ref(*add);
                        m.right_ref(*imm);
                        m.bop(BinaryOp::add);
                    });
                    auto div = m.new_id(nullptr);
                    if (!div) {
                        return div.error();
                    }
                    auto eight = immediate(8);
                    if (!eight) {
                        return eight.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*div);
                        m.left_ref(*add2);
                        m.right_ref(*eight);
                        m.bop(BinaryOp::div);
                    });
                    auto consumed_bytes = new_var(nbit_typ, *div);
                    if (!consumed_bytes) {
                        return consumed_bytes.error();
                    }
                    auto less = m.new_id(nullptr);
                    if (!less) {
                        return less.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*less);
                        m.left_ref(read_bytes);
                        m.right_ref(*consumed_bytes);
                        m.bop(BinaryOp::less);
                    });
                    op(AbstractOp::IF, [&](Code& m) {
                        m.ref(*less);
                    });
                    {
                        auto i_ = new_var(nbit_typ, read_bytes);
                        if (!i_) {
                            return i_.error();
                        }
                        auto less2 = m.new_id(nullptr);
                        if (!less2) {
                            return less2.error();
                        }
                        op(AbstractOp::BINARY, [&](Code& m) {
                            m.ident(*less2);
                            m.left_ref(*i_);
                            m.right_ref(*consumed_bytes);
                            m.bop(BinaryOp::less);
                        });
                        op(AbstractOp::LOOP_CONDITION, [&](Code& m) {
                            m.ref(*less2);
                        });
                        {
                            auto array_index = m.new_id(nullptr);
                            if (!array_index) {
                                return array_index.error();
                            }
                            op(AbstractOp::INDEX, [&](Code& m) {
                                m.ident(*array_index);
                                m.left_ref(tmp_array);
                                m.right_ref(*i_);
                            });
                            op(AbstractOp::DECODE_INT, [&](Code& m) {
                                m.ref(*array_index);
                                m.endian(endian);
                                m.bit_size(*varint(8));
                                m.belong(code.ref().value());
                            });

                            auto u8_typ = get_nbit_typ(8, false);
                            if (!u8_typ) {
                                return u8_typ.error();
                            }
                            auto u8_typ_storage = m.get_storage(*u8_typ);
                            if (!u8_typ_storage) {
                                return u8_typ_storage.error();
                            }
                            auto err = add_endian_specific(
                                m, endian, op,
                                [&]() {
                                    auto mul = m.new_id(nullptr);
                                    if (!mul) {
                                        return mul.error();
                                    }
                                    auto eight = immediate(8);
                                    if (!eight) {
                                        return eight.error();
                                    }
                                    op(AbstractOp::BINARY, [&](Code& m) {
                                        m.ident(*mul);
                                        m.left_ref(*i_);
                                        m.right_ref(*eight);
                                        m.bop(BinaryOp::mul);
                                    });
                                    return assign_to_target(*array_index, *mul, u8_typ, u8_typ_storage);
                                },
                                [&]() {
                                    auto bit_size_div_8_minus_1 = immediate(bit_size / 8 - 1);
                                    if (!bit_size_div_8_minus_1) {
                                        return bit_size_div_8_minus_1.error();
                                    }
                                    auto sub = m.new_id(nullptr);
                                    if (!sub) {
                                        return sub.error();
                                    }
                                    op(AbstractOp::BINARY, [&](Code& m) {
                                        m.ident(*sub);
                                        m.left_ref(*bit_size_div_8_minus_1);
                                        m.right_ref(*i_);
                                        m.bop(BinaryOp::sub);
                                    });
                                    auto mul = m.new_id(nullptr);
                                    if (!mul) {
                                        return mul.error();
                                    }
                                    auto eight = immediate(8);
                                    if (!eight) {
                                        return eight.error();
                                    }
                                    op(AbstractOp::BINARY, [&](Code& m) {
                                        m.ident(*mul);
                                        m.left_ref(*sub);
                                        m.right_ref(*eight);
                                        m.bop(BinaryOp::mul);
                                    });
                                    return assign_to_target(*array_index, *mul, u8_typ, u8_typ_storage);
                                });
                            op(AbstractOp::INC, [&](Code& m) {
                                m.ref(*i_);
                            });
                        }
                        op(AbstractOp::END_LOOP, [&](Code& m) {});
                        auto assign = m.new_id(nullptr);
                        if (!assign) {
                            return assign.error();
                        }
                        op(AbstractOp::ASSIGN, [&](Code& m) {
                            m.ident(*assign);
                            m.left_ref(read_bytes);
                            m.right_ref(*consumed_bytes);
                        });
                    }
                    op(AbstractOp::END_IF, [&](Code& m) {});
                }

                // mask = ((1 << dec_bit_size) - 1)
                // on little endian:
                //   // get from lsb
                //  ref = src_type((target >> counter) & mask)
                //  counter += dec_bit_size
                // on big endian:
                //   // get from msb
                //  counter += dec_bit_size
                //  ref = src_type((target >> (nbit - counter)) & mask)
                // on native endian, platform dependent
                // on dynamic endian, dynamic variable dependent
                auto assign_to_ref = [&](Varint shift_index) {
                    auto shift = m.new_id(nullptr);
                    if (!shift) {
                        return shift.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*shift);
                        m.left_ref(target);
                        m.right_ref(shift_index);
                        m.bop(BinaryOp::right_logical_shift);
                    });
                    auto and_ = m.new_id(nullptr);
                    if (!and_) {
                        return and_.error();
                    }
                    auto mask = immediate((std::uint64_t(1) << dec_bit_size) - 1);
                    if (!mask) {
                        return mask.error();
                    }
                    op(AbstractOp::BINARY, [&](Code& m) {
                        m.ident(*and_);
                        m.left_ref(*shift);
                        m.right_ref(*mask);
                        m.bop(BinaryOp::bit_and);
                    });
                    auto cast = m.new_id(nullptr);
                    if (!cast) {
                        return cast.error();
                    }
                    auto target_type_storage = m.get_storage(target_type);
                    if (!target_type_storage) {
                        return target_type_storage.error();
                    }
                    auto casted = add_assign_cast(m, op, &*src_type_storage, &*target_type_storage, *and_);
                    if (!casted) {
                        return casted.error();
                    }
                    auto assign = m.new_id(nullptr);
                    if (!assign) {
                        return assign.error();
                    }
                    op(AbstractOp::ASSIGN, [&](Code& m) {
                        m.ident(*assign);
                        m.left_ref(code.ref().value());
                        m.right_ref(*casted ? **casted : *and_);
                    });
                    return none;
                };
                auto err = add_endian_specific(
                    m, dec_endian, op,
                    [&]() {
                        auto err = assign_to_ref(counter);
                        if (err) {
                            return err;
                        }
                        return add_counter(dec_bit_size);
                    },
                    [&]() {
                        auto err = add_counter(dec_bit_size);
                        if (err) {
                            return err;
                        }
                        auto bit_size_ = immediate(bit_size);
                        if (!bit_size_) {
                            return bit_size_.error();
                        }
                        auto sub = m.new_id(nullptr);
                        if (!sub) {
                            return sub.error();
                        }
                        op(AbstractOp::BINARY, [&](Code& m) {
                            m.ident(*sub);
                            m.left_ref(*bit_size_);
                            m.right_ref(counter);
                            m.bop(BinaryOp::sub);
                        });
                        return assign_to_ref(*sub);
                    });
                if (err) {
                    return err;
                }
            }
            else {
                new_code.push_back(code);
            }
        }
        m.code = std::move(new_code);
        return none;
    }

    Error sort_immediate(Module& m) {
        std::vector<Code> new_code;
        for (auto& code : m.code) {
            if (code.op == rebgn::AbstractOp::IMMEDIATE_INT || code.op == rebgn::AbstractOp::IMMEDIATE_INT64) {
                new_code.push_back(std::move(code));
            }
        }
        for (auto& code : m.code) {
            if (code.op != rebgn::AbstractOp::IMMEDIATE_INT && code.op != rebgn::AbstractOp::IMMEDIATE_INT64) {
                new_code.push_back(std::move(code));
            }
        }
        m.code = std::move(new_code);
        return none;
    }

    Error optimize(Module& m, const std::shared_ptr<ast::Node>& node) {
        auto err = flatten(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = decide_bit_field_size(m);
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
        replace_call_encode_decode_ref(m);
        rebind_ident_index(m);
        err = merge_conditional_field(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = derive_property_functions(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        analyze_encoder_decoder_traits(m);
        err = expand_bit_operation(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = sort_immediate(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = generate_cfg1(m);
        if (err) {
            return err;
        }
        rebind_ident_index(m);
        err = add_ident_ranges(m);
        if (err) {
            return err;
        }
        remap_programs(m);
        err = optimize_type_usage(m);
        if (err) {
            return err;
        }
        return none;
    }
}  // namespace rebgn
