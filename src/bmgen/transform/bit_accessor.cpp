/*license*/
#include <bmgen/internal.hpp>
#include <bmgen/fallback.hpp>

namespace rebgn {
    expected<size_t> bit_size(const Storages& s) {
        if (s.storages.size() == 0) {
            return unexpect_error("Invalid storage size");
        }
        size_t index = 0;
        if (s.storages[index].type == StorageType::ENUM) {
            index++;
            if (s.storages.size() == index) {
                return unexpect_error("Invalid storage size");
            }
        }
        if (s.storages[index].type == StorageType::UINT || s.storages[index].type == StorageType::INT) {
            return s.storages[index].size()->value();
        }
        if (s.storages[index].type == StorageType::STRUCT_REF) {
            if (s.storages[index].size()->value() == 0) {
                return unexpect_error("Invalid storage size");
            }
            return s.storages[index].size()->value() - 1;
        }
        if (s.storages[index].type == StorageType::VARIANT) {
            size_t candidate = 0;
            for (index++; index < s.storages.size(); index++) {
                if (s.storages[index].type != StorageType::STRUCT_REF) {
                    return unexpect_error("Invalid storage type");
                }
                auto size = s.storages[index].size()->value();
                if (size == 0) {
                    return unexpect_error("Invalid storage size");
                }
                candidate = std::max(candidate, size);
            }
            return candidate;
        }
        return unexpect_error("Invalid storage type");
    }

    Error derive_bit_field_accessor_functions(Module& m) {
        std::vector<Code> new_code;
        NEW_CODE_OP(new_code);
        std::optional<Varint> target;
        std::optional<StorageRef> bit_field_type;
        std::optional<Storages> detailed_field_type;
        size_t offset = 0;
        size_t max_offset = 0;

        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_BIT_FIELD) {
                target = c.ident();
                bit_field_type = c.type();
                BM_ERROR_WRAP(detailed_type, error, m.get_storage(c.type().value()));
                detailed_field_type = detailed_type;
                max_offset = detailed_type.storages[0].size()->value();
            }
            if (c.op == AbstractOp::END_BIT_FIELD) {
                target.reset();
                bit_field_type.reset();
                detailed_field_type.reset();
                offset = 0;
            }
            if (target && c.op == AbstractOp::DEFINE_FIELD) {
                BM_ERROR_WRAP(detailed_type, error, m.get_storage(c.type().value()));
                auto n_bit = detailed_type.storages[0].size()->value();

                std::uint64_t max_mask = (std::uint64_t(1) << n_bit) - 1;

                BM_NEW_ID(getter_id, error, nullptr);
                BM_IMMEDIATE(op, shift_offset, offset);
                BM_IMMEDIATE(op, mask, max_mask);
                {
                    op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
                        c.ident(getter_id);
                        c.belong(c.belong().value());
                        c.func_type(FunctionType::BIT_GETTER);
                    });

                    op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                        c.type(c.type().value());
                    });

                    // target_type((bit_fields >> offset) & max(n bit))

                    BM_BINARY(op, shift, BinaryOp::right_logical_shift, *target, shift_offset);
                    BM_BINARY(op, and_, BinaryOp::bit_and, shift, mask);
                    auto cast_type = get_cast_type(detailed_type, *detailed_field_type);
                    BM_CAST(op, cast, c.type().value(), *bit_field_type, and_, cast_type);

                    op(AbstractOp::RET, [&](Code& c) {
                        c.ref(cast);
                        c.belong(getter_id);
                    });

                    BM_OP(op, AbstractOp::END_FUNCTION);
                }
                {
                    BM_NEW_ID(setter_id, error, nullptr);
                    op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
                        c.ident(setter_id);
                        c.belong(c.belong().value());
                        c.func_type(FunctionType::BIT_SETTER);
                    });

                    auto ret_type_ref = m.get_storage_ref(
                        Storages{
                            .length = *varint(1),
                            .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
                        },
                        nullptr);
                    if (!ret_type_ref) {
                        return ret_type_ref.error();
                    }

                    op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                        c.type(*ret_type_ref);
                    });

                    BM_NEW_ID(prop_input, error, nullptr);
                    op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& c) {
                        c.ident(prop_input);
                        c.left_ref(c.ident().value());
                        c.right_ref(setter_id);
                        c.type(c.type().value());
                    });

                    auto size = bit_size(*detailed_field_type);
                    if (!size) {
                        return size.error();
                    }
                    auto n_bit_type = Storages{
                        .length = *varint(1),
                        .storages = {Storage{.type = StorageType::UINT}},
                    };
                    n_bit_type.storages[0].size(*varint(size.value()));
                    auto n_bit_type_ref = m.get_storage_ref(
                        n_bit_type,
                        nullptr);
                    if (!n_bit_type_ref) {
                        return n_bit_type_ref.error();
                    }
                    Varint casted;
                    if (n_bit_type_ref->ref.value() != c.type()->ref.value()) {
                        auto cast_type = get_cast_type(n_bit_type, detailed_type);
                        BM_CAST(op, cast, *n_bit_type_ref, c.type().value(), prop_input, cast_type);
                        casted = cast;
                    }
                    else {
                        casted = prop_input;
                    }

                    // assert casted <= max(n bit)
                    BM_BINARY(op, asset_expr, BinaryOp::less_or_eq, casted, mask);
                    op(AbstractOp::ASSERT, [&](Code& c) {
                        c.ref(asset_expr);
                        c.belong(setter_id);
                    });

                    // target = (target & ~(max(n bit) << offset)) | ((casted & max(n bit)) << offset)
                    auto mask_not = (~(max_mask << offset)) & ((std::uint64_t(1) << max_offset) - 1);
                    BM_IMMEDIATE(op, mask_not_id, mask_not);
                    BM_BINARY(op, and_, BinaryOp::bit_and, *target, mask_not_id);
                    BM_BINARY(op, input_masked, BinaryOp::bit_and, casted, mask);
                    BM_BINARY(op, shift, BinaryOp::left_logical_shift, input_masked, shift_offset);

                    BM_ASSIGN(op, assign, *target, shift, null_varint, nullptr);

                    BM_OP(op, AbstractOp::RET_PROPERTY_SETTER_OK);

                    BM_OP(op, AbstractOp::END_FUNCTION);
                }
            }
        }
        m.code.insert_range(m.code.end(), std::move(new_code));
        return none;
    }
}  // namespace rebgn
