/*license*/
#pragma once

#define BM_ERROR_WRAP(name, handle_error, expr)                                                  \
    auto tmp__##name##__ = expr;                                                                 \
    if (!tmp__##name##__) {                                                                      \
        return handle_error("{} at {}:{}", tmp__##name##__.error().error(), __FILE__, __LINE__); \
    }                                                                                            \
    auto name = *tmp__##name##__

#define BM_ERROR_WRAP_ERROR(name, handle_error, expr)                                   \
    auto err__##name##__ = expr;                                                        \
    if (err__##name##__) {                                                              \
        return handle_error("{} at {}:{}" tmp__##name##__.error(), __FILE__, __LINE__); \
    }                                                                                   \
    auto name = *tmp__##name##__

#define BM_NEW_ID(name, handle_error) \
    BM_ERROR_WRAP(name, handle_error, (m.new_id(nullptr)))

#define BM_NEW_NODE_ID(name, handle_error, node) \
    BM_ERROR_WRAP(name, handle_error, (m.new_node_id(node)))

#define BM_BINARY_BASE(op, id_name, bin_op, left, right, new_id, ...) \
    new_id(id_name __VA_OPT__(, )##__VA_ARGS__);                      \
    op(AbstractOp::BINARY, [&](Code& c) {                             \
        c.ident(id_name);                                             \
        c.bop(bin_op);                                                \
        c.left_ref(left);                                             \
        c.right_ref(right);                                           \
    })

#define BM_BINARY(op, id_name, bin_op, left, right) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_ID, error)

#define BM_BINARY_NODE(op, id_name, bin_op, left, right, node) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_NODE_ID, error, node)

#define BM_BINARY_UNEXPECTED(op, id_name, bin_op, left, right) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_ID, unexpect_error)

#define BM_UNARY(op, id_name, un_op, ref_) \
    BM_NEW_ID(id_name, error);             \
    op(AbstractOp::UNARY, [&](Code& c) {   \
        c.ident(id_name);                  \
        c.uop(un_op);                      \
        c.ref(ref_);                       \
    })

#define BM_GET_EXPR(target, m, node) \
    BM_ERROR_WRAP(target, error, (get_expr(m, node)))

#define BM_LOOKUP_IDENT(ident, m, node) \
    BM_ERROR_WRAP(ident, error, (m.lookup_ident(node)))

#define BM_DEFINE_STORAGE(ident, m, type_node, ...) \
    BM_ERROR_WRAP(ident, error, (define_storage(m, type_node __VA_OPT__(, )##__VA_ARGS__)))

#define BM_NEW_OBJECT(op, id_name, type_)     \
    BM_NEW_ID(id_name, error);                \
    op(AbstractOp::NEW_OBJECT, [&](Code& c) { \
        c.ident(id_name);                     \
        c.type(type_);                        \
    })

#define BM_INDEX(op, id_name, array, index) \
    BM_NEW_ID(id_name, error);              \
    op(AbstractOp::INDEX, [&](Code& c) {    \
        c.ident(id_name);                   \
        c.left_ref(array);                  \
        c.right_ref(index);                 \
    })

#define BM_IMMEDIATE(op, id_name, value) \
    BM_ERROR_WRAP(id_name, error, (immediate(m, op, value)))