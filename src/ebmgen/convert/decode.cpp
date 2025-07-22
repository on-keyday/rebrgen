/*license*/
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include "../converter.hpp"

namespace ebmgen {
    expected<ebm::StatementRef> DecoderConverter::decode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef to, ebm::TypeRef cast_to) {
        COMMON_BUFFER_SETUP(EBM_READ_DATA, read_ref);

        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, u8_t, buffer, zero);
            EBM_CAST(cast_ref, cast_to, value_type, array_index);
            EBM_ASSIGNMENT(assign, to, cast_ref);
            ebm::Block block;
            block.container.reserve(3);
            append(block, buffer_def);
            append(block, read_ref);
            append(block, assign);
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        }

        EBM_DEFINE_ANONYMOUS_VARIABLE(value_holder, value_type, zero);

        EBMU_COUNTER_TYPE(counter_type);

        EBM_COUNTER_LOOP_START(counter);

        EBMU_INT_LITERAL(eight, 8);
        EBMU_INT_LITERAL(xFF, 0xff);
        // value_holder = value_type(0)
        // buffer = [0] * n
        // read(buffer)
        // for counter in 0..len:
        //   if little endian
        //     shift_index = counter
        //   else
        //     // len - 1 can be constant
        //     shift_index = len - 1 - counter
        //   value_holder |= (value_type(buffer[counter]) << (8 * shift_index))
        // to = (may cast) value_holder
        auto do_assign = [&](ebm::ExpressionRef shift_index) -> expected<ebm::StatementRef> {
            EBM_BINARY_OP(shift, ebm::BinaryOp::mul, counter_type, shift_index, eight);
            EBM_INDEX(array_index, u8_t, buffer, counter);
            EBM_CAST(casted, value_type, u8_t, array_index);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, value_type, casted, shift);
            EBM_BINARY_OP(masked, ebm::BinaryOp::bit_or, value_type, value_holder, shifted);
            EBM_ASSIGNMENT(res, value_holder, masked);
            return res;
        };

        auto do_it = add_endian_specific(
            ctx,
            endian,
            [&] -> expected<ebm::StatementRef> {
                return do_assign(counter);
            },
            [&] -> expected<ebm::StatementRef> {
                EBMU_INT_LITERAL(len_minus_one, n - 1);
                EBM_BINARY_OP(shift_index, ebm::BinaryOp::sub, counter_type, len_minus_one, counter);
                return do_assign(shift_index);
            });
        if (!do_it) {
            return unexpect_error(std::move(do_it.error()));
        }

        EBMU_INT_LITERAL(len, n);

        EBM_COUNTER_LOOP_END(loop_stmt, counter, len, *do_it);

        EBM_CAST(cast_ref, cast_to, value_type, value_holder);
        EBM_ASSIGNMENT(assign, to, cast_ref);

        ebm::Block block;
        block.container.reserve(5);
        append(block, buffer_def);
        append(block, counter_def);
        append(block, read_ref);
        append(block, loop_stmt);
        append(block, assign);
        EBM_BLOCK(block_ref, std::move(block));
        return block_ref;
    }

    expected<void> DecoderConverter::decode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& ity, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        MAYBE(endian, ctx.state().get_endian(ebm::Endian(ity->endian), ity->is_signed));
        io_desc.endian = endian;
        io_desc.size = get_size(*ity->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, decode_multi_byte_int_with_fixed_array(*ity->bit_size / 8, endian, base_ref, io_desc.data_type));
            append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, multi_byte_int));
        }
        return {};
    }

    expected<void> DecoderConverter::decode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& fty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        MAYBE(endian, ctx.state().get_endian(ebm::Endian(fty->endian), false));
        io_desc.endian = endian;
        io_desc.size = get_size(*fty->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, decode_multi_byte_int_with_fixed_array(*fty->bit_size / 8, endian, base_ref, io_desc.data_type));
            append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, multi_byte_int));
        }
        return {};
    }

    expected<void> DecoderConverter::decode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& ety, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        if (auto locked_enum = ety->base.lock()) {
            if (locked_enum->base_type) {
                EBMA_CONVERT_TYPE(to_ty, locked_enum->base_type);
                EBM_NEW_OBJECT(new_, to_ty);
                EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_var, to_ty, new_);
                MAYBE(decode_info, decode_field_type(locked_enum->base_type, tmp_var, nullptr));
                EBMA_ADD_STATEMENT(decode_stmt, std::move(decode_info));
                EBM_CAST(casted, io_desc.data_type, to_ty, tmp_var);
                EBM_ASSIGNMENT(assign, base_ref, casted);
                ebm::Block block;
                append(block, tmp_var_def);
                append(block, decode_stmt);
                append(block, assign);
                EBM_BLOCK(block_ref, std::move(block));
                auto io_data = ctx.repository().get_statement(decode_stmt)->body.read_data();
                io_desc.endian = io_data->endian;
                io_desc.size = io_data->size;
                ebm::LoweredStatement lowered;
                lowered.lowering_type = ebm::LoweringType::NAIVE;
                lowered.block = block_ref;
                append(lowered_stmts, std::move(lowered));
            }
            else {
                return unexpect_error("EnumType without base type cannot be used in encoding");
            }
        }
        else {
            return unexpect_error("EnumType has no base enum");
        }
        return {};
    }

    expected<void> DecoderConverter::decode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& aty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        EBMA_CONVERT_TYPE(element_type, aty->element_type);
        const auto elem_body = ctx.repository().get_type(element_type);
        const auto is_byte = elem_body->body.kind == ebm::TypeKind::UINT && *elem_body->body.size() == 8;
        auto fixed_unit = [is_byte]() -> ebm::SizeUnit {
            return is_byte ? ebm::SizeUnit::BYTE_FIXED : ebm::SizeUnit::ELEMENT_FIXED;
        };
        auto dynamic_unit = [is_byte]() -> ebm::SizeUnit {
            return is_byte ? ebm::SizeUnit::BYTE_DYNAMIC : ebm::SizeUnit::ELEMENT_DYNAMIC;
        };
        auto set_fixed_size = [&](size_t n) -> expected<void> {
            MAYBE(size, make_fixed_size(n, fixed_unit()));
            io_desc.size = size;
            return {};
        };
        auto set_dynamic_size = [&](ebm::ExpressionRef n) -> expected<void> {
            MAYBE(size, make_dynamic_size(n, dynamic_unit()));
            io_desc.size = size;
            return {};
        };
        std::optional<ebm::ExpressionRef> length;
        auto underlying_decoder = [&] -> expected<ebm::StatementRef> {
            EBM_NEW_OBJECT(new_, element_type);
            EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_var, element_type, new_);
            MAYBE(decode_info, decode_field_type(aty->element_type, tmp_var, nullptr));
            EBMA_ADD_STATEMENT(decode_stmt, std::move(decode_info));
            EBM_APPEND(appended, base_ref, tmp_var);
            ebm::Block block;
            append(block, tmp_var_def);
            append(block, decode_stmt);
            append(block, appended);
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        };
        if (aty->length_value) {
            EBMU_INT_LITERAL(imm_len, *aty->length_value);
            MAYBE_VOID(ok, set_fixed_size(*aty->length_value));
            length = imm_len;
        }
        else if (ast::is_any_range(aty->length)) {
            if (field) {
                if (is_alignment_vector(field)) {
                    MAYBE(req_size, get_alignment_requirement(ctx, *field->arguments->alignment_value / 8, ebm::StreamType::OUTPUT));
                    MAYBE_VOID(ok, set_dynamic_size(req_size));
                    length = req_size;
                }
                else if (field->follow == ast::Follow::end ||
                         (field->arguments && field->arguments->sub_byte_length)  // this means that the field is a sub-byte field
                ) {
                    const auto single_byte = get_size(8);
                    EBM_CAN_READ_STREAM(can_read, ebm::StreamType::INPUT, single_byte);
                    MAYBE(element_decoder, underlying_decoder());
                    EBM_WHILE_LOOP(loop, can_read, element_decoder);
                }
                else if (field->eventual_follow == ast::Follow::fixed) {
                    auto tail = field->belong_struct.lock()->fixed_tail_size / 8;
                    EBM_GET_REMAINING_BYTES(remain_bytes, ebm::StreamType::INPUT);
                    auto imm = immediate(m, tail);
                    if (!imm) {
                        return imm.error();
                    }
                    BM_NEW_ID(next_id, error, nullptr);
                    m.op(AbstractOp::REMAIN_BYTES, [&](Code& c) {
                        c.ident(next_id);
                    });
                    if (elem_is_int) {
                        // remain_bytes = REMAIN_BYTES - tail
                        // assert remain_bytes % elem_size == 0
                        // decode_int_vector(read_bytes / elem_size)
                        BM_BINARY(m.op, new_id, BinaryOp::sub, next_id, *imm);
                        auto elem_size = immediate(m, *elem_is_int->bit_size / futils::bit_per_byte);
                        if (!elem_size) {
                            return elem_size.error();
                        }
                        auto imm0 = immediate(m, 0);
                        if (!imm0) {
                            return imm0.error();
                        }
                        BM_BINARY(m.op, mod, BinaryOp::mod, new_id, *elem_size);
                        BM_BINARY(m.op, assert_expr, BinaryOp::equal, mod, *imm0);
                        m.op(AbstractOp::ASSERT, [&](Code& c) {
                            c.ref(assert_expr);
                            c.belong(m.get_function());
                        });
                        BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                        m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                            c.left_ref(base_ref);
                            c.right_ref(new_id);
                            c.endian(endian);
                            c.bit_size(*varint(*elem_is_int->bit_size));
                            c.belong(get_field_ref(m, field));
                        });
                        return none;
                    }
                    EBM_BINARY_OP(m.op, new_id, BinaryOp::grater, next_id, *imm);
                    return conditional_loop(m, new_id, undelying_decoder);
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
                    BM_NEW_OBJECT(m.op, new_id, *ref);
                    auto temporary_read_holder = define_typed_tmp_var(m, new_id, *ref, ast::ConstantLevel::variable);
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
                        BM_GET_STORAGE_REF_WITH_LOC(gen, error, isOkFlag, &next->loc);
                        BM_NEW_OBJECT(m.op, flagObj, gen);
                        BM_ERROR_WRAP(isOK, error, (define_bool_tmp_var(m, flagObj, ast::ConstantLevel::variable)));
                        auto immTrue = immediate_bool(m, true);
                        if (!immTrue) {
                            return immTrue.error();
                        }
                        auto immFalse = immediate_bool(m, false);
                        if (!immFalse) {
                            return immFalse.error();
                        }
                        auto err = do_assign(m, nullptr, nullptr, isOK, *immTrue);
                        if (err) {
                            return err;
                        }
                        err = counter_loop(m, *imm, [&](Varint i) {
                            BM_BEGIN_COND_BLOCK(m.op, m.code, cond_block, nullptr);
                            BM_INDEX(m.op, index_str, *str_ref, i);
                            BM_INDEX(m.op, index_peek, *temporary_read_holder, i);
                            BM_BINARY(m.op, cmp, BinaryOp::not_equal, index_str, index_peek);
                            BM_END_COND_BLOCK(m.op, m.code, cond_block, cmp);
                            m.init_phi_stack(cmp.value());
                            BM_REF(m.op, AbstractOp::IF, cond_block);
                            err = do_assign(m, nullptr, nullptr, isOK, *immFalse);
                            m.op(AbstractOp::BREAK);
                            m.op(AbstractOp::END_IF);
                            return insert_phi(m, m.end_phi_stack());
                        });
                        if (err) {
                            return err;
                        }
                        BM_REF(m.op, AbstractOp::IF, isOK);
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
                return unexpect_error("expected field but got nullptr");
            }
        }
        else {
            BM_GET_EXPR(id, m, arr->length);
            auto expr_type = arr->length->expr_type;
            if (auto u = ast::as<ast::UnionType>(expr_type)) {
                if (!u->common_type) {
                    return error("Union type must have common type");
                }
                expr_type = u->common_type;
            }
            BM_DEFINE_STORAGE(s, m, expr_type);
            auto len_ident = define_typed_tmp_var(m, id, s, ast::ConstantLevel::immutable_variable);
            if (!len_ident) {
                return len_ident.error();
            }
            if (elem_is_int) {
                BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*len_ident);
                    c.endian(endian);
                    c.bit_size(*varint(*elem_is_int->bit_size));
                    c.belong(get_field_ref(m, field));
                });
                return none;
            }
            m.op(AbstractOp::RESERVE_SIZE, [&](Code& c) {
                c.left_ref(base_ref);
                c.right_ref(*len_ident);
                c.reserve_type(ReserveType::DYNAMIC);
            });
            return counter_loop(m, *len_ident, [&](Varint) {
                return undelying_decoder();
            });
        }
    }

    expected<void> compare_string_array(ConverterContext& ctx, ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate) {
        EBMU_U8(u8_t);
        for (size_t i = 0; i < candidate.size(); i++) {
            EBMU_INT_LITERAL(i_ref, i);
            EBMU_INT_LITERAL(literal, static_cast<unsigned char>(candidate[i]));
            EBM_INDEX(array_index, u8_t, n_array, i_ref);
            EBM_BINARY_OP(check, ebm::BinaryOp::equal, u8_t, array_index, literal);
            MAYBE(assert_stmt, assert_statement(ctx, check));
            append(block, assert_stmt);
        }
        return {};
    }

    expected<void> DecoderConverter::decode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        MAYBE(candidate, decode_base64(typ->strong_ref));

        EBMU_U8_N_ARRAY(u8_n_array, candidate.size());
        EBM_NEW_OBJECT(new_obj_ref, u8_n_array);
        EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);

        MAYBE(io_size, make_fixed_size(candidate.size(), ebm::SizeUnit::BYTE_FIXED));
        io_desc.size = io_size;
        EBM_READ_DATA(read_ref, make_io_data(buffer, u8_n_array, io_desc.endian, io_size));

        ebm::Block block;
        append(block, buffer_def);
        append(block, read_ref);
        MAYBE_VOID(ok, compare_string_array(ctx, block, buffer, candidate));
        EBM_BLOCK(block_ref, std::move(block));
        append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, block_ref));
        return {};
    }

    expected<void> DecoderConverter::decode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        auto base = typ->base.lock();
        auto fmt = ast::as<ast::Format>(base);
        if (!fmt) {
            return unexpect_error("Struct type must have a format");
        }
        if (typ->bit_size && !(fmt->encode_fn.lock() || fmt->decode_fn.lock())) {
            auto size = get_size(*typ->bit_size);
            io_desc.size = size;
        }
        else {
            io_desc.size = ebm::Size{.unit = ebm::SizeUnit::DYNAMIC};
        }
        ebm::CallDesc call_desc;
        // force convert encode and decode functions
        EBMA_CONVERT_STATEMENT(ok, base);
        MAYBE(encdec, ctx.state().get_format_encode_decode(base));
        EBM_MEMBER_ACCESS(dec_access, encdec.decode_type, base_ref, encdec.decode);
        call_desc.callee = dec_access;
        // TODO: add arguments
        EBM_CALL(call_ref, std::move(call_desc));
        MAYBE(typ_ref, get_encoder_return_type(ctx));
        EBM_DEFINE_ANONYMOUS_VARIABLE(result, typ_ref, call_ref);
        EBM_IS_ERROR(is_error, result);
        EBM_ERROR_RETURN(error_return, result);
        EBM_IF_STATEMENT(if_stmt, is_error, error_return, {});

        ebm::Block block;
        block.container.reserve(2);
        append(block, result_def);
        append(block, if_stmt);
        EBM_BLOCK(block_ref, std::move(block));

        append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, block_ref));
        return {};
    }

}  // namespace ebmgen
