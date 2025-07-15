/*license*/
#pragma once
#include <cstddef>
#include <memory>
#include "../converter.hpp"
#include "core/ast/ast.h"
#include "core/ast/node/statement.h"
#include "core/ast/node/type.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "helper.hpp"
#include <fnet/util/base64.h>

namespace ebmgen {

#define COMMON_BUFFER_SETUP(IO_MACRO, io_ref)                              \
    MAYBE(u8_n_array, get_u8_n_array(n));                                  \
    EBM_NEW_OBJECT(new_obj_ref, u8_n_array);                               \
    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);        \
    MAYBE(value_type, get_unsigned_n_int(n * 8));                          \
    MAYBE(zero, get_int_literal(0));                                       \
    MAYBE(io_size, make_fixed_size(n, ebm::SizeUnit::BYTE_FIXED));         \
    IO_MACRO(io_ref, (make_io_data(buffer, u8_n_array, endian, io_size))); \
    MAYBE(u8_t, get_unsigned_n_int(8));

    expected<ebm::StatementRef> EncoderConverter::encode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef from, ebm::TypeRef cast_from) {
        COMMON_BUFFER_SETUP(EBM_WRITE_DATA, write_ref);
        EBM_CAST(casted, value_type, cast_from, from);  // if value_type == cast_from, then this is a no-op

        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, u8_t, buffer, zero);
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
            EBM_INDEX(array_index, u8_t, buffer, counter);
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

        MAYBE(counter_type, get_counter_type());

        EBM_COUNTER_LOOP_START(counter);

        MAYBE(eight, get_int_literal(8));
        MAYBE(xFF, get_int_literal(0xff));
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
        MAYBE(endian, get_endian(ebm::Endian(ity->endian), ity->is_signed));
        io_desc.endian = endian;
        io_desc.size = get_size(*ity->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, encode_multi_byte_int_with_fixed_array(*ity->bit_size / 8, endian, base_ref, io_desc.data_type));
            append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, multi_byte_int));
        }
        return {};
    }

    expected<void> Converter::encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& fty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        MAYBE(endian, get_endian(ebm::Endian(fty->endian), false));
        io_desc.endian = endian;
        io_desc.size = get_size(*fty->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, encode_multi_byte_int_with_fixed_array(*fty->bit_size / 8, endian, base_ref, io_desc.data_type));
            append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, multi_byte_int));
        }
        return {};
    }

    expected<void> Converter::encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& ety, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        if (auto locked_enum = ety->base.lock()) {
            if (locked_enum->base_type) {
                MAYBE(to_ty, convert_type(locked_enum->base_type));
                EBM_CAST(casted, to_ty, io_desc.data_type, base_ref);
                MAYBE(encode_info, encode_field_type(locked_enum->base_type, casted, nullptr));
                auto copy = *statement_repo.get(encode_info)->body.write_data();
                io_desc.endian = copy.endian;
                io_desc.size = copy.size;
                ebm::LoweredStatement lowered;
                lowered.lowering_type = ebm::LoweringType::NAIVE;
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

    bool is_alignment_vector(const std::shared_ptr<ast::Field>& t) {
        if (!t) {
            return false;
        }
        if (auto arr = ast::as<ast::ArrayType>(t->field_type)) {
            auto elm_is_int = ast::as<ast::IntType>(arr->element_type);
            if (elm_is_int && !elm_is_int->is_signed &&
                elm_is_int->bit_size == 8 &&
                t->arguments &&
                t->arguments->alignment_value &&
                *t->arguments->alignment_value != 0 &&
                *t->arguments->alignment_value % 8 == 0) {
                return true;
            }
        }
        return false;
    }

    expected<ebm::StatementRef> Converter::assert_statement(ebm::ExpressionRef condition) {
        MAYBE(assert_stmt, assert_statement_body(condition));
        MAYBE(stmt_ref, add_statement(std::move(assert_stmt)));
        return stmt_ref;
    }

    expected<ebm::StatementBody> Converter::assert_statement_body(ebm::ExpressionRef condition) {
        ebm::LoweredStatements lowered_stmts;

        // TODO: add more specific error message
        MAYBE(error_msg, add_string("Assertion failed"));
        EBM_ERROR_REPORT(error_report, error_msg, {});
        MAYBE(bool_type, get_bool_type());
        EBM_UNARY_OP(not_condition, ebm::UnaryOp::logical_not, bool_type, condition);
        EBM_IF_STATEMENT(if_stmt, not_condition, error_report, {});

        append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, if_stmt));

        EBM_LOWERED_STATEMENTS(lowered, std::move(lowered_stmts));

        return make_assert_statement(not_condition, lowered);
    }

    expected<void> Converter::encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& aty, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
        MAYBE(element_type, convert_type(aty->element_type));
        const auto elem_body = type_repo.get(element_type);
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
        ebm::ExpressionRef length;
        ebm::StatementRef assert_;
        if (aty->length_value) {
            MAYBE(imm_len, get_int_literal(*aty->length_value));
            MAYBE_VOID(ok, set_fixed_size(*aty->length_value));
            length = imm_len;
        }
        else {
            if (!aty->length) {
                return unexpect_error("Array length is not specified");
            }
            EBM_ARRAY_SIZE(array_size, base_ref);

            if (ast::is_any_range(aty->length)) {
                if (is_alignment_vector(field)) {  // this means array is fixed size, but we need to calculate the alignment at runtime
                    MAYBE(req_size, get_alignment_requirement(*field->arguments->alignment_value / 8, ebm::StreamType::OUTPUT));
                    MAYBE_VOID(ok, set_dynamic_size(req_size));
                    length = req_size;
                }
                else {
                    MAYBE_VOID(ok, set_dynamic_size(array_size));
                    length = array_size;
                }
            }
            else {
                MAYBE(len_init, convert_expr(aty->length));
                auto expr_type = aty->length->expr_type;
                if (auto u = ast::as<ast::UnionType>(expr_type)) {
                    if (!u->common_type) {
                        return unexpect_error("Union type must have common type");
                    }
                    expr_type = u->common_type;
                }
                MAYBE(bool_type, get_bool_type());
                EBM_BINARY_OP(eq, ebm::BinaryOp::equal, bool_type, array_size, len_init);
                MAYBE(assert_stmt, assert_statement(eq));
                assert_ = assert_stmt;
                MAYBE_VOID(ok, set_dynamic_size(len_init));
                length = len_init;
            }
        }
        EBM_COUNTER_LOOP_START(counter);
        EBM_INDEX(indexed, element_type, base_ref, counter);
        MAYBE(encode_info, encode_field_type(aty->element_type, indexed, nullptr));
        EBM_COUNTER_LOOP_END(loop_stmt, counter, length, encode_info);

        ebm::Block block;
        block.container.reserve(2 + (assert_.id.value() != 0));
        append(block, counter_def);
        if (assert_.id.value() != 0) {
            append(block, assert_);
        }
        append(block, loop_stmt);

        EBM_BLOCK(loop_block, std::move(block));

        append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, loop_block));

        return {};
    }

    expected<void> Converter::construct_string_array(ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate) {
        MAYBE(u8_t, get_unsigned_n_int(8));
        for (size_t i = 0; i < candidate.size(); i++) {
            MAYBE(i_ref, get_int_literal(i));
            MAYBE(literal, get_int_literal(static_cast<unsigned char>(candidate[i])));
            EBM_INDEX(array_index, u8_t, n_array, i_ref);
            EBM_ASSIGNMENT(assign, array_index, literal);
            append(block, assign);
        }
        return {};
    }

    expected<std::string> decode_base64(const std::shared_ptr<ast::StrLiteral>& lit) {
        std::string candidate;
        if (!futils::base64::decode(lit->binary_value, candidate)) {
            return unexpect_error("Invalid base64 string: {}", lit->binary_value);
        }
        return candidate;
    }

    expected<void> Converter::encode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts) {
        MAYBE(candidate, decode_base64(typ->strong_ref));

        MAYBE(u8_n_array, get_u8_n_array(candidate.size()));
        EBM_NEW_OBJECT(new_obj_ref, u8_n_array);
        EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);

        MAYBE(io_size, make_fixed_size(candidate.size(), ebm::SizeUnit::BYTE_FIXED));
        io_desc.size = io_size;
        EBM_WRITE_DATA(write_ref, make_io_data(buffer, u8_n_array, io_desc.endian, io_size));

        ebm::Block block;
        append(block, buffer_def);
        MAYBE_VOID(ok, construct_string_array(block, buffer, candidate));
        append(block, write_ref);
        EBM_BLOCK(block_ref, std::move(block));
        append(lowered_stmts, make_lowered_statement(ebm::LoweringType::NAIVE, block_ref));
        return {};
    }

    expected<void> Converter::encode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field) {
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
        MAYBE_VOID(ok, convert_statement(base));
        MAYBE(encdec, get_format_encode_decode(base));
        EBM_MEMBER_ACCESS(enc_access, encdec.encode_type, base_ref, encdec.encode);
        call_desc.callee = enc_access;
        // TODO: add arguments
        EBM_CALL(call_ref, std::move(call_desc));
        MAYBE(typ_ref, get_encoder_return_type());
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

    expected<ebm::StatementRef> Converter::encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        if (auto ity = ast::as<ast::IdentType>(typ)) {
            return encode_field_type(ity->base.lock(), base_ref, field);
        }
        auto typ_ref = convert_type(typ, field);
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
        else if (auto str_lit = ast::as<ast::StrLiteralType>(typ)) {
            MAYBE_VOID(ok, encode_str_literal_type(io_desc, ast::cast_to<ast::StrLiteralType>(typ), base_ref, lowered_stmts));
        }
        else if (auto ety = ast::as<ast::EnumType>(typ)) {
            MAYBE_VOID(ok, encode_enum_type(io_desc, ast::cast_to<ast::EnumType>(typ), base_ref, lowered_stmts, field));
        }
        else if (auto aty = ast::as<ast::ArrayType>(typ)) {
            MAYBE_VOID(ok, encode_array_type(io_desc, ast::cast_to<ast::ArrayType>(typ), base_ref, lowered_stmts, field));
        }
        else {
            return unexpect_error("Unsupported type for encoding: {}", node_type_to_string(typ->node_type));
        }
        assert(io_desc.size.unit != ebm::SizeUnit::UNKNOWN);
        if (lowered_stmts.container.size()) {
            EBM_LOWERED_STATEMENTS(lowered_stmt, std::move(lowered_stmts));
            io_desc.lowered_stmt = lowered_stmt;
        }
        ebm::StatementBody body;
        body.statement_kind = ebm::StatementOp::WRITE_DATA;
        body.write_data(std::move(io_desc));
        return add_statement(std::move(body));
    }

}  // namespace ebmgen
