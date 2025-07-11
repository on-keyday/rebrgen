/*license*/
#pragma once
#include <bit>
#include <cstddef>
#include <memory>
#include "../converter.hpp"
#include "core/ast/ast.h"
#include "core/ast/node/type.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "helper.hpp"

namespace ebmgen {
    expected<ebm::EndianExpr> Converter::get_endian(ebm::Endian base, bool sign) {
        ebm::EndianExpr e;
        e.sign(sign);
        e.endian(base);
        if (base != ebm::Endian::unspec) {
            return e;
        }
        e.endian(global_endian);
        if (on_function) {
            e.endian(local_endian);
        }
        if (e.endian() == ebm::Endian::dynamic) {
            e.dynamic_ref = current_dynamic_endian;
        }
        return e;
    }

    bool Converter::set_endian(ebm::Endian e, ebm::StatementRef id) {
        if (on_function) {
            local_endian = e;
            current_dynamic_endian = id;
            return true;
        }
        if (e == ebm::Endian::dynamic) {
            return false;
        }
        global_endian = e;
        return true;
    }

    expected<ebm::TypeRef> Converter::get_unsigned_n_int(size_t n) {
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::UINT;
        typ.size(n);
        auto utyp = type_repo.add(ident_source, std::move(typ));
        if (!utyp) {
            return unexpect_error(std::move(utyp.error()));
        }
        return utyp;
    }

    expected<ebm::TypeRef> Converter::get_counter_type() {
        return get_unsigned_n_int(64);
    }

    expected<ebm::ExpressionRef> Converter::get_int_literal(std::uint64_t value) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::LITERAL_INT;
        body.int_value(value);
        auto int_literal = add_expr(std::move(body));
        if (!int_literal) {
            return unexpect_error(std::move(int_literal.error()));
        }
        return *int_literal;
    }

    expected<ebm::TypeRef> Converter::get_bool_type() {
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::BOOL;
        auto bool_type = type_repo.add(ident_source, std::move(typ));
        if (!bool_type) {
            return unexpect_error(std::move(bool_type.error()));
        }
        return bool_type;
    }

    expected<ebm::TypeRef> Converter::get_u8_n_array(size_t n) {
        auto u8typ = get_unsigned_n_int(8);
        if (!u8typ) {
            return unexpect_error(std::move(u8typ.error()));
        }
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::ARRAY;
        typ.element_type(*u8typ);
        typ.length(*varint(n));
        return type_repo.add(ident_source, std::move(typ));
    }

    expected<ebm::ExpressionRef> Converter::get_alignment_requirement(std::uint64_t alignment_bytes, ebm::StreamType type) {
        if (alignment_bytes == 0) {
            return unexpect_error("0 is not valid alignment");
        }
        if (alignment_bytes == 1) {
            return get_int_literal(0);
        }
        ebm::ExpressionBody body;
        MAYBE(counter_type, get_counter_type());
        body.type = counter_type;
        body.op = ebm::ExpressionOp::GET_STREAM_OFFSET;
        body.stream_type(type);
        MAYBE(stream_offset, add_expr(std::move(body)));

        MAYBE(alignment, get_int_literal(alignment_bytes));

        if (std::popcount(alignment_bytes) == 1) {
            MAYBE(alignment_bitmask, get_int_literal(alignment_bytes - 1));
            // size(=offset) & (alignment - 1)
            EBM_BINARY_OP(mod, ebm::BinaryOp::bit_and, counter_type, stream_offset, alignment_bitmask);
            // alignment - (size & (alignment-1))
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);
            // (alignment - (size & (alignment-1))) & (alignment - 1)
            EBM_BINARY_OP(req_size, ebm::BinaryOp::bit_and, counter_type, cmp, alignment_bitmask);
            return req_size;
        }
        else {
            // size(=offset) % alignment
            EBM_BINARY_OP(mod, ebm::BinaryOp::mod, counter_type, stream_offset, alignment);

            // alignment - (size % alignment)
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);

            // (alignment - (size % alignment)) % alignment
            EBM_BINARY_OP(req_size, ebm::BinaryOp::mod, counter_type, cmp, alignment);
            return req_size;
        }
    }

    ebm::IOData make_io_data(ebm::ExpressionRef target, ebm::TypeRef data_type, ebm::EndianExpr endian, ebm::Size size) {
        return ebm::IOData{
            .target = target,
            .data_type = data_type,
            .endian = endian,
            .size = size,
            .lowered_stmt = ebm::StatementRef{},
        };
    }

    expected<ebm::Size> make_fixed_size(size_t n, ebm::SizeUnit unit) {
        ebm::Size size;
        size.unit = unit;
        MAYBE(size_val, varint(n));
        if (!size.size(size_val)) {
            return unexpect_error("Unit {} is not fixed", to_string(unit));
        }
        return size;
    }

    expected<ebm::Size> make_dynamic_size(ebm::ExpressionRef ref, ebm::SizeUnit unit) {
        ebm::Size size;
        size.unit = unit;
        if (!size.ref(ref)) {
            return unexpect_error("Unit {} is not dynamic", to_string(unit));
        }
        return size;
    }

#define COMMON_BUFFER_SETUP(IO_MACRO, io_ref)                       \
    MAYBE(u8_n_array, get_u8_n_array(n));                           \
    EBM_NEW_OBJECT(new_obj_ref, u8_n_array);                        \
    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref); \
    MAYBE(value_type, get_unsigned_n_int(n * 8));                   \
    MAYBE(zero, get_int_literal(0));                                \
    MAYBE(io_size, make_fixed_size(n, ebm::SizeUnit::BYTE_FIXED));  \
    IO_MACRO(io_ref, (make_io_data(buffer, u8_n_array, endian, io_size)));

    expected<ebm::StatementRef> Converter::encode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef from, ebm::TypeRef cast_from) {
        COMMON_BUFFER_SETUP(EBM_WRITE_DATA, write_ref);
        EBM_CAST(casted, value_type, cast_from, from);  // if value_type == cast_from, then this is a no-op

        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, buffer, zero);
            EBM_ASSIGNMENT(assign, array_index, casted);
            ebm::Block block;
            block.container.reserve(3);
            append(block, buffer_def);
            append(block, assign);
            append(block, write_ref);
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        }

        MAYBE(counter_type, get_counter_type());

        EBM_COUNTER_LOOP_START(counter);

        MAYBE(eight, get_int_literal(8));
        MAYBE(xFF, get_int_literal(0xff));
        MAYBE(u8_t, get_unsigned_n_int(8));
        // casted = value_type(from)
        // buffer = [0] * n
        // for counter in 0..len:
        //   if little endian
        //     shift_index = counter
        //   else
        //     // len - 1 can be constant
        //     shift_index = len - 1 - counter
        //   buffer[counter] = (casted >> (8 * shift_index)) & 0xff
        // write(buffer)
        auto do_assign = [&](ebm::ExpressionRef shift_index) -> expected<ebm::StatementRef> {
            EBM_BINARY_OP(shift, ebm::BinaryOp::mul, counter_type, shift_index, eight);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::right_shift, value_type, casted, shift);
            EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, value_type, shifted, xFF);
            EBM_CAST(casted2, u8_t, value_type, masked);
            EBM_INDEX(array_index, buffer, counter);
            EBM_ASSIGNMENT(res, array_index, casted2);
            return res;
        };

        auto do_it = add_endian_specific(
            endian,
            [&] -> expected<ebm::StatementRef> {
                return do_assign(counter);
            },
            [&] -> expected<ebm::StatementRef> {
                MAYBE(len_minus_one, get_int_literal(n - 1));
                EBM_BINARY_OP(shift_index, ebm::BinaryOp::sub, counter_type, len_minus_one, counter);
                return do_assign(shift_index);
            });
        if (!do_it) {
            return unexpect_error(std::move(do_it.error()));
        }

        MAYBE(len, get_int_literal(n));

        EBM_COUNTER_LOOP_END(loop_stmt, counter, len, *do_it);

        ebm::Block block;
        block.container.reserve(4);
        append(block, buffer_def);
        append(block, counter_def);
        append(block, loop_stmt);
        append(block, write_ref);
        EBM_BLOCK(block_ref, std::move(block));
        return block_ref;
    }

    expected<ebm::StatementRef> Converter::decode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef to, ebm::TypeRef cast_to) {
        COMMON_BUFFER_SETUP(EBM_READ_DATA, read_ref);

        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, buffer, zero);
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

        MAYBE(counter_type, get_counter_type());

        EBM_COUNTER_LOOP_START(counter);

        MAYBE(eight, get_int_literal(8));
        MAYBE(xFF, get_int_literal(0xff));
        MAYBE(u8_t, get_unsigned_n_int(8));
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
            EBM_INDEX(array_index, buffer, counter);
            EBM_CAST(casted, value_type, u8_t, array_index);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, value_type, casted, shift);
            EBM_BINARY_OP(masked, ebm::BinaryOp::bit_or, value_type, value_holder, shifted);
            EBM_ASSIGNMENT(res, value_holder, masked);
            return res;
        };

        auto do_it = add_endian_specific(
            endian,
            [&] -> expected<ebm::StatementRef> {
                return do_assign(counter);
            },
            [&] -> expected<ebm::StatementRef> {
                MAYBE(len_minus_one, get_int_literal(n - 1));
                EBM_BINARY_OP(shift_index, ebm::BinaryOp::sub, counter_type, len_minus_one, counter);
                return do_assign(shift_index);
            });
        if (!do_it) {
            return unexpect_error(std::move(do_it.error()));
        }

        MAYBE(len, get_int_literal(n));

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

    ebm::Size get_size(size_t bit_size) {
        ebm::Size size;
        if (bit_size % 8 == 0) {
            size.unit = ebm::SizeUnit::BYTE_FIXED;
            size.size(*varint(bit_size / 8));
        }
        else {
            size.unit = ebm::SizeUnit::BIT_FIXED;
            size.size(*varint(bit_size));
        }
        return size;
    }

    expected<void> Converter::encode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& ity, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        auto endian = get_endian(ebm::Endian(ity->endian), ity->is_signed);
        if (!endian) {
            return unexpect_error(std::move(endian.error()));
        }
        io_desc.endian = *endian;
        io_desc.size = get_size(*ity->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            auto multi_byte_int = encode_multi_byte_int_with_fixed_array(*ity->bit_size / 8, *endian, base_ref, io_desc.data_type);
            if (!multi_byte_int) {
                return unexpect_error(std::move(multi_byte_int.error()));
            }
            ebm::LoweredStatement lowered;
            lowered.lowering_type = ebm::LoweringType::FUNDAMENTAL;
            lowered.block = *multi_byte_int;
            append(lowered_stmts, std::move(lowered));
        }
        return {};
    }

    expected<void> Converter::encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& fty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        auto endian = get_endian(ebm::Endian(fty->endian), false);
        if (!endian) {
            return unexpect_error(std::move(endian.error()));
        }
        io_desc.endian = *endian;
        io_desc.size = get_size(*fty->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            auto multi_byte_int = encode_multi_byte_int_with_fixed_array(*fty->bit_size / 8, *endian, base_ref, io_desc.data_type);
            if (!multi_byte_int) {
                return unexpect_error(std::move(multi_byte_int.error()));
            }
            ebm::LoweredStatement lowered;
            lowered.lowering_type = ebm::LoweringType::FUNDAMENTAL;
            lowered.block = *multi_byte_int;
            append(lowered_stmts, std::move(lowered));
        }
        return {};
    }

    expected<void> Converter::encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& ety, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        if (auto locked_enum = ety->base.lock()) {
            if (locked_enum->base_type) {
                MAYBE(to_ty, convert_type(locked_enum->base_type));
                EBM_CAST(casted, to_ty, io_desc.data_type, base_ref);
                MAYBE(encode_info, encode_field_type(locked_enum->base_type, casted, field));
                auto copy = *statement_repo.get(encode_info)->body.write_data();
                io_desc.endian = copy.endian;
                io_desc.size = copy.size;
                ebm::LoweredStatement lowered;
                lowered.lowering_type = ebm::LoweringType::FUNDAMENTAL;
                lowered.block = encode_info;
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

    expected<void> Converter::encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& aty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        if (aty->length_value) {
            io_desc.size.unit = ebm::SizeUnit::ELEMENT_FIXED;
            MAYBE(length_value, varint(*aty->length_value));
            io_desc.size.size(length_value);
        }
        else if (aty->length) {
            MAYBE(length_expr, convert_expr(aty->length));
            io_desc.size.unit = ebm::SizeUnit::ELEMENT_DYNAMIC;
            io_desc.size.ref(length_expr);
        }
        else {
            return unexpect_error("ArrayType must have a length or length_value");
        }
        auto len = aty->length_value;
        // auto elem_is_int = ast::as<ast::IntType>(aty->element_type);
        if (len) {
            MAYBE(imm_len, get_int_literal(*len));
            EBM_COUNTER_LOOP_START(i);
            EBM_INDEX(indexed, base_ref, i);
            MAYBE(encode_info, encode_field_type(aty->element_type, indexed, field));
            EBM_COUNTER_LOOP_END(loop_stmt, i, imm_len, encode_info);
            ebm::LoweredStatement lowered;
            lowered.lowering_type = ebm::LoweringType::FUNDAMENTAL;
            lowered.block = loop_stmt;
            append(lowered_stmts, std::move(lowered));
            return {};
        }
        if (!aty->length) {
            return unexpect_error("Array length is not specified");
        }
        EBM_ARRAY_SIZE(array_size, base_ref);
        if (ast::is_any_range(aty->length)) {
            if (is_alignment_vector(field)) {
                auto req_size = get_alignment_requirement(*field->arguments->alignment_value / 8, ebm::StreamType::OUTPUT);
                if (!req_size) {
                    return req_size.error();
                }
                BM_GET_ENDIAN(endian, Endian::unspec, false);
                auto array_size = *field->arguments->alignment_value / 8 - 1;
                BM_ENCODE_INT_VEC_FIXED(m.op, base_ref, *req_size, endian, 8, (get_field_ref(m, field)), array_size);
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
            BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
            m.op(AbstractOp::ENCODE_INT_VECTOR, [&](Code& c) {
                c.left_ref(base_ref);
                c.right_ref(len_);
                c.endian(endian);
                c.bit_size(*varint(*elem_is_int->bit_size));
                c.belong(get_field_ref(m, field));
            });
            return none;
        }
        return counter_loop(m, len_, [&](Varint counter) {
            BM_INDEX(m.op, index, base_ref, counter);
            auto err = encode_type(m, arr->element_type, index, mapped_type, field, false);
            if (err) {
                return err;
            }
            return none;
        });
    }

    expected<ebm::StatementRef> Converter::encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        if (auto ity = ast::as<ast::IdentType>(typ)) {
            return encode_field_type(ity->base.lock(), base_ref, field);
        }
        auto typ_ref = convert_type(typ);
        if (!typ_ref) {
            return unexpect_error(std::move(typ_ref.error()));
        }
        ebm::IOData io_desc = make_io_data(base_ref, *typ_ref, ebm::EndianExpr{}, ebm::Size{});
        ebm::LoweredStatements lowered_stmts;  // omit if empty

        io_desc.data_type = *typ_ref;
        if (auto ity = ast::as<ast::IntType>(typ)) {
            MAYBE_VOID(ok, encode_int_type(io_desc, ast::cast_to<ast::IntType>(typ), base_ref, lowered_stmts));
        }
        else if (auto fty = ast::as<ast::FloatType>(typ)) {
            MAYBE_VOID(ok, encode_float_type(io_desc, ast::cast_to<ast::FloatType>(typ), base_ref, lowered_stmts));
        }
        else if (auto ety = ast::as<ast::EnumType>(typ)) {
            MAYBE_VOID(ok, encode_enum_type(io_desc, ast::cast_to<ast::EnumType>(typ), base_ref, lowered_stmts, field));
        }
        else if (auto aty = ast::as<ast::ArrayType>(typ)) {
            MAYBE_VOID(ok, encode_array_type(io_desc, ast::cast_to<ast::ArrayType>(typ), base_ref, lowered_stmts));
        }
        else {
            return unexpect_error("Unsupported type for encoding: {}", node_type_to_string(typ->node_type));
        }
        if (lowered_stmts.container.size()) {
            ebm::StatementBody body;
            body.statement_kind = ebm::StatementOp::LOWERED_STATEMENTS;
            body.lowered_statements(std::move(lowered_stmts));
            MAYBE(lowered_stmt, add_statement(std::move(body)));
            io_desc.lowered_stmt = lowered_stmt;
        }
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::WRITE_DATA;
        body.write_data(std::move(io_desc));
        return add_statement(std::move(body));
    }

}  // namespace ebmgen
