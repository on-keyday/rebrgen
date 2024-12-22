/*license*/
#pragma once
#include "convert.hpp"

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

    expected<Varint> immediate(Module& m, std::uint64_t n);
    expected<Varint> define_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level);
    expected<Varint> define_counter(Module& m, std::uint64_t init);
    Error define_storage(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive = false);
    expected<Varint> get_expr(Module& m, const std::shared_ptr<ast::Expr>& n);

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref);
    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref);

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

}  // namespace rebgn
