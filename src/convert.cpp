/*license*/
#include "convert.hpp"
#include <core/ast/traverse.h>
#include <format>

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

    expected<Varint> immediate(Module& m, std::uint64_t n) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        auto val = varint(n);
        if (!val) {
            m.op(AbstractOp::IMMEDIATE_INT64, [&](Code& c) {
                c.int_value64(n);
                c.ident(*ident);
            });
        }
        else {
            m.op(AbstractOp::IMMEDIATE_INT, [&](Code& c) {
                c.int_value(*val);
                c.ident(*ident);
            });
        }
        return ident;
    }

    expected<Varint> define_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        m.op(AbstractOp::DEFINE_VARIABLE, [&](Code& c) {
            c.ident(*ident);
            c.ref(init_ref);
        });
        return ident;
    }

    expected<Varint> define_counter(Module& m, std::uint64_t init) {
        auto init_ref = immediate(m, init);
        if (!init_ref) {
            return init_ref;
        }
        return define_tmp_var(m, *init_ref, ast::ConstantLevel::variable);
    }

    template <>
    Error define<ast::IntLiteral>(Module& m, std::shared_ptr<ast::IntLiteral>& node) {
        auto n = node->parse_as<std::uint64_t>();
        if (!n) {
            return error("Invalid integer literal: {}", node->value);
        }
        auto id = immediate(m, *n);
        if (!id) {
            return id.error();
        }
        m.prev_expr_id = id->value();
        return none;
    }

    template <>
    Error define<ast::StrLiteral>(Module& m, std::shared_ptr<ast::StrLiteral>& node) {
        auto str_ref = m.lookup_string(node->value);
        if (!str_ref) {
            return str_ref.error();
        }
        m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
            c.ident(*str_ref);
        });
        m.prev_expr_id = str_ref->value();
        return none;
    }

    template <>
    Error define<ast::Ident>(Module& m, std::shared_ptr<ast::Ident>& node) {
        auto ident = m.lookup_ident(node);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::IDENT_REF, [&](Code& c) {
            c.ref(*ident);
        });
        m.prev_expr_id = ident->value();
        return none;
    }

    template <>
    Error define<ast::MemberAccess>(Module& m, std::shared_ptr<ast::MemberAccess>& node) {
        auto err = convert_node_definition(m, node->target);
        if (err) {
            return err;
        }
        auto base = m.prev_expr_id;
        auto ident = m.lookup_ident(node->member);
        if (!ident) {
            return ident.error();
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::ACCESS, [&](Code& c) {
            c.ident(*new_id);
            c.left_ref(*varint(base));
            c.right_ref(*ident);
        });
        m.prev_expr_id = new_id->value();
        return none;
    }

    Error define_storage(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ) {
        auto push = [&](StorageType t, auto&& set) {
            Storage c;
            c.type = t;
            set(c);
            s.storages.push_back(c);
            s.length.value(s.length.value() + 1);
        };
        auto typ_with_size = [&](StorageType t, auto&& typ) {
            if (!typ->bit_size) {
                return error("Invalid integer type(maybe bug)");
            }
            auto bit_size = varint(*typ->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            push(t, [&](Storage& c) {
                c.size(*bit_size);
            });
            return none;
        };
        if (auto i = ast::as<ast::IntType>(typ)) {
            return typ_with_size(i->is_signed ? StorageType::INT : StorageType::UINT, i);
        }
        if (auto f = ast::as<ast::FloatType>(typ)) {
            return typ_with_size(StorageType::FLOAT, f);
        }
        if (auto f = ast::as<ast::StrLiteralType>(typ)) {
            auto len = f->bit_size;
            if (!len) {
                return error("Invalid string literal type(maybe bug)");
            }
            push(StorageType::ARRAY, [&](Storage& c) {
                c.size(*varint(*len / 8));
            });
            push(StorageType::UINT, [&](Storage& c) {
                c.size(*varint(8));
            });
            auto ref = m.lookup_string(f->strong_ref->value);
            if (!ref) {
                return ref.error();
            }
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(*ref);
            });
            m.op(AbstractOp::SPECIFY_FIXED_VALUE, [&](Code& c) {
                c.ref(*ref);
            });
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return define_storage(m, s, base_type);
        }
        if (auto i = ast::as<ast::StructType>(typ)) {
            auto l = i->base.lock();
            if (auto member = ast::as<ast::Member>(l)) {
                auto ident = m.lookup_ident(member->ident);
                if (!ident) {
                    return ident.error();
                }
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(*ident);
                });
                return none;
            }
            return error("unknown struct type");
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("Invalid enum type(maybe bug)");
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            push(StorageType::ENUM, [&](Storage& c) {
                c.ref(*ident);
            });
            auto base_type = base_enum->base_type;
            if (!base_type) {
                return none;  // this is abstract enum
            }
            return define_storage(m, s, base_type);
        }
        if (auto su = ast::as<ast::StructUnionType>(typ)) {
            auto ident = m.new_id();
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_UNION, [&](Code& c) {
                c.ident(*ident);
            });
            auto size = su->structs.size();
            push(StorageType::VARIANT, [&](Storage& c) {
                c.size(*varint(size));
                c.ref(*ident);
            });
            for (auto& st : su->structs) {
                auto ident = m.new_id();
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::DEFINE_UNION_MEMBER, [&](Code& c) {
                    c.ident(*ident);
                });
                for (auto& f : st->fields) {
                    auto err = convert_node_definition(m, f);
                    if (err) {
                        return err;
                    }
                }
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(*ident);
                });
                m.op(AbstractOp::END_UNION_MEMBER);
            }
            m.op(AbstractOp::END_UNION);
            return none;
        }
        if (auto a = ast::as<ast::ArrayType>(typ)) {
            if (a->length_value) {
                push(StorageType::ARRAY, [&](Storage& c) {
                    c.size(*varint(*a->length_value));
                });
            }
            else {
                auto err = convert_node_definition(m, a->length);
                if (err) {
                    return err;
                }
                auto length_ref = m.prev_expr_id;
                push(StorageType::VECTOR, [&](Storage& c) {
                    c.ref(*varint(length_ref));
                });
            }
            auto err = define_storage(m, s, a->element_type);
            if (err) {
                return err;
            }
            return none;
        }
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    template <>
    Error define<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FIELD, [&](Code& c) {
            c.ident(*ident);
        });
        if (!ast::as<ast::UnionType>(node->field_type)) {
            Storages s;
            auto err = define_storage(m, s, node->field_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
        }
        m.op(AbstractOp::END_FIELD);
        return none;
    }

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

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(base_ref);
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            m.op(AbstractOp::ENCODE_FLOAT, [&](Code& c) {
                c.ref(base_ref);
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
            counter_loop(m, *max_len, [&](Varint counter) {
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
            // len = <len>
            // i = 0
            // cmp = i < len
            // loop cmp:
            //   index = base_ref[i]
            //   encode_type(index)
            //   i++
            auto err = convert_node_encode(m, arr->length);
            if (err) {
                return err;
            }
            auto len_init = varint(m.prev_expr_id);
            if (!len_init) {
                return len_init.error();
            }
            auto len_ident = define_tmp_var(m, *len_init, ast::ConstantLevel::immutable_variable);
            if (!len_ident) {
                return len_ident.error();
            }
            counter_loop(m, *len_ident, [&](Varint counter) {
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
            });
            return none;
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
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(base_ref);
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            m.op(AbstractOp::DECODE_FLOAT, [&](Code& c) {
                c.ref(base_ref);
            });
            return none;
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = m.lookup_string(str_ty->strong_ref->value);
            if (!str_ref) {
                return str_ref.error();
            }
            // str_ref = <string>
            // counter_init = 0
            // counter_ident = counter_init
            // max_len = <max_len>
            // cmp = counter_ident < max_len
            // loop cmp:
            //   index = str_ref[counter_ident]
            //   encode_int(index)
            //   counter_ident++
            // end_loop
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(*str_ref);
            });
            auto counter = m.new_id();
            if (!counter) {
                return error("Failed to generate new id");
            }
            auto counter = define_counter(m, 0);
            if (!counter) {
                return counter.error();
            }
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            auto cmp = m.new_id();
            if (!cmp) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::BINARY, [&](Code& c) {
                c.ident(*cmp);
                c.bop(BinaryOp::less);
                c.left_ref(*counter);
                c.right_ref(*max_len);
            });
            m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
                c.ref(*cmp);
            });
            auto index = m.new_id();
            if (!index) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::INDEX, [&](Code& c) {
                c.ident(*index);
                c.left_ref(*str_ref);
                c.right_ref(*counter);
            });
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(*index);
            });
            m.op(AbstractOp::INC, [&](Code& c) {
                c.ref(*counter);
            });
            m.op(AbstractOp::END_LOOP);
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
            // len = <len>
            // i = 0
            // cmp = i < len
            // loop cmp:
            //   index = base_ref[i]
            //   encode_type(index)
            //   i++
            auto err = convert_node_encode(m, arr->length);
            if (err) {
                return err;
            }
            auto id = varint(m.prev_expr_id);
            if (!id) {
                return id.error();
            }
            auto len_ident = define_tmp_var(m, *id, ast::ConstantLevel::immutable_variable);
            if (!len_ident) {
                return len_ident.error();
            }
            auto cmp = m.new_id();
            if (!cmp) {
                return cmp.error();
            }
            auto i_ = define_counter(m, 0);
            if (!i_) {
                return i_.error();
            }
            m.op(AbstractOp::BINARY, [&](Code& c) {
                c.ident(*cmp);
                c.bop(BinaryOp::less);
                c.left_ref(*i_);
                c.right_ref(*len_ident);
            });
            m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
                c.ref(*cmp);
            });
            auto index = m.new_id();
            if (!index) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::INDEX, [&](Code& c) {
                c.ident(*index);
                c.left_ref(base_ref);
                c.right_ref(*len_ident);
            });
            err = encode_type(m, arr->element_type, *index);
            if (err) {
                return err;
            }
            m.op(AbstractOp::INC, [&](Code& c) {
                c.ref(*i_);
            });
            m.op(AbstractOp::END_LOOP);
            return none;
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
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    template <>
    Error encode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        encode_type(m, node->field_type, *ident);
    }

    template <>
    Error define<ast::Function>(Module& m, std::shared_ptr<ast::Function>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*ident);
        });
        for (auto& p : node->parameters) {
            auto err = convert_node_definition(m, p);
            if (err) {
                return err;
            }
        }
        for (auto& f : node->body->elements) {
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FUNCTION);
        return none;
    }

    template <>
    Error define<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FORMAT, [&](Code& c) {
            c.ident(*ident);
        });
        if (node->body->struct_type->type_map) {
            Storages s;
            auto err = define_storage(m, s, node->body->struct_type->type_map->type_literal);
            if (err) {
                return err;
            }
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
        }
        for (auto& f : node->body->struct_type->fields) {
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FORMAT);
        return none;
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
        });
        for (auto& elem : node->body->elements) {
            auto err = convert_node_encode(m, elem);
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
    Error define<ast::Enum>(Module& m, std::shared_ptr<ast::Enum>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_ENUM, [&](Code& c) {
            c.ident(*ident);
        });
        for (auto& me : node->members) {
            auto ident = m.lookup_ident(me->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_ENUM_MEMBER, [&](Code& c) {
                c.ident(*ident);
            });
            auto err = convert_node_definition(m, me->value);
            if (err) {
                return err;
            }
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(*ident);
                c.right_ref(*varint(m.prev_expr_id));
            });
            m.op(AbstractOp::END_ENUM_MEMBER);
        }
        m.op(AbstractOp::END_ENUM);
        return none;
    }

    template <>
    Error define<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        auto err = convert_node_definition(m, node->left);
        if (err) {
            return err;
        }
        auto left_ref = m.prev_expr_id;
        if (node->op == ast::BinaryOp::logical_and ||
            node->op == ast::BinaryOp::logical_or) {
            m.op(AbstractOp::SHORT_CIRCUIT, [&](Code& c) {
                c.ref(*ident);
            });
        }
        auto err2 = convert_node_definition(m, node->right);
        if (err2) {
            return err2;
        }
        auto bop = BinaryOp(node->op);
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*ident);
            c.bop(bop);
            c.left_ref(*varint(left_ref));
            c.right_ref(*varint(m.prev_expr_id));
        });
        return none;
    }

    template <>
    Error encode<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        return define<ast::Binary>(m, node);
    }

    template <>
    Error decode<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        return define<ast::Binary>(m, node);
    }

    template <>
    Error define<ast::Metadata>(Module& m, std::shared_ptr<ast::Metadata>& node) {
        auto node_name = m.lookup_string(node->name);
        if (!node_name) {
            return node_name.error();
        }
        std::vector<Varint> values;
        for (auto& value : node->values) {
            auto err = convert_node_definition(m, value);
            if (err) {
                return err;
            }
            auto val = varint(m.prev_expr_id);
            if (!val) {
                return val.error();
            }
            values.push_back(*val);
        }
        Metadata md;
        md.name = *node_name;
        md.expr_refs = std::move(values);
        auto length = varint(md.expr_refs.size());
        if (!length) {
            return length.error();
        }
        md.len_exprs = length.value();
        m.op(AbstractOp::METADATA, [&](Code& c) {
            c.metadata(std::move(md));
        });
        return none;
    }

    template <>
    Error define<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        for (auto& n : node->elements) {
            auto err = convert_node_definition(m, n);
            if (err) {
                return err;
            }
        }
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

    template <class T>
    concept has_define = requires(Module& m, std::shared_ptr<T>& n) {
        define<T>(m, n);
    };

    template <class T>
    concept has_encode = requires(Module& m, std::shared_ptr<T>& n) {
        encode<T>(m, n);
    };

    template <class T>
    concept has_decode = requires(Module& m, std::shared_ptr<T>& n) {
        decode<T>(m, n);
    };

    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_define<T>) {
                err = define<T>(m, node);
            }
        });
        return err;
    }

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

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node) {
        Module m;
        auto err = convert_node_definition(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_encode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_decode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        return m;
    }
}  // namespace rebgn
