/*license*/
#pragma once
#include "convert.hpp"

namespace rebgn {
    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n);
    Error convert_node_encode(Module& m, const std::shared_ptr<ast::Node>& n);
    Error convert_node_decode(Module& m, const std::shared_ptr<ast::Node>& n);

    template <class T>
    Error define(Module& m, std::shared_ptr<T>& node) = delete;

    template <class T>
    Error encode(Module& m, std::shared_ptr<T>& node) = delete;

    template <class T>
    Error decode(Module& m, std::shared_ptr<T>& node) = delete;

    expected<Varint> immediate(Module& m, std::uint64_t n);
    expected<Varint> define_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level);
    expected<Varint> define_counter(Module& m, std::uint64_t init);
    Error define_storage(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive = false);
    expected<Varint> get_expr(Module& m, const std::shared_ptr<ast::Expr>& n);

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref);
    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref);

    Error counter_loop(Module& m, Varint length, auto&& block) {
        auto counter = define_counter(m, 0);
        if (!counter) {
            return counter.error();
        }
        auto cmp = m.new_id();
        if (!cmp) {
            return cmp.error();
        }
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*cmp);
            c.bop(BinaryOp::less);
            c.left_ref(*counter);
            c.right_ref(length);
        });
        m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
            c.ref(*cmp);
        });
        auto err = block(*counter);
        if (err) {
            return err;
        }
        m.op(AbstractOp::INC, [&](Code& c) {
            c.ref(*counter);
        });
        m.op(AbstractOp::END_LOOP);
        return none;
    }

    void add_switch_union(Module& m, std::shared_ptr<ast::StructType>& node);

    Error convert_if(Module& m, std::shared_ptr<ast::If>& node, auto&& eval) {
        std::optional<Varint> yield_value;
        if (!ast::as<ast::VoidType>(node->expr_type)) {
            Storages s;
            auto err = define_storage(m, s, node->expr_type);
            if (err) {
                return err;
            }
            auto new_id = m.new_id();
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.storage(std::move(s));
            });
            auto tmp_var = define_tmp_var(m, *new_id, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            yield_value = *tmp_var;
        }
        auto yield_value_preproc = [&]() {
            if (yield_value) {
                m.set_prev_expr(null_id);
            }
        };
        auto yield_value_postproc = [&]() {
            if (yield_value) {
                auto prev_expr = m.get_prev_expr();
                if (!prev_expr) {
                    return prev_expr.error();
                }
                m.op(AbstractOp::ASSIGN, [&](Code& c) {
                    c.left_ref(*yield_value);
                    c.right_ref(*prev_expr);
                });
            }
            return none;
        };
        auto yield_value_finalproc = [&]() {
            if (yield_value) {
                m.set_prev_expr(yield_value->value());
            }
        };
        auto cond = get_expr(m, node->cond->expr);
        if (!cond) {
            return cond.error();
        }
        auto if_ = m.new_id();
        if (!if_) {
            return error("Failed to generate new id");
        }
        m.op(AbstractOp::IF, [&](Code& c) {
            c.ident(*if_);
            c.ref(*cond);
        });
        add_switch_union(m, node->then->struct_type);
        for (auto& n : node->then->elements) {
            yield_value_preproc();
            auto err = eval(m, n);
            if (err) {
                return err;
            }
        }
        yield_value_postproc();
        if (node->els) {
            std::shared_ptr<ast::If> els = node;
            while (els) {
                if (auto e = ast::as<ast::If>(els->els)) {
                    auto cond = get_expr(m, e->cond->expr);
                    if (!cond) {
                        return cond.error();
                    }
                    m.op(AbstractOp::ELIF, [&](Code& c) {
                        c.ref(*cond);
                    });
                    add_switch_union(m, e->then->struct_type);
                    for (auto& n : e->then->elements) {
                        yield_value_preproc();
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                    auto err = yield_value_postproc();
                    if (err) {
                        return err;
                    }
                    els = ast::cast_to<ast::If>(e->els);
                }
                else if (auto block = ast::as<ast::IndentBlock>(els->els)) {
                    m.op(AbstractOp::ELSE);
                    add_switch_union(m, block->struct_type);
                    for (auto& n : block->elements) {
                        yield_value_preproc();
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                    auto err = yield_value_postproc();
                    if (err) {
                        return err;
                    }
                    break;
                }
                else {
                    break;
                }
            }
        }
        m.op(AbstractOp::END_IF);
        yield_value_finalproc();
        return none;
    }

    Error convert_match(Module& m, std::shared_ptr<ast::Match>& node, auto&& eval) {
        std::optional<Varint> yield_value;
        if (!ast::as<ast::VoidType>(node->expr_type)) {
            Storages s;
            auto err = define_storage(m, s, node->expr_type);
            if (err) {
                return err;
            }
            auto new_id = m.new_id();
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.storage(std::move(s));
            });
            auto tmp_var = define_tmp_var(m, *new_id, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            yield_value = *tmp_var;
        }
        auto yield_value_preproc = [&]() {
            if (yield_value) {
                m.set_prev_expr(null_id);
            }
        };
        auto yield_value_postproc = [&]() {
            if (yield_value) {
                auto prev_expr = m.get_prev_expr();
                if (!prev_expr) {
                    return prev_expr.error();
                }
                m.op(AbstractOp::ASSIGN, [&](Code& c) {
                    c.left_ref(*yield_value);
                    c.right_ref(*prev_expr);
                });
            }
            return none;
        };
        auto yield_value_finalproc = [&]() {
            if (yield_value) {
                m.set_prev_expr(yield_value->value());
            }
        };
        if (node->cond) {
            auto cond = get_expr(m, node->cond);
            if (!cond) {
                return cond.error();
            }
            if (node->struct_union_type->exhaustive) {
                m.op(AbstractOp::EXHAUSTIVE_MATCH, [&](Code& c) {
                    c.ref(*cond);
                });
            }
            else {
                m.op(AbstractOp::MATCH, [&](Code& c) {
                    c.ref(*cond);
                });
            }
            for (auto& c : node->branch) {
                if (ast::is_any_range(c->cond->expr)) {
                    m.op(AbstractOp::DEFAULT_CASE);
                }
                else {
                    auto cond = get_expr(m, c->cond->expr);
                    if (!cond) {
                        return cond.error();
                    }
                    m.op(AbstractOp::CASE, [&](Code& c) {
                        c.ref(*cond);
                    });
                }
                if (auto scoped = ast::as<ast::ScopedStatement>(c->then)) {
                    add_switch_union(m, scoped->struct_type);
                    yield_value_preproc();
                    auto err = eval(m, scoped->statement);
                    if (err) {
                        return err;
                    }
                }
                else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                    add_switch_union(m, block->struct_type);
                    for (auto& n : block->elements) {
                        yield_value_preproc();
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                }
                else {
                    return error("Invalid match branch");
                }
                auto err = yield_value_postproc();
                if (err) {
                    return err;
                }
                m.op(AbstractOp::END_CASE);
            }
            m.op(AbstractOp::END_MATCH);
            yield_value_finalproc();
            return none;
        }
        if (node->trial_match) {
            return error("Trial match is not supported yet");
        }
        // translate into if-else
        std::shared_ptr<ast::Node> last = nullptr;
        for (auto& c : node->branch) {
            if (ast::is_any_range(c->cond->expr)) {
                // if first branch, so we don't need to use if-elses
                if (last) {
                    m.op(AbstractOp::ELSE);
                    last = c;
                }
            }
            else {
                auto cond = get_expr(m, c->cond->expr);
                if (!cond) {
                    return cond.error();
                }
                if (!last) {
                    auto if_ = m.new_id();
                    if (!if_) {
                        return error("Failed to generate new id");
                    }
                    m.op(AbstractOp::IF, [&](Code& c) {
                        c.ident(*if_);
                        c.ref(*cond);
                    });
                    last = c;
                }
                else {
                    m.op(AbstractOp::ELIF, [&](Code& c) {
                        c.ref(*cond);
                    });
                    last = c;
                }
            }
            if (auto scoped = ast::as<ast::ScopedStatement>(c->then)) {
                add_switch_union(m, scoped->struct_type);
                yield_value_preproc();
                auto err = eval(m, scoped->statement);
                if (err) {
                    return err;
                }
            }
            else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                add_switch_union(m, block->struct_type);
                for (auto& n : block->elements) {
                    yield_value_preproc();
                    auto err = eval(m, n);
                    if (err) {
                        return err;
                    }
                }
            }
            else {
                return error("Invalid match branch");
            }
            auto err = yield_value_postproc();
            if (err) {
                return err;
            }
        }
        if (last) {
            m.op(AbstractOp::END_IF);
        }
        yield_value_finalproc();
        return none;
    }
}  // namespace rebgn
