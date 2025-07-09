/*license*/
#pragma once
#include "../converter.hpp"
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

    expected<ebm::StatementRef> Converter::encode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef from, ebm::TypeRef cast_from) {
        // add fallback
        MAYBE(u8_n_array, get_u8_n_array(n));
        EBM_NEW_OBJECT(new_obj_ref, u8_n_array);
        EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);
        MAYBE(value_type, get_unsigned_n_int(n * 8));
        if (value_type.id.value() != cast_from.id.value()) {
            EBM_CAST(cast_ref, value_type, cast_from, from);
            from = cast_ref;
        }

        MAYBE(zero, get_int_literal(0));
        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, buffer, zero);
            EBM_ASSIGNMENT(assign, array_index, from);
            ebm::IOData io_desc;
            io_desc.data_type = u8_n_array;
            io_desc.size.unit = ebm::SizeUnit::BYTE_FIXED;
            auto size_val = varint(n);
            if (!size_val) {
                return unexpect_error(std::move(size_val.error()));
            }
            io_desc.size.size(*size_val);
            io_desc.endian = endian;
            ebm::StatementBody body;
            body.statement_kind = ebm::StatementOp::WRITE_DATA;
            body.write_data(std::move(io_desc));
            MAYBE(write_ref, add_statement(std::move(body)));
            ebm::Block block;
            block.container.reserve(2);
            append(block, buffer_def);
            append(block, assign);
            append(block, write_ref);
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        }

        MAYBE(counter_type, get_counter_type());

        MAYBE(len, get_int_literal(n));

        EBM_DEFINE_ANONYMOUS_VARIABLE(counter, counter_type, zero);

        MAYBE(bool_type, get_bool_type());

        EBM_BINARY_OP(cmp_id, ebm::BinaryOp::less, bool_type, counter, len);
        MAYBE(eight, get_int_literal(8));
        MAYBE(xFF, get_int_literal(0xff));
        MAYBE(u8_t, get_unsigned_n_int(8));
        MAYBE(len_minus_one, get_int_literal(n - 1));
        // if little endian
        //   shift_index = counter
        // else
        //   // len - 1 can be constant
        //   shift_index = len - 1 - counter
        // buffer[counter] = (from >> (8 * shift_index)) & 0xff
        auto do_assign = [&](ebm::ExpressionRef shift_index) -> expected<ebm::StatementRef> {
            EBM_BINARY_OP(shift, ebm::BinaryOp::mul, counter_type, shift_index, eight);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::right_shift, value_type, from, shift);
            EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, value_type, shifted, xFF);
            EBM_CAST(casted, u8_t, value_type, masked);
            EBM_INDEX(array_index, buffer, counter);
            EBM_ASSIGNMENT(res, array_index, casted);
            return res;
        };

        auto do_it = add_endian_specific(
            endian,
            [&] -> expected<ebm::StatementRef> {
                return do_assign(counter);
            },
            [&] -> expected<ebm::StatementRef> {
                EBM_BINARY_OP(shift_index, ebm::BinaryOp::sub, counter_type, len_minus_one, counter);
                return do_assign(shift_index);
            });
        if (!do_it) {
            return unexpect_error(std::move(do_it.error()));
        }
        ebm::Block loop_body;
        loop_body.container.reserve(2);
        append(loop_body, *do_it);
        EBM_INCREMENT(inc, counter);
        append(loop_body, inc);
        EBM_BLOCK(loop_block, std::move(loop_body));
        EBM_WHILE_LOOP(loop_stmt, cmp_id, loop_block);

        ebm::IOData io_desc;
        io_desc.data_type = u8_n_array;
        io_desc.size.unit = ebm::SizeUnit::BYTE_FIXED;
        auto size_val = varint(n);
        if (!size_val) {
            return unexpect_error(std::move(size_val.error()));
        }
        io_desc.size.size(*size_val);
        io_desc.endian = endian;
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::WRITE_DATA;
        body.write_data(std::move(io_desc));
        MAYBE(write_ref, add_statement(std::move(body)));

        ebm::Block block;
        block.container.reserve(4);
        append(block, buffer_def);
        append(block, counter_def);
        append(block, loop_stmt);
        append(block, write_ref);
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

    expected<ebm::StatementRef> Converter::encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        if (auto ity = ast::as<ast::IdentType>(typ)) {
            return encode_field_type(ity->base.lock(), base_ref, field);
        }
        ebm::IOData io_desc;
        ebm::FallbackStatements fallback_stmts;  // omit if empty
        auto typ_ref = convert_type(typ);
        if (!typ_ref) {
            return unexpect_error(std::move(typ_ref.error()));
        }
        io_desc.data_type = *typ_ref;
        if (auto ity = ast::as<ast::IntType>(typ)) {
            auto endian = get_endian(ebm::Endian(ity->endian), ity->is_signed);
            if (!endian) {
                return unexpect_error(std::move(endian.error()));
            }
            io_desc.endian = *endian;
            io_desc.size = get_size(*ity->bit_size);
            if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                auto multi_byte_int = encode_multi_byte_int_with_fixed_array(*ity->bit_size / 8, *endian, base_ref, *typ_ref);
                if (!multi_byte_int) {
                    return unexpect_error(std::move(multi_byte_int.error()));
                }
                ebm::FallbackStatement fallback;
                fallback.fallback_type = ebm::FallbackType::FUNDAMENTAL;
                fallback.block = *multi_byte_int;
                append(fallback_stmts, std::move(fallback));
            }
        }
        if (fallback_stmts.container.size()) {
            ebm::StatementBody body;
            body.statement_kind = ebm::StatementOp::FALLBACK_STATEMENTS;
            body.fallback_statements(std::move(fallback_stmts));
            MAYBE(fallback_stmt, add_statement(std::move(body)));
            io_desc.fallback_stmt = fallback_stmt;
        }
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::WRITE_DATA;
        body.write_data(std::move(io_desc));
        return add_statement(std::move(body));
    }

}  // namespace ebmgen
