/*license*/
#include <bm/fallback.hpp>

namespace rebgn {
    Error fallback_encode_int(Module& m, Code& code, std::vector<Code>& new_code) {
        NEW_CODE_OP(new_code);
        BM_NEW_ID(fallback_id, error, nullptr);
        code.fallback(fallback_id);
        op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
            m.ident(fallback_id);
        });

        auto value = code.ref().value();
        auto bit_size = code.bit_size()->value();
        auto endian = code.endian().value();
        auto belong = code.belong().value();

        BM_ERROR_WRAP(buffer, error, (new_array(bit_size / 8)));
        BM_IMMEDIATE(op, len, bit_size / 8);

        auto st = Storages{.length = *varint(1), .storages = {Storage{.type = StorageType::UINT}}};
        st.storages[0].size(*varint(64));

        BM_GET_STORAGE_REF(ref, st);
        BM_IMMEDIATE(op, imm0, 0);

        BM_ERROR_WRAP(counter, error, (new_var(ref, imm0)));

        BM_BINARY(op, cmp_id, BinaryOp::less, counter, len);

        auto loop = [&](auto&& body) -> Error {
            op(AbstractOp::LOOP_CONDITION, [&](Code& m) {
                m.ref(cmp_id);
            });
            BM_ERROR_WRAP_ERROR(error, body());
            BM_REF(op, AbstractOp::INC, counter);
            op(AbstractOp::END_LOOP, [&](Code& m) {});
            return none;
        };

        auto assign = [&](Varint shift_index) {
            BM_IMMEDIATE(op, imm8, 8);
            BM_BINARY(op, shift, BinaryOp::mul, shift_index, imm8);
            BM_BINARY(op, shifted, BinaryOp::right_logical_shift, value, shift);
            BM_IMMEDIATE(op, immFF, 0xff);
            BM_BINARY(op, masked, BinaryOp::bit_and, shifted, immFF);
            BM_INDEX(op, array_index, buffer, counter);
            BM_ASSIGN(op, assign, array_index, masked, null_varint, nullptr);
            return none;
        };

        // if little endian
        //   shift_index = counter
        // else
        //   // len - 1 can be constant
        //   shift_index = len - 1 - counter
        // buffer[counter] = (value >> (8 * shift_index)) & 0xff

        auto err = add_endian_specific(
            m, endian, op,
            [&] {  // on little endian
                return loop([&] {
                    return assign(counter);
                });
            },
            [&] {  // on big endian
                return loop([&] {
                    BM_IMMEDIATE(op, len_minus_1, bit_size / 8 - 1);
                    BM_BINARY(op, shift_index, BinaryOp::sub, len_minus_1, counter);
                    return assign(shift_index);
                });
            });
        if (err) {
            return err;
        }

        BM_ENCODE_INT_VEC_FIXED(op, buffer, endian, 8, belong, bit_size / 8);

        op(AbstractOp::END_FALLBACK, [&](Code& m) {});
        return none;
    }

}  // namespace rebgn