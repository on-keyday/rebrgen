/*license*/

#include "internal.hpp"
#include <core/ast/tool/eval.h>

namespace rebgn {
    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type) {
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
            auto new_id = m.new_node_id(typ);
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
            auto str_ref = m.lookup_string(str_ty->strong_ref->value, &str_ty->strong_ref->loc);
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
                auto index = m.new_id(nullptr);
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
            auto elem_is_int = ast::as<ast::IntType>(arr->element_type);
            if (len) {
                auto imm = immediate(m, *len);
                if (!imm) {
                    return imm.error();
                }
                if (elem_is_int) {
                    m.op(AbstractOp::ENCODE_INT_VECTOR, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*imm);
                        c.endian(Endian(elem_is_int->endian));
                        c.bit_size(*varint(*elem_is_int->bit_size));
                    });
                    return none;
                }
                return counter_loop(m, *imm, [&](Varint i) {
                    auto index = m.new_id(nullptr);
                    if (!index) {
                        return error("Failed to generate new id");
                    }
                    m.op(AbstractOp::INDEX, [&](Code& c) {
                        c.ident(*index);
                        c.left_ref(base_ref);
                        c.right_ref(i);
                    });
                    return encode_type(m, arr->element_type, *index, mapped_type);
                });
            }
            expected<Varint> len_;
            if (ast::is_any_range(arr->length)) {
                len_ = m.new_node_id(arr->length);
                if (!len_) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                    c.ident(*len_);
                    c.ref(base_ref);
                });
            }
            else {
                auto len_init = get_expr(m, arr->length);
                if (!len_init) {
                    return len_init.error();
                }
                len_ = define_tmp_var(m, *len_init, ast::ConstantLevel::immutable_variable);
                if (!len_) {
                    return len_.error();
                }
            }
            if (elem_is_int) {
                m.op(AbstractOp::ENCODE_INT_VECTOR, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*len_);
                    c.endian(Endian(elem_is_int->endian));
                    c.bit_size(*varint(*elem_is_int->bit_size));
                });
                return none;
            }
            return counter_loop(m, *len_, [&](Varint counter) {
                auto index = m.new_id(nullptr);
                if (!index) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INDEX, [&](Code& c) {
                    c.ident(*index);
                    c.left_ref(base_ref);
                    c.right_ref(counter);
                });
                auto err = encode_type(m, arr->element_type, *index, mapped_type);
                if (err) {
                    return err;
                }
                return none;
            });
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Format>(member)) {  // only format can encode
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::CALL_ENCODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                });
                return none;
            }
            return error("unknown struct type: {}", node_type_to_string(member->node_type));
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (mapped_type) {
                base_type = mapped_type;
                mapped_type = nullptr;
            }
            if (!base_type) {
                return error("abstract enum {} in encode", base_enum->ident->ident);
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            auto casted = m.new_node_id(typ);
            if (!casted) {
                return casted.error();
            }
            Storages to;
            auto err = define_storage(m, to, base_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::STATIC_CAST, [&](Code& c) {
                c.ident(*casted);
                c.storage(std::move(to));
                c.ref(base_ref);
            });
            err = encode_type(m, base_type, *casted, mapped_type);
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
            return encode_type(m, base_type, base_ref, mapped_type);
        }
        return error("unsupported type on encode type: {}", node_type_to_string(typ->node_type));
    }

    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(base_ref);
                c.endian(Endian(int_ty->endian));
                c.bit_size(*varint(*int_ty->bit_size));
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            auto new_id = m.new_node_id(typ);
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
            auto next_id = m.new_id(nullptr);
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
            auto str_ref = m.lookup_string(str_ty->strong_ref->value, &str_ty->strong_ref->loc);
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
                auto tmp = m.new_id(nullptr);
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
                auto index = m.new_id(nullptr);
                if (!index) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INDEX, [&](Code& c) {
                    c.ident(*index);
                    c.left_ref(*str_ref);
                    c.right_ref(counter);
                });
                auto cmp = m.new_id(nullptr);
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
            auto elem_is_int = ast::as<ast::IntType>(arr->element_type);
            auto len = arr->length_value;
            if (len) {
                auto imm = immediate(m, *len);
                if (!imm) {
                    return imm.error();
                }
                if (elem_is_int) {
                    m.op(AbstractOp::DECODE_INT_VECTOR_FIXED, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*imm);
                        c.endian(Endian(elem_is_int->endian));
                        c.bit_size(*varint(*elem_is_int->bit_size));
                    });
                    return none;
                }
                return counter_loop(m, *imm, [&](Varint i) {
                    auto index = m.new_id(nullptr);
                    if (!index) {
                        return error("Failed to generate new id");
                    }
                    m.op(AbstractOp::INDEX, [&](Code& c) {
                        c.ident(*index);
                        c.left_ref(base_ref);
                        c.right_ref(i);
                    });
                    return decode_type(m, arr->element_type, *index, mapped_type, field);
                });
            }
            auto undelying_decoder = [&] {
                auto new_obj = m.new_node_id(arr->element_type);
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
                err = decode_type(m, arr->element_type, *tmp_var, mapped_type, field);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::APPEND, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*tmp_var);
                });
                return none;
            };
            if (ast::is_any_range(arr->length)) {
                if (field) {
                    if (field->eventual_follow == ast::Follow::end) {
                        if (elem_is_int) {
                            m.op(AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF, [&](Code& c) {
                                c.ref(base_ref);
                                c.endian(Endian(elem_is_int->endian));
                                c.bit_size(*varint(*elem_is_int->bit_size));
                            });
                            return none;
                        }
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return error("Failed to generate new id");
                        }
                        m.op(AbstractOp::CAN_READ, [&](Code& c) {
                            c.ident(*new_id);
                        });
                        return conditional_loop(m, *new_id, undelying_decoder);
                    }
                    else if (field->eventual_follow == ast::Follow::fixed) {
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return new_id.error();
                        }
                        auto tail = field->belong_struct.lock()->fixed_tail_size / 8;
                        auto imm = immediate(m, tail);
                        if (!imm) {
                            return imm.error();
                        }
                        auto next_id = m.new_id(nullptr);
                        if (!next_id) {
                            return next_id.error();
                        }
                        m.op(AbstractOp::REMAIN_BYTES, [&](Code& c) {
                            c.ident(*next_id);
                        });
                        if (elem_is_int) {
                            // remain_bytes = REMAIN_BYTES - tail
                            // assert remain_bytes % elem_size == 0
                            // decode_int_vector(read_bytes / elem_size)
                            m.op(AbstractOp::BINARY, [&](Code& c) {
                                c.ident(*new_id);
                                c.bop(BinaryOp::sub);
                                c.left_ref(*next_id);
                                c.right_ref(*imm);
                            });
                            auto assert_expr = m.new_id(nullptr);
                            if (!assert_expr) {
                                return assert_expr.error();
                            }
                            m.op(AbstractOp::BINARY, [&](Code& c) {
                                c.ident(*assert_expr);
                                c.bop(BinaryOp::mod);
                                c.left_ref(*new_id);
                                c.right_ref(*varint(*elem_is_int->bit_size / futils::bit_per_byte));
                            });
                            m.op(AbstractOp::ASSERT, [&](Code& c) {
                                c.ref(*assert_expr);
                            });
                            m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                                c.left_ref(base_ref);
                                c.right_ref(*new_id);
                                c.endian(Endian(elem_is_int->endian));
                                c.bit_size(*varint(*elem_is_int->bit_size));
                            });
                            return none;
                        }
                        m.op(AbstractOp::BINARY, [&](Code& c) {
                            c.ident(*new_id);
                            c.bop(BinaryOp::grater);
                            c.left_ref(*next_id);
                            c.right_ref(*imm);
                        });
                        return conditional_loop(m, *new_id, undelying_decoder);
                    }
                    else if (field->eventual_follow == ast::Follow::constant) {
                        auto next = field->next.lock();
                        if (!next) {
                            return error("Invalid next field");
                        }
                        auto str = ast::cast_to<ast::StrLiteralType>(next->field_type);
                        auto str_ref = m.lookup_string(str->strong_ref->value, &str->strong_ref->loc);
                        if (!str_ref) {
                            return str_ref.error();
                        }
                        m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                            c.ident(*str_ref);
                        });
                        auto imm = immediate(m, *str->bit_size / futils::bit_per_byte);
                        if (!imm) {
                            return imm.error();
                        }
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return new_id.error();
                        }
                        Storages s;
                        s.length.value(2);
                        s.storages.push_back(Storage{.type = StorageType::ARRAY});
                        s.storages.back().size(*varint(*str->bit_size / futils::bit_per_byte));
                        s.storages.push_back(Storage{.type = StorageType::UINT});
                        s.storages.back().size(*varint(8));
                        m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                            c.ident(*new_id);
                            c.storage(std::move(s));
                        });
                        auto temporary_read_holder = define_tmp_var(m, *new_id, ast::ConstantLevel::variable);
                        if (!temporary_read_holder) {
                            return temporary_read_holder.error();
                        }
                        m.op(AbstractOp::LOOP_INFINITE);
                        {
                            m.op(AbstractOp::PEEK_INT_VECTOR, [&](Code& c) {
                                c.left_ref(*temporary_read_holder);
                                c.right_ref(*imm);
                                c.endian(Endian::unspec);
                                c.bit_size(*varint(8));
                            });
                            Storages isOkFlag;
                            isOkFlag.length.value(1);
                            isOkFlag.storages.push_back(Storage{.type = StorageType::BOOL});
                            auto flagObj = m.new_id(nullptr);
                            if (!flagObj) {
                                return error("Failed to generate new id");
                            }
                            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                                c.ident(*flagObj);
                                c.storage(std::move(isOkFlag));
                            });
                            auto isOK = define_tmp_var(m, *flagObj, ast::ConstantLevel::variable);
                            auto immTrue = immediate_bool(m, true);
                            if (!immTrue) {
                                return immTrue.error();
                            }
                            auto immFalse = immediate_bool(m, false);
                            if (!immFalse) {
                                return immFalse.error();
                            }
                            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                                c.left_ref(*isOK);
                                c.right_ref(*immTrue);
                            });
                            auto err = counter_loop(m, *imm, [&](Varint i) {
                                auto index_str = m.new_id(nullptr);
                                if (!index_str) {
                                    return error("Failed to generate new id");
                                }
                                m.op(AbstractOp::INDEX, [&](Code& c) {
                                    c.ident(*index_str);
                                    c.left_ref(*str_ref);
                                    c.right_ref(i);
                                });
                                auto index_peek = m.new_id(nullptr);
                                if (!index_peek) {
                                    return error("Failed to generate new id");
                                }
                                m.op(AbstractOp::INDEX, [&](Code& c) {
                                    c.ident(*index_peek);
                                    c.left_ref(*temporary_read_holder);
                                    c.right_ref(i);
                                });
                                auto cmp = m.new_id(nullptr);
                                if (!cmp) {
                                    return error("Failed to generate new id");
                                }
                                m.op(AbstractOp::BINARY, [&](Code& c) {
                                    c.ident(*cmp);
                                    c.bop(BinaryOp::not_equal);
                                    c.left_ref(*index_str);
                                    c.right_ref(*index_peek);
                                });
                                m.op(AbstractOp::IF, [&](Code& c) {
                                    c.ref(*cmp);
                                });
                                m.op(AbstractOp::ASSIGN, [&](Code& c) {
                                    c.left_ref(*isOK);
                                    c.right_ref(*immFalse);
                                });
                                m.op(AbstractOp::BREAK);
                                m.op(AbstractOp::END_IF);
                                return none;
                            });
                            if (err) {
                                return err;
                            }
                            m.op(AbstractOp::IF, [&](Code& c) {
                                c.ref(*isOK);
                            });
                            m.op(AbstractOp::BREAK);
                            m.op(AbstractOp::END_IF);
                            err = undelying_decoder();
                            if (err) {
                                return err;
                            }
                        }
                        m.op(AbstractOp::END_LOOP);
                        return none;
                    }
                    else {
                        return error("Invalid follow type");
                    }
                }
                else {
                    return error("Invalid field");
                }
            }
            else {
                auto id = get_expr(m, arr->length);
                if (!id) {
                    return id.error();
                }
                auto len_ident = define_tmp_var(m, *id, ast::ConstantLevel::immutable_variable);
                if (!len_ident) {
                    return len_ident.error();
                }
                if (elem_is_int) {
                    m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*len_ident);
                        c.endian(Endian(elem_is_int->endian));
                        c.bit_size(*varint(*elem_is_int->bit_size));
                    });
                    return none;
                }
                m.op(AbstractOp::RESERVE_SIZE, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*len_ident);
                });
                return counter_loop(m, *len_ident, [&](Varint) {
                    return undelying_decoder();
                });
            }
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Format>(member)) {  // only format can decode
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::CALL_DECODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                });
                return none;
            }
            return error("unknown struct type: {}", node_type_to_string(member->node_type));
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (mapped_type) {
                base_type = mapped_type;
                mapped_type = nullptr;
            }
            if (!base_type) {
                return error("abstract enum {} in decode", base_enum->ident->ident);
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            auto storage = m.new_id(nullptr);
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
            err = decode_type(m, base_type, *tmp_var, mapped_type, field);
            if (err) {
                return err;
            }
            auto casted = m.new_id(nullptr);
            if (!casted) {
                return casted.error();
            }
            Storages to;
            err = define_storage(m, to, ast::cast_to<ast::EnumType>(typ));
            if (err) {
                return err;
            }
            m.op(AbstractOp::STATIC_CAST, [&](Code& c) {
                c.ident(*casted);
                c.storage(std::move(to));
                c.ref(*tmp_var);
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
            return decode_type(m, base_type, base_ref, mapped_type, field);
        }
        return error("unsupported type on decode type: {}", node_type_to_string(typ->node_type));
    }

    template <>
    Error encode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        if (node->arguments && node->arguments->sub_byte_length) {
            auto len = get_expr(m, node->arguments->sub_byte_length);
            if (!len) {
                return len.error();
            }
            m.op(AbstractOp::BEGIN_ENCODE_SUB_RANGE, [&](Code& c) {
                c.ref(*len);
            });
        }
        Error err;
        if (node->arguments && node->arguments->type_map) {
            err = encode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal);
        }
        else {
            err = encode_type(m, node->field_type, *ident, nullptr);
        }
        if (err) {
            return err;
        }
        if (node->arguments && node->arguments->sub_byte_length) {
            m.op(AbstractOp::END_ENCODE_SUB_RANGE);
        }
        return none;
    }

    template <>
    Error decode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        if (node->arguments && node->arguments->sub_byte_length) {
            auto len = get_expr(m, node->arguments->sub_byte_length);
            if (!len) {
                return len.error();
            }
            m.op(AbstractOp::BEGIN_DECODE_SUB_RANGE, [&](Code& c) {
                c.ref(*len);
            });
        }
        Error err;
        if (node->arguments && node->arguments->type_map) {
            err = decode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal, node.get());
        }
        else {
            err = decode_type(m, node->field_type, *ident, nullptr, node.get());
        }
        if (node->arguments && node->arguments->sub_byte_length) {
            m.op(AbstractOp::END_DECODE_SUB_RANGE);
        }
        return err;
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
        auto temporary_ident = std::make_shared<ast::Ident>(node->loc, "encode");
        temporary_ident->base = node;  // for lookup
        auto new_id = m.lookup_ident(temporary_ident);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(*fmt_ident);
        });
        m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
            c.storage(Storages{
                .length = varint(1).value(),
                .storages = {
                    Storage{.type = StorageType::CODER_RETURN},
                },
            });
        });
        m.on_encode_fn = true;
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            if (m.bit_field_begin.contains(n)) {
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::DEFINE_PACKED_OPERATION, [&](Code& c) {
                    c.ident(*new_id);
                });
            }
            auto err = convert_node_encode(m, n);
            if (m.bit_field_end.contains(n)) {
                m.op(AbstractOp::END_PACKED_OPERATION);
            }
            return err;
        });
        if (err) {
            return err;
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
        auto temporary_ident = std::make_shared<ast::Ident>(node->loc, "decode");
        temporary_ident->base = node;  // for lookup
        auto new_id = m.lookup_ident(temporary_ident);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(fmt_ident.value());
        });
        m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
            c.storage(Storages{
                .length = varint(1).value(),
                .storages = {
                    Storage{.type = StorageType::CODER_RETURN},
                },
            });
        });
        m.on_encode_fn = false;
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            return convert_node_decode(m, n);
        });
        if (err) {
            return err;
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
        return foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        return foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_decode(m, n);
        });
    }

    void add_switch_union(Module& m, std::shared_ptr<ast::StructType>& node) {
        auto ident = m.lookup_struct(node);
        if (ident.value() == 0) {
            return;
        }
        bool has_field = false;
        for (auto& field : node->fields) {
            if (ast::as<ast::Field>(field)) {
                has_field = true;
                break;
            }
        }
        if (!has_field) {
            return;  // omit check for empty struct
        }
        if (m.on_encode_fn) {
            m.op(AbstractOp::CHECK_UNION, [&](Code& c) {
                c.ref(ident);
            });
        }
        else {
            m.op(AbstractOp::SWITCH_UNION, [&](Code& c) {
                c.ref(ident);
            });
        }
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
            else if (std::is_base_of_v<ast::Expr, T>) {
                err = convert_node_definition(m, node);
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
            else if (std::is_base_of_v<ast::Expr, T>) {
                err = convert_node_definition(m, node);
            }
        });
        return err;
    }
}  // namespace rebgn