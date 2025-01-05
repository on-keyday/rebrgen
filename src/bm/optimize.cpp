/*license*/
#include "convert.hpp"
#include <core/ast/tool/sort.h>
#include <view/span.h>
#include "helper.hpp"

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
                case AbstractOp::DEFINE_PARAMETER: {
                    auto err = do_extract(AbstractOp::DEFINE_PARAMETER, AbstractOp::END_PARAMETER);
                    if (err) {
                        return err;
                    }
                    break;
                }
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
            case AbstractOp::DEFINE_FIELD:
                return AbstractOp::DECLARE_FIELD;
            case AbstractOp::DEFINE_ENUM_MEMBER:
                return AbstractOp::DECLARE_ENUM_MEMBER;
            case AbstractOp::DEFINE_UNION_MEMBER:
                return AbstractOp::DECLARE_UNION_MEMBER;
            case AbstractOp::DEFINE_PROGRAM:
                return AbstractOp::DECLARE_PROGRAM;
            case AbstractOp::DEFINE_STATE:
                return AbstractOp::DECLARE_STATE;
            case AbstractOp::DEFINE_BIT_FIELD:
                return AbstractOp::DECLARE_BIT_FIELD;
            case AbstractOp::DEFINE_PARAMETER:
                return AbstractOp::DECLARE_PARAMETER;
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
                case AbstractOp::DEFINE_FIELD:
                    end_op = AbstractOp::END_FIELD;
                    break;
                case AbstractOp::DEFINE_ENUM_MEMBER:
                    end_op = AbstractOp::END_ENUM_MEMBER;
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
                case AbstractOp::DEFINE_PARAMETER:
                    end_op = AbstractOp::END_PARAMETER;
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

    expected<size_t> decide_maximum_bit_field_size(Module& m, std::set<ObjectID>& searched, AbstractOp end_op, size_t index) {
        size_t bit_size = 0;
        for (size_t i = index; m.code[i].op != end_op; i++) {
            if (m.code[i].op == AbstractOp::SPECIFY_STORAGE_TYPE) {
                auto storage = *m.code[i].storage();
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
                bit_size += factor;
            }
            if (m.code[i].op == AbstractOp::DECLARE_FIELD) {
                auto index = m.ident_index_table[m.code[i].ref().value().value()];
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_FIELD, index);
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
            rebound.push_back(std::move(c));
            if (c.op == AbstractOp::DEFINE_BIT_FIELD) {
                auto ident = c.ident().value().value();
                auto found = bit_fields.find(ident);
                if (found == bit_fields.end()) {
                    continue;
                }
                Storages storage;
                Storage s;
                s.type = StorageType::UINT;
                s.size(*varint(found->second));
                storage.storages.push_back(std::move(s));
                storage.length = *varint(1);
                rebound.push_back(make_code(AbstractOp::SPECIFY_STORAGE_TYPE, [&](auto& c) {
                    c.storage(std::move(storage));
                }));
            }
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
                    for (size_t i = idx + 1; i < m.code.size(); i++) {
                        if (m.code[i].op == AbstractOp::END_FIELD) {
                            break;
                        }
                        if (m.code[i].op == AbstractOp::SPECIFY_STORAGE_TYPE) {
                            auto storage = m.code[i].storage().value();
                            auto key = storage_key(storage);
                            ctx.type_to_storage[key] = std::move(storage);
                            auto& map = conditional_fields[parent];
                            map[key].push_back(target);
                            break;
                        }
                    }
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
                auto key = storage_key(c.storage().value());
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
                    auto common_type_key = storage_key(c.storage().value());
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
                    for (auto& m : found->second) {
                        if (ctx.exists.find(m) != ctx.exists.end()) {
                            continue;
                        }
                        auto& merged = ctx.merged_conditional_fields[m];
                        Param param;
                        param.expr_refs = std::move(merged.second);
                        param.len_exprs = *varint(param.expr_refs.size());
                        rebound.push_back(make_code(AbstractOp::MERGED_CONDITIONAL_FIELD, [&](auto& c) {
                            c.ident(*varint(m));
                            c.storage(ctx.type_to_storage[merged.first]);
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

    void retrieve_var(Module& m, const rebgn::Code& code, std::set<ObjectID>& variables) {
        std::vector<std::string> res;
        switch (code.op) {
            case rebgn::AbstractOp::ARRAY_SIZE: {
                retrieve_var(m, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::CALL_CAST: {
                for (auto& p : code.param().value().expr_refs) {
                    retrieve_var(m, m.code[m.ident_index_table[p.value()]], variables);
                }
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                retrieve_var(m, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                retrieve_var(m, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::APPEND: {
                retrieve_var(m, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                retrieve_var(m, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                retrieve_var(m, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                retrieve_var(m, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::ACCESS: {
                retrieve_var(m, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                retrieve_var(m, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                retrieve_var(m, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                retrieve_var(m, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::STATIC_CAST: {
                retrieve_var(m, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD:
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                retrieve_var(m, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                if (auto left_ref = code.left_ref(); left_ref->value() != 0) {
                    retrieve_var(m, m.code[m.ident_index_table[left_ref.value().value()]], variables);
                }
                retrieve_var(m, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                break;
            }
            case rebgn::AbstractOp::CALL: {
                retrieve_var(m, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                for (auto& p : code.param().value().expr_refs) {
                    retrieve_var(m, m.code[m.ident_index_table[p.value()]], variables);
                }
                break;
            }
            default:
                break;
        }
    }

    Error derive_function_from_merged_fields(Module& m) {
        std::vector<Code> funcs;
        auto op = [&](AbstractOp o, auto&& set) {
            funcs.push_back(make_code(o, set));
        };
        std::map<ObjectID, std::pair<Varint, Varint>> merged_fields;
        std::map<ObjectID, ObjectID> properties_to_merged;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
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
                    ident += "_" + property_name_suffix(m, c.storage().value());
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
                auto belong_of_belong = m.code[m.ident_index_table[c.belong().value().value()]].belong().value();
                auto merged_ident = c.ident().value();
                auto getter_setter = merged_fields[merged_ident.value()];
                auto getter_ident = getter_setter.first;
                auto setter_ident = getter_setter.second;
                auto mode = c.merge_mode().value();
                auto type = c.storage().value();
                auto originalType = type;
                auto param = c.param().value();
                auto do_foreach = [&](auto&& action) {
                    for (auto& c : param.expr_refs) {
                        auto& code = m.code[m.ident_index_table[c.value()]];
                        op(AbstractOp::IF, [&](Code& m) {
                            m.ref(*code.left_ref());
                        });
                        auto err = action(code);
                        if (err) {
                            return err;
                        }
                        op(AbstractOp::END_IF, [&](Code& m) {});
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
                op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& n) {
                    n.storage(type);
                });
                auto err = do_foreach([&](const Code& code) {
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
                        if (mode == MergeMode::COMMON_TYPE) {
                            op(AbstractOp::OPTIONAL_OF, [&](Code& m) {
                                m.ident(*ident);
                                m.ref(expr_ref);
                                m.storage(originalType);
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
                    });
                    return none;
                });
                if (err) {
                    return err;
                }
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
                });
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
                    n.storage(originalType);
                });
                op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& n) {
                    n.storage(Storages{
                        .length = *varint(1),
                        .storages = {Storage{.type = StorageType::BOOL}},
                    });
                });
                auto bool_ident = m.new_id(nullptr);
                if (!bool_ident) {
                    return bool_ident.error();
                }
                op(AbstractOp::IMMEDIATE_TRUE, [&](Code& m) {
                    m.ident(*bool_ident);
                });
                err = do_foreach([&](const Code& code) {
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
                        op(AbstractOp::ASSIGN, [&](Code& m) {
                            m.left_ref(expr_ref);
                            m.right_ref(*prop);
                        });
                    }
                    op(AbstractOp::RET, [&](Code& m) {
                        m.ref(*bool_ident);
                    });
                    return none;
                });
                if (err) {
                    return err;
                }
                auto false_ident = m.new_id(nullptr);
                if (!false_ident) {
                    return false_ident.error();
                }
                op(AbstractOp::IMMEDIATE_FALSE, [&](Code& m) {
                    m.ident(*false_ident);
                });
                op(AbstractOp::RET, [&](Code& m) {
                    m.ref(*false_ident);
                });
                op(AbstractOp::END_FUNCTION, [&](Code& m) {});
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
            rebound.push_back(std::move(c));
        }
        m.code = std::move(rebound);
        m.code.insert(m.code.end(), funcs.begin(), funcs.end());
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
        err = derive_function_from_merged_fields(m);
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
        return none;
    }
}  // namespace rebgn
