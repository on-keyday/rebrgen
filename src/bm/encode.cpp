/*license*/

#include "internal.hpp"
#include <core/ast/tool/eval.h>

namespace rebgn {
    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            auto bit_size = varint(*int_ty->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(base_ref);
                c.endian(Endian(int_ty->endian));
                c.bit_size(*bit_size);
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            Storages to;
            auto err = define_storage(m, to, std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false));
            if (err) {
                return err;
            }
            auto new_id = m.new_id();
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::BIT_CAST, [&](Code& c) {
                c.ident(*new_id);
                c.ref(base_ref);
                c.storage(std::move(to));
            });
            auto bit_size = varint(*float_ty->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(*new_id);
                c.endian(Endian(float_ty->endian));
                c.bit_size(*bit_size);
            });
            return none;
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = m.lookup_string(str_ty->strong_ref->value);
            if (!str_ref) {
                return str_ref.error();
            }
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(*str_ref);
            });
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            return counter_loop(m, *max_len, [&](Varint counter) {
                auto index = m.new_id();
                if (!index) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INDEX, [&](Code& c) {
                    c.ident(*index);
                    c.left_ref(*str_ref);
                    c.right_ref(counter);
                });
                m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                    c.ref(*index);
                });
                return none;
            });
        }
        if (auto arr = ast::as<ast::ArrayType>(typ)) {
            auto len = arr->length_value;
            if (len) {
                for (size_t i = 0; i < *len; i++) {
                    auto index = m.new_id();
                    if (!index) {
                        return error("Failed to generate new id");
                    }
                    auto i_ = immediate(m, i);
                    if (!i_) {
                        return i_.error();
                    }
                    m.op(AbstractOp::INDEX, [&](Code& c) {
                        c.ident(*index);
                        c.left_ref(base_ref);
                        c.right_ref(*i_);
                    });
                    auto err = encode_type(m, arr->element_type, *index);
                    if (err) {
                        return err;
                    }
                }
                return none;
            }
            auto len_init = get_expr(m, arr->length);
            if (!len_init) {
                return len_init.error();
            }
            auto len_ = define_tmp_var(m, *len_init, ast::ConstantLevel::immutable_variable);
            if (!len_) {
                return len_.error();
            }
            return counter_loop(m, *len_, [&](Varint counter) {
                auto index = m.new_id();
                if (!index) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INDEX, [&](Code& c) {
                    c.ident(*index);
                    c.left_ref(base_ref);
                    c.right_ref(counter);
                });
                auto err = encode_type(m, arr->element_type, *index);
                if (err) {
                    return err;
                }
                return none;
            });
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Member>(member)) {
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::CALL_ENCODE, [&](Code& c) {
                    c.left_ref(*ident);
                    c.right_ref(base_ref);
                });
                return none;
            }
            return error("unknown struct type");
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (!base_type) {
                return error("abstract enum {} in encode", base_enum->ident->ident);
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            auto casted = m.new_id();
            if (!casted) {
                return casted.error();
            }
            m.op(AbstractOp::ENUM_TO_INT_CAST, [&](Code& c) {
                c.ident(*casted);
                c.left_ref(*ident);
                c.right_ref(base_ref);
            });
            auto err = encode_type(m, base_type, *casted);
            if (err) {
                return err;
            }
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return encode_type(m, base_type, base_ref);
        }
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(base_ref);
                c.endian(Endian(int_ty->endian));
                c.bit_size(*varint(*int_ty->bit_size));
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            auto new_id = m.new_id();
            if (!new_id) {
                return error("Failed to generate new id");
            }
            Storages from, to;
            auto err = define_storage(m, from, std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false));
            if (err) {
                return err;
            }
            err = define_storage(m, to, typ);
            if (err) {
                return err;
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.storage(std::move(from));
            });
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(*new_id);
                c.endian(Endian::unspec);
                c.bit_size(*varint(*float_ty->bit_size));
            });
            auto next_id = m.new_id();
            if (!next_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::BIT_CAST, [&](Code& c) {
                c.ident(*next_id);
                c.storage(std::move(to));
                c.ref(*new_id);
            });
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(base_ref);
                c.right_ref(*next_id);
            });
            return none;
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = m.lookup_string(str_ty->strong_ref->value);
            if (!str_ref) {
                return str_ref.error();
            }
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(*str_ref);
            });
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            return counter_loop(m, *max_len, [&](Varint counter) {
                auto tmp = m.new_id();
                if (!tmp) {
                    return error("Failed to generate new id");
                }
                Storages s;
                auto err = define_storage(m, s, std::make_shared<ast::IntType>(str_ty->loc, 8, ast::Endian::unspec, false));
                if (err) {
                    return err;
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*tmp);
                    c.storage(std::move(s));
                });
                auto tmp_var = define_tmp_var(m, *tmp, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                    c.ref(*tmp_var);
                    c.endian(Endian::unspec);
                    c.bit_size(*varint(8));
                });
                auto index = m.new_id();
                if (!index) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INDEX, [&](Code& c) {
                    c.ident(*index);
                    c.left_ref(*str_ref);
                    c.right_ref(counter);
                });
                auto cmp = m.new_id();
                if (!cmp) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::BINARY, [&](Code& c) {
                    c.ident(*cmp);
                    c.bop(BinaryOp::equal);
                    c.left_ref(*index);
                    c.right_ref(*tmp_var);
                });
                m.op(AbstractOp::ASSERT, [&](Code& c) {
                    c.ref(*cmp);
                });
                return none;
            });
        }
        if (auto arr = ast::as<ast::ArrayType>(typ)) {
            auto len = arr->length_value;
            if (len) {
                for (size_t i = 0; i < *len; i++) {
                    auto index = m.new_id();
                    if (!index) {
                        return error("Failed to generate new id");
                    }
                    auto i_ = immediate(m, i);
                    if (!i_) {
                        return i_.error();
                    }
                    m.op(AbstractOp::INDEX, [&](Code& c) {
                        c.ident(*index);
                        c.left_ref(base_ref);
                        c.right_ref(*i_);
                    });
                    auto err = decode_type(m, arr->element_type, *index);
                    if (err) {
                        return err;
                    }
                }
                return none;
            }
            // len = <len>
            // i = 0
            // cmp = i < len
            // loop cmp:
            //   index = base_ref[i]
            //   encode_type(index)
            //   i++
            auto id = get_expr(m, arr->length);
            if (!id) {
                return id.error();
            }
            auto len_ident = define_tmp_var(m, *id, ast::ConstantLevel::immutable_variable);
            if (!len_ident) {
                return len_ident.error();
            }
            return counter_loop(m, *len_ident, [&](Varint counter) {
                auto new_obj = m.new_id();
                if (!new_obj) {
                    return new_obj.error();
                }
                Storages s;
                auto err = define_storage(m, s, arr->element_type);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*new_obj);
                    c.storage(std::move(s));
                });
                auto tmp_var = define_tmp_var(m, *new_obj, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                err = decode_type(m, arr->element_type, *tmp_var);
                if (err) {
                    return err;
                }
                auto append = m.new_id();
                if (!append) {
                    return append.error();
                }
                m.op(AbstractOp::APPEND, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*tmp_var);
                });
                return none;
            });
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Member>(member)) {
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::CALL_DECODE, [&](Code& c) {
                    c.left_ref(*ident);
                    c.right_ref(base_ref);
                });
                return none;
            }
            return error("unknown struct type");
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (!base_type) {
                return error("abstract enum {} in encode", base_enum->ident->ident);
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            auto storage = m.new_id();
            if (!storage) {
                return storage.error();
            }
            Storages s;
            auto err = define_storage(m, s, base_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*storage);
                c.storage(std::move(s));
            });
            auto tmp_var = define_tmp_var(m, *storage, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            err = decode_type(m, base_type, *tmp_var);
            if (err) {
                return err;
            }
            auto casted = m.new_id();
            if (!casted) {
                return casted.error();
            }
            m.op(AbstractOp::INT_TO_ENUM_CAST, [&](Code& c) {
                c.ident(*casted);
                c.left_ref(*ident);
                c.right_ref(*tmp_var);
            });
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(base_ref);
                c.right_ref(*casted);
            });
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return decode_type(m, base_type, base_ref);
        }
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    template <>
    Error encode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        return encode_type(m, node->field_type, *ident);
    }

    template <>
    Error decode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        return decode_type(m, node->field_type, *ident);
    }

    template <>
    Error encode<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto fmt_ident = m.lookup_ident(node->ident);
        if (!fmt_ident) {
            return fmt_ident.error();
        }
        auto fn = node->encode_fn.lock();
        if (fn) {
            auto ident = m.lookup_ident(fn->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_ENCODER, [&](Code& c) {
                c.left_ref(*fmt_ident);
                c.right_ref(*ident);
            });
            return none;
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(*fmt_ident);
        });
        for (auto& elem : node->body->elements) {
            auto err = convert_node_encode(m, elem);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FUNCTION);
        m.op(AbstractOp::DEFINE_ENCODER, [&](Code& c) {
            c.left_ref(*fmt_ident);
            c.right_ref(*new_id);
        });
        return none;
    }

    template <>
    Error decode<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto fmt_ident = m.lookup_ident(node->ident);
        if (!fmt_ident) {
            return fmt_ident.error();
        }
        auto fn = node->decode_fn.lock();
        if (fn) {
            auto ident = m.lookup_ident(fn->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_DECODER, [&](Code& c) {
                c.left_ref(*fmt_ident);
                c.right_ref(*ident);
            });
            return none;
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(fmt_ident.value());
        });
        for (auto& elem : node->body->elements) {
            auto err = convert_node_decode(m, elem);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FUNCTION);
        m.op(AbstractOp::DEFINE_DECODER, [&](Code& c) {
            c.left_ref(*fmt_ident);
            c.right_ref(*new_id);
        });
        return none;
    }

    template <>
    Error encode<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        for (auto& n : node->elements) {
            auto err = convert_node_encode(m, n);
            if (err) {
                return err;
            }
        }
        return none;
    }

    template <>
    Error decode<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        for (auto& n : node->elements) {
            auto err = convert_node_decode(m, n);
            if (err) {
                return err;
            }
        }
        return none;
    }

    template <>
    Error encode<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error encode<ast::Unary>(Module& m, std::shared_ptr<ast::Unary>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Unary>(Module& m, std::shared_ptr<ast::Unary>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error encode<ast::Paren>(Module& m, std::shared_ptr<ast::Paren>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Paren>(Module& m, std::shared_ptr<ast::Paren>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error encode<ast::MemberAccess>(Module& m, std::shared_ptr<ast::MemberAccess>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::MemberAccess>(Module& m, std::shared_ptr<ast::MemberAccess>& node) {
        return convert_node_definition(m, node);
    }

    void add_switch_union(Module& m, std::shared_ptr<ast::StructType>& node) {
        auto ident = m.lookup_struct(node);
        if (ident.value() == 0) {
            return;
        }
        m.op(AbstractOp::SWITCH_UNION, [&](Code& c) {
            c.ref(ident);
        });
    }

    Error convert_match(Module& m, std::shared_ptr<ast::Match>& node, auto&& eval) {
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
                    auto err = eval(m, scoped->statement);
                    if (err) {
                        return err;
                    }
                }
                else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                    add_switch_union(m, block->struct_type);
                    for (auto& n : block->elements) {
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                }
                else {
                    return error("Invalid match branch");
                }
                m.op(AbstractOp::END_CASE);
            }
            m.op(AbstractOp::END_MATCH);
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
                auto err = eval(m, scoped->statement);
                if (err) {
                    return err;
                }
            }
            else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                add_switch_union(m, block->struct_type);
                for (auto& n : block->elements) {
                    auto err = eval(m, n);
                    if (err) {
                        return err;
                    }
                }
            }
            else {
                return error("Invalid match branch");
            }
        }
        if (last) {
            m.op(AbstractOp::END_IF);
        }
        return none;
    }

    template <>
    Error encode<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    Error convert_if(Module& m, std::shared_ptr<ast::If>& node, auto&& eval) {
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
            auto err = eval(m, n);
            if (err) {
                return err;
            }
        }
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
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                    els = ast::cast_to<ast::If>(e->els);
                }
                else if (auto block = ast::as<ast::IndentBlock>(els->els)) {
                    m.op(AbstractOp::ELSE);
                    add_switch_union(m, block->struct_type);
                    for (auto& n : block->elements) {
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                    }
                    break;
                }
                else {
                    break;
                }
            }
        }
        m.op(AbstractOp::END_IF);
        return none;
    }

    template <>
    Error encode<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    Error convert_loop(Module& m, std::shared_ptr<ast::Loop>& node, auto&& eval) {
        if (node->init) {
            auto err = eval(m, node->init);
            if (err) {
                return err;
            }
        }
        if (node->cond) {
            auto cond = get_expr(m, node->cond);
            if (!cond) {
                return cond.error();
            }
            m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
                c.ref(*cond);
            });
        }
        else {
            m.op(AbstractOp::LOOP_INFINITE);
        }
        for (auto& n : node->body->elements) {
            auto err = eval(m, n);
            if (err) {
                return err;
            }
        }
        if (node->step) {
            auto err = eval(m, node->step);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_LOOP);
        return none;
    }

    template <>
    Error encode<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    template <>
    Error encode<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error decode<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error encode<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error decode<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error encode<ast::Return>(Module& m, std::shared_ptr<ast::Return>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Return>(Module& m, std::shared_ptr<ast::Return>& node) {
        return convert_node_definition(m, node);
    }

    template <class T>
    concept has_encode = requires(Module& m, std::shared_ptr<T>& n) {
        encode<T>(m, n);
    };

    template <class T>
    concept has_decode = requires(Module& m, std::shared_ptr<T>& n) {
        decode<T>(m, n);
    };

    Error convert_node_encode(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_encode<T>) {
                err = encode<T>(m, node);
            }
        });
        return err;
    }

    Error convert_node_decode(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_decode<T>) {
                err = decode<T>(m, node);
            }
        });
        return err;
    }

}  // namespace rebgn