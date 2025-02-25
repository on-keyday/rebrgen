/*license*/

#include "internal.hpp"
#include <core/ast/tool/eval.h>

namespace rebgn {
    Varint get_field_ref(Module& m, ast::Field* field) {
        if (!field) {
            return *varint(null_id);
        }
        return *m.lookup_ident(field->ident);
    }

    expected<Varint> get_alignment_requirement(Module& m, ast::ArrayType* arr, ast::Field* field, bool on_encode) {
        auto ident = m.new_node_id(arr->length);
        if (!ident) {
            return ident;
        }
        m.op(on_encode ? AbstractOp::OUTPUT_BYTE_OFFSET : AbstractOp::INPUT_BYTE_OFFSET, [&](Code& c) {
            c.ident(*ident);
        });
        auto imm_alignment = immediate(m, *field->arguments->alignment_value / 8);
        if (!imm_alignment) {
            return imm_alignment;
        }
        auto mod = m.new_id(nullptr);
        if (!mod) {
            return unexpect_error("Failed to generate new id");
        }
        // current size % alignment
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*mod);
            c.bop(BinaryOp::mod);
            c.left_ref(*ident);
            c.right_ref(*imm_alignment);
        });
        auto cmp = m.new_id(nullptr);
        if (!cmp) {
            return unexpect_error("Failed to generate new id");
        }
        auto imm_zero = immediate(m, 0);
        if (!imm_zero) {
            return imm_zero;
        }
        // alignment - (size % alignment)
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*cmp);
            c.bop(BinaryOp::sub);
            c.left_ref(*imm_alignment);
            c.right_ref(*mod);
        });
        auto req_size = m.new_id(nullptr);
        if (!req_size) {
            return unexpect_error("Failed to generate new id");
        }
        // (alignment - (size % alignment)) % alignment
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*req_size);
            c.bop(BinaryOp::mod);
            c.left_ref(*cmp);
            c.right_ref(*imm_alignment);
        });
        return define_int_tmp_var(m, *req_size, ast::ConstantLevel::variable);
    }

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            auto bit_size = varint(*int_ty->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            auto endian = m.get_endian(Endian(int_ty->endian), int_ty->is_signed);
            if (!endian) {
                return endian.error();
            }
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(base_ref);
                c.endian(*endian);
                c.bit_size(*bit_size);
                c.belong(get_field_ref(m, field));
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            auto from = define_storage(m, typ);
            if (!from) {
                return from.error();
            }
            auto to = define_storage(m, std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false));
            if (!to) {
                return to.error();
            }
            auto new_id = m.new_node_id(typ);
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::CAST, [&](Code& c) {
                c.ident(*new_id);
                c.ref(base_ref);
                c.type(*to);
                c.from_type(*from);
                c.cast_type(CastType::FLOAT_TO_INT_BIT);
            });
            auto bit_size = varint(*float_ty->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            auto endian = m.get_endian(Endian(float_ty->endian), false);
            if (!endian) {
                return endian.error();
            }
            m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                c.ref(*new_id);
                c.endian(*endian);
                c.bit_size(*bit_size);
                c.belong(get_field_ref(m, field));
            });
            return none;
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = static_str(m, str_ty->strong_ref);
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
                auto endian = m.get_endian(Endian::unspec, false);
                if (!endian) {
                    return endian.error();
                }
                m.op(AbstractOp::ENCODE_INT, [&](Code& c) {
                    c.ref(*index);
                    c.endian(*endian);
                    c.bit_size(*varint(8));
                    c.belong(get_field_ref(m, field));
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
                    auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                    if (!endian) {
                        return endian.error();
                    }
                    m.op(AbstractOp::ENCODE_INT_VECTOR_FIXED, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*imm);
                        c.endian(*endian);
                        c.bit_size(*varint(*elem_is_int->bit_size));
                        c.belong(get_field_ref(m, field));
                        c.array_length(*varint(*len));
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
                    return encode_type(m, arr->element_type, *index, mapped_type, field, should_init_recursive);
                });
            }
            if (!arr->length) {
                return error("Array length is not specified");
            }
            expected<Varint> len_;
            len_ = m.new_node_id(arr->length);
            if (!len_) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                c.ident(*len_);
                c.ref(base_ref);
            });
            if (ast::is_any_range(arr->length)) {
                if (is_alignment_vector(field)) {
                    auto req_size = get_alignment_requirement(m, arr, field, true);
                    if (!req_size) {
                        return req_size.error();
                    }
                    m.op(AbstractOp::ENCODE_INT_VECTOR_FIXED, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*req_size);
                        c.endian(*m.get_endian(Endian::unspec, false));
                        c.bit_size(*varint(8));
                        c.belong(get_field_ref(m, field));
                        c.array_length(*varint(*field->arguments->alignment_value / 8 - 1));
                    });
                    return none;
                }
            }
            else {
                auto len_init = get_expr(m, arr->length);
                if (!len_init) {
                    return len_init.error();
                }
                auto expr_type = arr->length->expr_type;
                if (auto u = ast::as<ast::UnionType>(expr_type)) {
                    if (!u->common_type) {
                        return error("Union type must have common type");
                    }
                    expr_type = u->common_type;
                }
                auto s = define_storage(m, expr_type);
                if (!s) {
                    return s.error();
                }
                auto expected_len = define_typed_tmp_var(m, *len_init, *s, ast::ConstantLevel::immutable_variable);
                if (!expected_len) {
                    return expected_len.error();
                }
                // add length check
                m.op(AbstractOp::LENGTH_CHECK, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*expected_len);
                    c.belong(get_field_ref(m, field));
                });
            }
            if (elem_is_int) {
                auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                if (!endian) {
                    return endian.error();
                }
                m.op(AbstractOp::ENCODE_INT_VECTOR, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*len_);
                    c.endian(*endian);
                    c.bit_size(*varint(*elem_is_int->bit_size));
                    c.belong(get_field_ref(m, field));
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
                auto err = encode_type(m, arr->element_type, *index, mapped_type, field, false);
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
                Varint bit_size_plus_1;
                if (me->body->struct_type->bit_size) {
                    auto s = varint(*me->body->struct_type->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                if (should_init_recursive && me->body->struct_type->recursive) {
                    m.op(AbstractOp::CHECK_RECURSIVE_STRUCT, [&](Code& c) {
                        c.left_ref(*ident);
                        c.right_ref(base_ref);
                    });
                }
                m.op(AbstractOp::CALL_ENCODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                    c.bit_size_plus(bit_size_plus_1);
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
            auto to = define_storage(m, base_type);
            if (!to) {
                return to.error();
            }
            auto from = define_storage(m, typ);
            if (!from) {
                return from.error();
            }
            m.op(AbstractOp::CAST, [&](Code& c) {
                c.ident(*casted);
                c.type(*to);
                c.from_type(*from);
                c.ref(base_ref);
                c.cast_type(CastType::ENUM_TO_INT);
            });
            auto err = encode_type(m, base_type, *casted, mapped_type, field, should_init_recursive);
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
            return encode_type(m, base_type, base_ref, mapped_type, field, should_init_recursive);
        }
        return error("unsupported type on encode type: {}", node_type_to_string(typ->node_type));
    }

    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            auto endian = m.get_endian(Endian(int_ty->endian), int_ty->is_signed);
            if (!endian) {
                return endian.error();
            }
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(base_ref);
                c.endian(*endian);
                c.bit_size(*varint(*int_ty->bit_size));
                c.belong(get_field_ref(m, field));
            });
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            auto new_id = m.new_node_id(typ);
            if (!new_id) {
                return error("Failed to generate new id");
            }
            auto from = define_storage(m, std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false));
            if (!from) {
                return from.error();
            }
            auto to = define_storage(m, typ);
            if (!to) {
                return to.error();
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.type(*from);
            });
            auto endian = m.get_endian(Endian(float_ty->endian), false);
            if (!endian) {
                return endian.error();
            }
            m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                c.ref(*new_id);
                c.endian(*endian);
                c.bit_size(*varint(*float_ty->bit_size));
                c.belong(get_field_ref(m, field));
            });
            auto next_id = m.new_id(nullptr);
            if (!next_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::CAST, [&](Code& c) {
                c.ident(*next_id);
                c.type(*to);
                c.from_type(*from);
                c.ref(*new_id);
                c.cast_type(CastType::INT_TO_FLOAT_BIT);
            });
            return do_assign(m, nullptr, nullptr, base_ref, *next_id);
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = static_str(m, str_ty->strong_ref);
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            return counter_loop(m, *max_len, [&](Varint counter) {
                auto tmp = m.new_id(nullptr);
                if (!tmp) {
                    return error("Failed to generate new id");
                }
                auto int_typ = std::make_shared<ast::IntType>(str_ty->loc, 8, ast::Endian::unspec, false);
                auto s = define_storage(m, int_typ);
                if (!s) {
                    return s.error();
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*tmp);
                    c.type(*s);
                });
                auto tmp_var = define_typed_tmp_var(m, *tmp, *s, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                auto endian = m.get_endian(Endian::unspec, false);
                if (!endian) {
                    return endian.error();
                }
                m.op(AbstractOp::DECODE_INT, [&](Code& c) {
                    c.ref(*tmp_var);
                    c.endian(*endian);
                    c.bit_size(*varint(8));
                    c.belong(get_field_ref(m, field));
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
                    c.belong(m.get_function());
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
                    auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                    if (!endian) {
                        return endian.error();
                    }
                    m.op(AbstractOp::DECODE_INT_VECTOR_FIXED, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*imm);
                        c.endian(*endian);
                        c.bit_size(*varint(*elem_is_int->bit_size));
                        c.belong(get_field_ref(m, field));
                        c.array_length(*varint(*len));
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
                    return decode_type(m, arr->element_type, *index, mapped_type, field, should_init_recursive);
                });
            }
            auto undelying_decoder = [&] {
                auto new_obj = m.new_node_id(arr->element_type);
                if (!new_obj) {
                    return new_obj.error();
                }
                auto s = define_storage(m, arr->element_type);
                if (!s) {
                    return s.error();
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*new_obj);
                    c.type(*s);
                });
                auto tmp_var = define_typed_tmp_var(m, *new_obj, *s, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                auto err = decode_type(m, arr->element_type, *tmp_var, mapped_type, field, false);
                if (err) {
                    return err;
                }
                auto prev_ = m.prev_assign(tmp_var->value());
                m.op(AbstractOp::APPEND, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*varint(prev_));
                });
                return none;
            };
            if (ast::is_any_range(arr->length)) {
                if (field) {
                    if (is_alignment_vector(field)) {
                        auto req_size = get_alignment_requirement(m, arr, field, false);
                        if (!req_size) {
                            return req_size.error();
                        }
                        m.op(AbstractOp::DECODE_INT_VECTOR_FIXED, [&](Code& c) {
                            c.left_ref(base_ref);
                            c.right_ref(*req_size);
                            c.endian(*m.get_endian(Endian::unspec, false));
                            c.bit_size(*varint(8));
                            c.belong(get_field_ref(m, field));
                            c.array_length(*varint(*field->arguments->alignment_value / 8 - 1));
                        });
                        return none;
                    }
                    else if (field->follow == ast::Follow::end ||
                             (field->arguments && field->arguments->sub_byte_length)  // this means that the field is a sub-byte field
                    ) {
                        if (elem_is_int) {
                            auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                            if (!endian) {
                                return endian.error();
                            }
                            m.op(AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF, [&](Code& c) {
                                c.ref(base_ref);
                                c.endian(*endian);
                                c.bit_size(*varint(*elem_is_int->bit_size));
                                c.belong(get_field_ref(m, field));
                            });
                            return none;
                        }
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return error("Failed to generate new id");
                        }
                        m.op(AbstractOp::CAN_READ, [&](Code& c) {
                            c.ident(*new_id);
                            c.belong(get_field_ref(m, field));
                        });
                        return conditional_loop(m, *new_id, undelying_decoder);
                    }
                    else if (field->follow == ast::Follow::fixed) {
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
                                c.belong(m.get_function());
                            });
                            auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                            if (!endian) {
                                return endian.error();
                            }
                            m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                                c.left_ref(base_ref);
                                c.right_ref(*new_id);
                                c.endian(*endian);
                                c.bit_size(*varint(*elem_is_int->bit_size));
                                c.belong(get_field_ref(m, field));
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
                    else if (field->follow == ast::Follow::constant) {
                        auto next = field->next.lock();
                        if (!next) {
                            return error("Invalid next field");
                        }
                        auto str = ast::cast_to<ast::StrLiteralType>(next->field_type);
                        auto str_ref = static_str(m, str->strong_ref);
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
                        auto ref = m.get_storage_ref(s, &next->loc);
                        if (!ref) {
                            return ref.error();
                        }
                        m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                            c.ident(*new_id);
                            c.type(*ref);
                        });
                        auto temporary_read_holder = define_typed_tmp_var(m, *new_id, *ref, ast::ConstantLevel::variable);
                        if (!temporary_read_holder) {
                            return temporary_read_holder.error();
                        }
                        m.op(AbstractOp::LOOP_INFINITE);
                        {
                            auto endian = m.get_endian(Endian::unspec, false);
                            if (!endian) {
                                return endian.error();
                            }
                            m.op(AbstractOp::PEEK_INT_VECTOR, [&](Code& c) {
                                c.left_ref(*temporary_read_holder);
                                c.right_ref(*imm);
                                c.endian(*endian);
                                c.bit_size(*varint(8));
                                c.belong(get_field_ref(m, field));
                            });
                            Storages isOkFlag;
                            isOkFlag.length.value(1);
                            isOkFlag.storages.push_back(Storage{.type = StorageType::BOOL});
                            auto gen = m.get_storage_ref(isOkFlag, &next->loc);
                            if (!gen) {
                                return gen.error();
                            }
                            auto flagObj = m.new_id(nullptr);
                            if (!flagObj) {
                                return error("Failed to generate new id");
                            }
                            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                                c.ident(*flagObj);
                                c.type(*gen);
                            });
                            auto isOK = define_bool_tmp_var(m, *flagObj, ast::ConstantLevel::variable);
                            auto immTrue = immediate_bool(m, true);
                            if (!immTrue) {
                                return immTrue.error();
                            }
                            auto immFalse = immediate_bool(m, false);
                            if (!immFalse) {
                                return immFalse.error();
                            }
                            auto err = do_assign(m, nullptr, nullptr, *isOK, *immTrue);
                            if (err) {
                                return err;
                            }
                            err = counter_loop(m, *imm, [&](Varint i) {
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
                                m.init_phi_stack(cmp->value());
                                m.op(AbstractOp::IF, [&](Code& c) {
                                    c.ref(*cmp);
                                });
                                err = do_assign(m, nullptr, nullptr, *isOK, *immFalse);
                                m.op(AbstractOp::BREAK);
                                m.op(AbstractOp::END_IF);
                                return insert_phi(m, m.end_phi_stack());
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
                auto expr_type = arr->length->expr_type;
                if (auto u = ast::as<ast::UnionType>(expr_type)) {
                    if (!u->common_type) {
                        return error("Union type must have common type");
                    }
                    expr_type = u->common_type;
                }
                auto s = define_storage(m, expr_type);
                if (!s) {
                    return s.error();
                }
                auto len_ident = define_typed_tmp_var(m, *id, *s, ast::ConstantLevel::immutable_variable);
                if (!len_ident) {
                    return len_ident.error();
                }
                if (elem_is_int) {
                    auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                    if (!endian) {
                        return endian.error();
                    }
                    m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*len_ident);
                        c.endian(*endian);
                        c.bit_size(*varint(*elem_is_int->bit_size));
                        c.belong(get_field_ref(m, field));
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
                Varint bit_size_plus_1;
                if (me->body->struct_type->bit_size) {
                    auto s = varint(*me->body->struct_type->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                if (should_init_recursive && me->body->struct_type->recursive) {
                    m.op(AbstractOp::INIT_RECURSIVE_STRUCT, [&](Code& c) {
                        c.left_ref(*ident);
                        c.right_ref(base_ref);
                    });
                }
                m.op(AbstractOp::CALL_DECODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                    c.bit_size_plus(bit_size_plus_1);
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
            auto s = define_storage(m, base_type);
            if (!s) {
                return s.error();
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*storage);
                c.type(*s);
            });
            auto tmp_var = define_typed_tmp_var(m, *storage, *s, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            auto err = decode_type(m, base_type, *tmp_var, mapped_type, field, should_init_recursive);
            if (err) {
                return err;
            }
            auto casted = m.new_id(nullptr);
            if (!casted) {
                return casted.error();
            }

            auto to = define_storage(m, ast::cast_to<ast::EnumType>(typ));
            if (!to) {
                return err;
            }
            m.op(AbstractOp::CAST, [&](Code& c) {
                c.ident(*casted);
                c.type(*to);
                c.from_type(*s);
                c.ref(*tmp_var);
                c.cast_type(CastType::INT_TO_ENUM);
            });
            return do_assign(m, nullptr, nullptr, base_ref, *casted);
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return decode_type(m, base_type, base_ref, mapped_type, field, should_init_recursive);
        }
        return error("unsupported type on decode type: {}", node_type_to_string(typ->node_type));
    }

    Error do_field_argument_assert(Module& m, Varint ident, std::shared_ptr<ast::Field>& node) {
        if (node->arguments && node->arguments->arguments.size()) {
            if (size_t(node->arguments->argument_mapping) & size_t(ast::FieldArgumentMapping::direct)) {
                std::optional<Varint> prev;
                for (auto& arg : node->arguments->arguments) {
                    auto val = get_expr(m, arg);
                    if (!val) {
                        return val.error();
                    }
                    auto new_id = m.new_id(nullptr);
                    if (!new_id) {
                        return error("Failed to generate new id");
                    }
                    m.op(AbstractOp::BINARY, [&](Code& c) {
                        c.ident(*new_id);
                        c.bop(BinaryOp::equal);
                        c.left_ref(ident);
                        c.right_ref(*val);
                    });
                    if (prev) {
                        auto or_id = m.new_id(nullptr);
                        if (!or_id) {
                            return error("Failed to generate new id");
                        }
                        m.op(AbstractOp::BINARY, [&](Code& c) {
                            c.ident(*or_id);
                            c.bop(BinaryOp::logical_or);
                            c.left_ref(*new_id);
                            c.right_ref(*prev);
                        });
                        prev = *or_id;
                    }
                    else {
                        prev = *new_id;
                    }
                }
                m.op(AbstractOp::ASSERT, [&](Code& c) {
                    c.ref(*prev);
                    c.belong(m.get_function());
                });
            }
        }
        return none;
    }

    template <>
    Error encode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        if (node->is_state_variable) {
            return none;
        }
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        auto err = do_field_argument_assert(m, *ident, node);
        if (err) {
            return err;
        }
        std::optional<Varint> seek_pos_holder;
        if (node->arguments && node->arguments->sub_byte_length) {
            if (node->arguments->sub_byte_begin) {
                auto offset = get_expr(m, node->arguments->sub_byte_begin);
                if (!offset) {
                    return offset.error();
                }
                auto save_current_pos = m.new_id(nullptr);
                if (!save_current_pos) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::OUTPUT_BYTE_OFFSET, [&](Code& c) {
                    c.ident(*save_current_pos);
                });
                auto tmpvar = define_int_tmp_var(m, *save_current_pos, ast::ConstantLevel::immutable_variable);
                if (!tmpvar) {
                    return tmpvar.error();
                }
                seek_pos_holder = *tmpvar;
                m.op(AbstractOp::SEEK_ENCODER, [&](Code& c) {
                    c.ref(*offset);
                    c.belong(*ident);
                });
            }
            if (ast::is_any_range(node->arguments->sub_byte_length)) {
                if (!node->arguments->sub_byte_begin) {
                    return error("until eof subrange is not supported without begin");
                }
            }
            else {
                auto len = get_expr(m, node->arguments->sub_byte_length);
                if (!len) {
                    return len.error();
                }
                m.op(AbstractOp::BEGIN_ENCODE_SUB_RANGE, [&](Code& c) {
                    c.sub_range_type(SubRangeType::byte_len);
                    c.ref(*len);
                    c.belong(*ident);
                });
            }
        }
        if (node->arguments && node->arguments->type_map) {
            err = encode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal, node.get(), true);
        }
        else {
            err = encode_type(m, node->field_type, *ident, nullptr, node.get(), true);
        }
        if (err) {
            return err;
        }
        if (node->arguments && node->arguments->sub_byte_length &&
            !ast::is_any_range(node->arguments->sub_byte_length)) {
            m.op(AbstractOp::END_ENCODE_SUB_RANGE);
        }
        if (seek_pos_holder) {
            m.op(AbstractOp::SEEK_ENCODER, [&](Code& c) {
                c.ref(*seek_pos_holder);
                c.belong(*ident);
            });
        }
        return none;
    }

    template <>
    Error decode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        if (node->is_state_variable) {
            return none;
        }
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        std::optional<Varint> seek_pos_holder;
        if (node->arguments && node->arguments->sub_byte_length) {
            if (node->arguments->sub_byte_begin) {
                auto offset = get_expr(m, node->arguments->sub_byte_begin);
                if (!offset) {
                    return offset.error();
                }
                auto save_current_pos = m.new_id(nullptr);
                if (!save_current_pos) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INPUT_BYTE_OFFSET, [&](Code& c) {
                    c.ident(*save_current_pos);
                });
                auto tmpvar = define_int_tmp_var(m, *save_current_pos, ast::ConstantLevel::immutable_variable);
                if (!tmpvar) {
                    return tmpvar.error();
                }
                seek_pos_holder = *tmpvar;
                m.op(AbstractOp::SEEK_DECODER, [&](Code& c) {
                    c.ref(*offset);
                    c.belong(*ident);
                });
            }
            if (ast::is_any_range(node->arguments->sub_byte_length)) {
                if (!node->arguments->sub_byte_begin) {
                    return error("until eof subrange is not supported without begin");
                }
            }
            else {
                auto len = get_expr(m, node->arguments->sub_byte_length);
                if (!len) {
                    return len.error();
                }
                m.op(AbstractOp::BEGIN_DECODE_SUB_RANGE, [&](Code& c) {
                    c.sub_range_type(SubRangeType::byte_len);
                    c.ref(*len);
                    c.belong(*ident);
                });
            }
        }
        Error err;
        if (node->arguments && node->arguments->type_map) {
            err = decode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal, node.get(), true);
        }
        else {
            err = decode_type(m, node->field_type, *ident, nullptr, node.get(), true);
        }
        if (err) {
            return err;
        }
        if (node->arguments && node->arguments->sub_byte_length &&
            !ast::is_any_range(node->arguments->sub_byte_length)) {
            m.op(AbstractOp::END_DECODE_SUB_RANGE);
        }
        if (seek_pos_holder) {
            m.op(AbstractOp::SEEK_DECODER, [&](Code& c) {
                c.ref(*seek_pos_holder);
                c.belong(*ident);
            });
        }
        err = do_field_argument_assert(m, *ident, node);
        if (err) {
            return err;
        }
        return err;
    }

    Endian get_type_endian(const std::shared_ptr<ast::Type>& typ) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            return Endian(int_ty->endian);
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            return Endian(float_ty->endian);
        }
        if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
            return get_type_endian(enum_ty->base.lock()->base_type);
        }
        return Endian::unspec;
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
        auto typ = m.get_storage_ref(Storages{
                                         .length = varint(1).value(),
                                         .storages = {
                                             Storage{.type = StorageType::CODER_RETURN},
                                         },
                                     },
                                     &node->loc);
        if (!typ) {
            return typ.error();
        }
        m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
            c.type(*typ);
        });
        m.on_encode_fn = true;
        m.init_phi_stack(0);  // make it temporary
        auto f = m.enter_function(*new_id);
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            if (auto found = m.bit_field_begin.find(n);
                found != m.bit_field_begin.end()) {
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return error("Failed to generate new id");
                }
                auto typ = m.bit_field_variability[n];
                auto field = ast::as<ast::Field>(n);
                m.op(AbstractOp::BEGIN_ENCODE_PACKED_OPERATION, [&](Code& c) {
                    c.ident(*new_id);
                    c.belong(found->second);
                    c.packed_op_type(typ);
                    c.endian(*m.get_endian(get_type_endian(field ? field->field_type : nullptr), false));
                });
            }
            auto err = convert_node_encode(m, n);
            if (m.bit_field_end.contains(n)) {
                m.op(AbstractOp::END_ENCODE_PACKED_OPERATION);
            }
            return err;
        });
        if (err) {
            return err;
        }
        f.execute();
        m.op(AbstractOp::RET_SUCCESS, [&](Code& c) {
            c.belong(*new_id);
        });
        m.op(AbstractOp::END_FUNCTION);
        m.end_phi_stack();  // remove temporary
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
        auto typ = m.get_storage_ref(Storages{
                                         .length = varint(1).value(),
                                         .storages = {
                                             Storage{.type = StorageType::CODER_RETURN},
                                         },
                                     },
                                     &node->loc);
        if (!typ) {
            return typ.error();
        }
        m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
            c.type(*typ);
        });
        m.on_encode_fn = false;
        m.init_phi_stack(0);  // make it temporary
        auto f = m.enter_function(*new_id);
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            if (auto found = m.bit_field_begin.find(n);
                found != m.bit_field_begin.end()) {
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return error("Failed to generate new id");
                }
                auto typ = m.bit_field_variability[n];
                auto field = ast::as<ast::Field>(n);
                m.op(AbstractOp::BEGIN_DECODE_PACKED_OPERATION, [&](Code& c) {
                    c.ident(*new_id);
                    c.belong(found->second);
                    c.packed_op_type(typ);
                    c.endian(*m.get_endian(get_type_endian(field ? field->field_type : nullptr), false));
                });
            }
            auto err = convert_node_decode(m, n);
            if (m.bit_field_end.contains(n)) {
                m.op(AbstractOp::END_DECODE_PACKED_OPERATION);
            }
            return err;
        });
        if (err) {
            return err;
        }
        f.execute();
        m.op(AbstractOp::RET_SUCCESS, [&](Code& c) {
            c.belong(*new_id);
        });
        m.op(AbstractOp::END_FUNCTION);
        m.end_phi_stack();  // remove temporary
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
                c.check_at(UnionCheckAt::ENCODER);
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
    Error encode<ast::SpecifyOrder>(Module& m, std::shared_ptr<ast::SpecifyOrder>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::SpecifyOrder>(Module& m, std::shared_ptr<ast::SpecifyOrder>& node) {
        return convert_node_definition(m, node);
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
    Error encode<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
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
