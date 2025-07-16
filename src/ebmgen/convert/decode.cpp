/*license*/
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
}