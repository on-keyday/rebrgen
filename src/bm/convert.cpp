/*license*/
#include "internal.hpp"
#include <core/ast/traverse.h>
#include <format>
#include <set>
namespace rebgn {

    expected<Varint> get_expr(Module& m, const std::shared_ptr<ast::Expr>& n) {
        m.set_prev_expr(null_id);
        auto err = convert_node_definition(m, n);
        if (err) {
            return unexpect_error(std::move(err));
        }
        return m.get_prev_expr();
    }

    expected<Varint> immediate_bool(Module& m, bool b) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        if (b) {
            m.op(AbstractOp::IMMEDIATE_TRUE, [&](Code& c) {
                c.ident(*ident);
            });
        }
        else {
            m.op(AbstractOp::IMMEDIATE_FALSE, [&](Code& c) {
                c.ident(*ident);
            });
        }
        return ident;
    }

    expected<Varint> immediate_char(Module& m, std::uint64_t c) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        auto val = varint(c);
        if (!val) {
            return val;
        }
        m.op(AbstractOp::IMMEDIATE_CHAR, [&](Code& c) {
            c.int_value(*val);
            c.ident(*ident);
        });
        return ident;
    }

    expected<Varint> immediate(Module& m, std::uint64_t n) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        auto val = varint(n);
        if (!val) {
            m.op(AbstractOp::IMMEDIATE_INT64, [&](Code& c) {
                c.int_value64(n);
                c.ident(*ident);
            });
        }
        else {
            m.op(AbstractOp::IMMEDIATE_INT, [&](Code& c) {
                c.int_value(*val);
                c.ident(*ident);
            });
        }
        return ident;
    }

    expected<Varint> define_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level) {
        auto ident = m.new_id();
        if (!ident) {
            return ident;
        }
        m.op(AbstractOp::DEFINE_VARIABLE, [&](Code& c) {
            c.ident(*ident);
            c.ref(init_ref);
        });
        return ident;
    }

    expected<Varint> define_counter(Module& m, std::uint64_t init) {
        auto init_ref = immediate(m, init);
        if (!init_ref) {
            return init_ref;
        }
        return define_tmp_var(m, *init_ref, ast::ConstantLevel::variable);
    }

    template <>
    Error define<ast::IntLiteral>(Module& m, std::shared_ptr<ast::IntLiteral>& node) {
        auto n = node->parse_as<std::uint64_t>();
        if (!n) {
            return error("Invalid integer literal: {}", node->value);
        }
        auto id = immediate(m, *n);
        if (!id) {
            return id.error();
        }
        m.set_prev_expr(id->value());
        return none;
    }

    template <>
    Error define<ast::Import>(Module& m, std::shared_ptr<ast::Import>& node) {
        auto prog_id = m.new_id();
        if (!prog_id) {
            return prog_id.error();
        }
        m.op(AbstractOp::DEFINE_PROGRAM, [&](Code& c) {
            c.ident(*prog_id);
        });
        m.map_struct(node->import_desc->struct_type, prog_id->value());
        auto err = foreach_node(m, node->import_desc->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_PROGRAM);
        auto import_id = m.new_id();
        if (!import_id) {
            return import_id.error();
        }
        m.op(AbstractOp::IMPORT, [&](Code& c) {
            c.ident(*import_id);
            c.ref(*prog_id);
        });
        m.set_prev_expr(import_id->value());
        return none;
    }

    template <>
    Error define<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error define<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error define<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::Return>(Module& m, std::shared_ptr<ast::Return>& node) {
        if (node->expr) {
            auto val = get_expr(m, node->expr);
            if (!val) {
                return val.error();
            }
            m.op(AbstractOp::RET, [&](Code& c) {
                c.ref(*val);
            });
        }
        else {
            m.op(AbstractOp::RET, [](Code& c) {
                c.ref(*varint(null_id));
            });
        }
        return none;
    }

    template <>
    Error define<ast::IOOperation>(Module& m, std::shared_ptr<ast::IOOperation>& node) {
        switch (node->method) {
            case ast::IOMethod::input_backward: {
                if (node->arguments.size()) {
                    auto arg = get_expr(m, node->arguments[0]);
                    if (!arg) {
                        return arg.error();
                    }
                    m.op(AbstractOp::BACKWARD, [&](Code& c) {
                        c.ref(*arg);
                    });
                }
                else {
                    auto imm = immediate(m, 1);
                    if (!imm) {
                        return imm.error();
                    }
                    m.op(AbstractOp::BACKWARD, [&](Code& c) {
                        c.ref(*imm);
                    });
                }
                return none;
            }
            case ast::IOMethod::input_offset:
            case ast::IOMethod::input_bit_offset: {
                auto new_id = m.new_id();
                if (!new_id) {
                    return new_id.error();
                }
                m.op(node->method == ast::IOMethod::input_offset
                         ? AbstractOp::BYTE_OFFSET
                         : AbstractOp::BIT_OFFSET,
                     [&](Code& c) {
                         c.ident(*new_id);
                     });
                m.set_prev_expr(new_id->value());
                return none;
            }
            case ast::IOMethod::input_get: {
                Storages s;
                auto err = define_storage(m, s, node->expr_type);
                if (err) {
                    return err;
                }
                auto id = m.new_id();
                if (!id) {
                    return id.error();
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*id);
                    c.storage(std::move(s));
                });
                auto tmp_var = define_tmp_var(m, *id, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                err = decode_type(m, node->expr_type, *tmp_var, nullptr, nullptr);
                if (err) {
                    return err;
                }
                m.set_prev_expr(tmp_var->value());
                return none;
            }
            case ast::IOMethod::output_put: {
                if (!node->arguments.size()) {
                    return error("Invalid output_put: no arguments");
                }
                auto arg = get_expr(m, node->arguments[0]);
                if (!arg) {
                    return arg.error();
                }
                auto err = encode_type(m, node->arguments[0]->expr_type, *arg, nullptr);
                if (err) {
                    return err;
                }
                return none;
            }
            default: {
                return error("Unsupported IO operation: {}", to_string(node->method));
            }
        }
    }

    template <>
    Error define<ast::BoolLiteral>(Module& m, std::shared_ptr<ast::BoolLiteral>& node) {
        auto ref = immediate_bool(m, node->value);
        if (!ref) {
            return ref.error();
        }
        m.set_prev_expr(ref->value());
        return none;
    }

    template <>
    Error define<ast::CharLiteral>(Module& m, std::shared_ptr<ast::CharLiteral>& node) {
        auto ref = immediate_char(m, node->code);
        if (!ref) {
            return ref.error();
        }
        m.set_prev_expr(ref->value());
        return none;
    }

    template <>
    Error define<ast::StrLiteral>(Module& m, std::shared_ptr<ast::StrLiteral>& node) {
        auto str_ref = m.lookup_string(node->value);
        if (!str_ref) {
            return str_ref.error();
        }
        m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
            c.ident(*str_ref);
        });
        m.set_prev_expr(str_ref->value());
        return none;
    }

    template <>
    Error define<ast::TypeLiteral>(Module& m, std::shared_ptr<ast::TypeLiteral>& node) {
        Storages s;
        auto err = define_storage(m, s, node->type_literal);
        if (err) {
            return err;
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::IMMEDIATE_TYPE, [&](Code& c) {
            c.ident(*new_id);
            c.storage(std::move(s));
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    template <>
    Error define<ast::Ident>(Module& m, std::shared_ptr<ast::Ident>& node) {
        auto ident = m.lookup_ident(node);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::IDENT_REF, [&](Code& c) {
            c.ref(*ident);
        });
        m.set_prev_expr(ident->value());
        return none;
    }

    template <>
    Error define<ast::Index>(Module& m, std::shared_ptr<ast::Index>& node) {
        auto base = get_expr(m, node->expr);
        if (!base) {
            return error("Invalid index target: {}", base.error().error());
        }
        auto index = get_expr(m, node->index);
        if (!index) {
            return error("Invalid index value: {}", index.error().error());
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::INDEX, [&](Code& c) {
            c.ident(*new_id);
            c.left_ref(*base);
            c.right_ref(*index);
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    template <>
    Error define<ast::MemberAccess>(Module& m, std::shared_ptr<ast::MemberAccess>& node) {
        auto base = get_expr(m, node->target);
        if (!base) {
            return error("Invalid member access target: {}", base.error().error());
        }
        if (node->member->usage == ast::IdentUsage::reference_builtin_fn) {
            if (node->member->ident == "length") {
                auto new_id = m.new_id();
                if (!new_id) {
                    return new_id.error();
                }
                m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                    c.ident(*new_id);
                    c.ref(*base);
                });
                m.set_prev_expr(new_id->value());
                return none;
            }
            return error("Unsupported builtin function: {}", node->member->ident);
        }
        auto ident = m.lookup_ident(node->member);
        if (!ident) {
            return ident.error();
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::ACCESS, [&](Code& c) {
            c.ident(*new_id);
            c.left_ref(*base);
            c.right_ref(*ident);
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    expected<Varint> define_union(Module& m, Varint field_id, const std::shared_ptr<ast::StructUnionType>& su) {
        auto union_ident = m.new_id();
        if (!union_ident) {
            return union_ident;
        }
        m.op(AbstractOp::DEFINE_UNION, [&](Code& c) {
            c.ident(*union_ident);
            c.belong(field_id);
        });
        for (auto& st : su->structs) {
            auto ident = m.new_id();
            if (!ident) {
                return ident;
            }
            m.op(AbstractOp::DEFINE_UNION_MEMBER, [&](Code& c) {
                c.ident(*ident);
                c.belong(*union_ident);
            });
            m.map_struct(st, ident->value());
            for (auto& f : st->fields) {
                auto err = convert_node_definition(m, f);
                if (err) {
                    return unexpect_error(std::move(err));
                }
            }
            m.op(AbstractOp::END_UNION_MEMBER);
        }
        m.op(AbstractOp::END_UNION);
        return union_ident;
    }

    Error define_storage(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive) {
        auto push = [&](StorageType t, auto&& set) {
            Storage c;
            c.type = t;
            set(c);
            s.storages.push_back(c);
            s.length.value(s.length.value() + 1);
        };
        auto typ_with_size = [&](StorageType t, auto&& typ) {
            if (!typ->bit_size) {
                return error("Invalid integer type(maybe bug)");
            }
            auto bit_size = varint(*typ->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            push(t, [&](Storage& c) {
                c.size(*bit_size);
            });
            return none;
        };
        if (auto i = ast::as<ast::BoolType>(typ)) {
            push(StorageType::BOOL, [](Storage& c) {});
            return none;
        }
        if (auto i = ast::as<ast::IntType>(typ)) {
            return typ_with_size(i->is_signed ? StorageType::INT : StorageType::UINT, i);
        }
        if (auto f = ast::as<ast::FloatType>(typ)) {
            return typ_with_size(StorageType::FLOAT, f);
        }
        if (auto f = ast::as<ast::StrLiteralType>(typ)) {
            auto len = f->bit_size;
            if (!len) {
                return error("Invalid string literal type(maybe bug)");
            }
            push(StorageType::ARRAY, [&](Storage& c) {
                c.size(*varint(*len / 8));
            });
            push(StorageType::UINT, [&](Storage& c) {
                c.size(*varint(8));
            });
            auto ref = m.lookup_string(f->strong_ref->value);
            if (!ref) {
                return ref.error();
            }
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(*ref);
            });
            m.op(AbstractOp::SPECIFY_FIXED_VALUE, [&](Code& c) {
                c.ref(*ref);
            });
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return define_storage(m, s, base_type, should_detect_recursive);
        }
        if (auto i = ast::as<ast::StructType>(typ)) {
            auto l = i->base.lock();
            if (auto member = ast::as<ast::Member>(l)) {
                auto ident = m.lookup_ident(member->ident);
                if (!ident) {
                    return ident.error();
                }
                if (auto fmt = ast::as<ast::Format>(member); should_detect_recursive && fmt && fmt->body->struct_type->recursive) {
                    push(StorageType::RECURSIVE_STRUCT_REF, [&](Storage& c) {
                        c.ref(*ident);
                    });
                }
                else {
                    Varint bit_size_plus_1;
                    if (auto fmt = ast::as<ast::Format>(member); fmt && fmt->body->struct_type->bit_size) {
                        auto s = varint(*fmt->body->struct_type->bit_size + 1);
                        if (!s) {
                            return s.error();
                        }
                        bit_size_plus_1 = s.value();
                    }
                    push(StorageType::STRUCT_REF, [&](Storage& c) {
                        c.ref(*ident);
                        c.size(bit_size_plus_1);
                    });
                }
                return none;
            }
            return error("unknown struct type");
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("Invalid enum type(maybe bug)");
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            push(StorageType::ENUM, [&](Storage& c) {
                c.ref(*ident);
            });
            auto base_type = base_enum->base_type;
            if (!base_type) {
                return none;  // this is abstract enum
            }
            return define_storage(m, s, base_type, should_detect_recursive);
        }
        if (auto su = ast::as<ast::StructUnionType>(typ)) {
            auto size = su->structs.size();
            push(StorageType::VARIANT, [&](Storage& c) {
                c.size(*varint(size));
                // c.ref(*union_ident); MUST FILL BY CALLER
            });
            for (auto& st : su->structs) {
                auto ident = m.lookup_struct(st);
                if (ident.value() == null_id) {
                    return error("Invalid struct ident(maybe bug)");
                }
                Varint bit_size_plus_1;
                if (st->bit_size) {
                    auto s = varint(*st->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(ident);
                    c.size(bit_size_plus_1);
                });
            }
            return none;
        }
        if (auto a = ast::as<ast::ArrayType>(typ)) {
            if (a->length_value) {
                push(StorageType::ARRAY, [&](Storage& c) {
                    c.size(*varint(*a->length_value));
                });
            }
            else {
                push(StorageType::VECTOR, [&](Storage& c) {});
                should_detect_recursive = false;
            }
            auto err = define_storage(m, s, a->element_type, should_detect_recursive);
            if (err) {
                return err;
            }
            return none;
        }
        return error("unsupported type on define storage: {}", node_type_to_string(typ->node_type));
    }

    Error handle_union_type(Module& m, const std::shared_ptr<ast::UnionType>& ty, auto&& block) {
        std::optional<Varint> base_cond;
        if (auto cond = ty->cond.lock()) {
            auto cond_ = get_expr(m, cond);
            if (!cond_) {
                return error("Invalid union field condition");
            }
            base_cond = cond_.value();
        }
        std::optional<Varint> prev_cond;
        for (auto& c : ty->candidates) {
            auto cond = c->cond.lock();
            auto field = c->field.lock();
            if (cond && !ast::is_any_range(cond)) {
                auto cond_ = get_expr(m, cond);
                if (!cond_) {
                    return error("Invalid union field condition");
                }
                if (base_cond) {
                    auto new_id = m.new_id();
                    if (!new_id) {
                        return new_id.error();
                    }
                    m.op(AbstractOp::BINARY, [&](Code& c) {
                        c.ident(*new_id);
                        c.bop(BinaryOp::equal);
                        c.left_ref(*base_cond);
                        c.right_ref(*cond_);
                    });
                    cond_ = new_id;
                }
                if (prev_cond) {
                    auto new_id_1 = m.new_id();
                    if (!new_id_1) {
                        return new_id_1.error();
                    }
                    m.op(AbstractOp::UNARY, [&](Code& c) {
                        c.ident(*new_id_1);
                        c.uop(UnaryOp::logical_not);
                        c.ref(*prev_cond);
                    });
                    auto new_id_2 = m.new_id();
                    if (!new_id_2) {
                        return new_id_2.error();
                    }
                    m.op(AbstractOp::BINARY, [&](Code& c) {
                        c.ident(*new_id_2);
                        c.bop(BinaryOp::logical_and);
                        c.left_ref(*new_id_1);
                        c.right_ref(*cond_);
                    });
                    cond_ = new_id_2;
                }
                if (field) {
                    auto err = block(cond_.value(), field);
                    if (err) {
                        return err;
                    }
                }
                prev_cond = cond_.value();
            }
            else {
                auto new_id = m.new_id();
                if (!new_id) {
                    return new_id.error();
                }
                if (prev_cond) {
                    m.op(AbstractOp::UNARY, [&](Code& c) {
                        c.ident(*new_id);
                        c.uop(UnaryOp::logical_not);
                        c.ref(*prev_cond);
                    });
                }
                else {
                    m.op(AbstractOp::IMMEDIATE_TRUE, [&](Code& c) {
                        c.ident(*new_id);
                    });
                }
                if (field) {
                    auto err = block(*new_id, field);
                    if (err) {
                        return err;
                    }
                }
            }
        }
        return none;
    }

    template <>
    Error define<ast::Available>(Module& m, std::shared_ptr<ast::Available>& node) {
        Varint new_id;
        if (auto union_ty = ast::as<ast::UnionType>(node->target->expr_type)) {
            std::optional<Varint> prev_cond;
            auto err = handle_union_type(m, ast::cast_to<ast::UnionType>(node->target->expr_type), [&](Varint cond, std::shared_ptr<ast::Field>& field) {
                if (prev_cond) {
                    auto new_id = m.new_id();
                    if (!new_id) {
                        return new_id.error();
                    }
                    m.op(AbstractOp::BINARY, [&](Code& c) {
                        c.ident(*new_id);
                        c.bop(BinaryOp::logical_or);
                        c.left_ref(*prev_cond);
                        c.right_ref(cond);
                    });
                    prev_cond = *new_id;
                }
                else {
                    prev_cond = cond;
                }
                return none;
            });
            if (err) {
                return err;
            }
            if (!prev_cond) {
                auto imm = immediate_bool(m, false);
                if (!imm) {
                    return imm.error();
                }
                new_id = *imm;
            }
            else {
                new_id = prev_cond.value();
            }
        }
        else {
            auto imm = immediate_bool(m, ast::as<ast::Ident>(node->target) || ast::as<ast::MemberAccess>(node->target));
            if (!imm) {
                return imm.error();
            }
            new_id = *imm;
        }
        auto base_expr = get_expr(m, node->target);
        if (!base_expr) {
            return base_expr.error();
        }
        auto id = m.new_id();
        if (!id) {
            return id.error();
        }
        m.op(AbstractOp::FIELD_AVAILABLE, [&](Code& c) {
            c.ident(*id);
            c.left_ref(*base_expr);
            c.right_ref(new_id);
        });
        m.set_prev_expr(id->value());
        return none;
    }

    Error define_union_field(Module& m, const std::shared_ptr<ast::UnionType>& ty, Varint belong) {
        Param param;
        auto err = handle_union_type(m, ty, [&](Varint cond, auto&& field) {
            auto ident = m.lookup_ident(field->ident);
            if (!ident) {
                return ident.error();
            }
            auto cond_field_id = m.new_id();
            if (!cond_field_id) {
                return cond_field_id.error();
            }
            m.op(AbstractOp::CONDITIONAL_FIELD, [&](Code& c) {
                c.ident(*cond_field_id);
                c.left_ref(cond);
                c.right_ref(*ident);
                c.belong(belong);
            });
            param.expr_refs.push_back(*cond_field_id);
            return none;
        });
        if (err) {
            return err;
        }
        if (ty->common_type) {
            Storages s;
            auto err = define_storage(m, s, ty->common_type, true);
            if (err) {
                return err;
            }
            auto len = varint(param.expr_refs.size());
            if (!len) {
                return len.error();
            }
            param.len_exprs = *len;
            auto ident = m.new_id();
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::MERGED_CONDITIONAL_FIELD, [&](Code& c) {  // all common type
                c.ident(*ident);
                c.storage(std::move(s));
                c.param(std::move(param));
                c.belong(belong);
                c.merge_mode(MergeMode::COMMON_TYPE);
            });
        }
        return none;
    }

    template <>
    Error define<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
        auto cond = get_expr(m, node->cond);
        if (!cond) {
            return cond.error();
        }
        m.op(AbstractOp::ASSERT, [&](Code& c) {
            c.ref(*cond);
        });
        return none;
    }

    template <>
    Error define<ast::ExplicitError>(Module& m, std::shared_ptr<ast::ExplicitError>& node) {
        Param param;
        for (auto& c : node->base->arguments) {
            auto err = get_expr(m, c);
            if (!err) {
                return err.error();
            }
            param.expr_refs.push_back(*err);
        }
        auto len = varint(param.expr_refs.size());
        if (!len) {
            return len.error();
        }
        param.len_exprs = *len;
        m.op(AbstractOp::EXPLICIT_ERROR, [&](Code& c) {
            c.param(std::move(param));
        });
        return none;
    }

    template <>
    Error define<ast::Call>(Module& m, std::shared_ptr<ast::Call>& node) {
        auto callee = get_expr(m, node->callee);
        if (!callee) {
            return callee.error();
        }
        std::vector<Varint> args;
        for (auto& p : node->arguments) {
            auto arg = get_expr(m, p);
            if (!arg) {
                return arg.error();
            }
            args.push_back(arg.value());
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        auto len = varint(args.size());
        if (!len) {
            return len.error();
        }
        Param param;
        param.len_exprs = *len;
        param.expr_refs = std::move(args);
        m.op(AbstractOp::CALL, [&](Code& c) {
            c.ident(*new_id);
            c.ref(*callee);
            c.param(std::move(param));
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    template <>
    Error define<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        // maybe null
        auto parent = m.lookup_struct(node->belong_struct.lock());
        if (auto union_struct = ast::as<ast::StructUnionType>(node->field_type)) {
            auto union_ident_ref = define_union(m, *ident, ast::cast_to<ast::StructUnionType>(node->field_type));
            if (!union_ident_ref) {
                return union_ident_ref.error();
            }
            Storages s;
            auto err = define_storage(m, s, node->field_type, true);
            if (err) {
                return err;
            }
            if (s.storages.size() == 0 || s.storages[0].type != StorageType::VARIANT) {
                return error("Invalid union type(maybe bug)");
            }
            s.storages[0].ref(*union_ident_ref);  // fill union ident
            m.op(AbstractOp::DEFINE_FIELD, [&](Code& c) {
                c.ident(*ident);
                c.belong(parent);
            });
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
            m.op(AbstractOp::END_FIELD);
        }
        else {
            if (!ast::as<ast::UnionType>(node->field_type)) {
                m.op(AbstractOp::DEFINE_FIELD, [&](Code& c) {
                    c.ident(*ident);
                    c.belong(parent);
                });
                Storages s;
                auto err = define_storage(m, s, node->field_type, true);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                    c.storage(std::move(s));
                });
                m.op(AbstractOp::END_FIELD);
            }
            else {
                m.op(AbstractOp::DEFINE_PROPERTY, [&](Code& c) {
                    c.ident(*ident);
                    c.belong(parent);
                });
                auto err = define_union_field(m, ast::cast_to<ast::UnionType>(node->field_type), *ident);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::END_PROPERTY);
            }
        }

        return none;
    }

    template <>
    Error define<ast::Function>(Module& m, std::shared_ptr<ast::Function>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        Varint parent;
        if (auto p = node->belong.lock()) {
            auto parent_ = m.lookup_ident(p->ident);
            if (!parent_) {
                return parent_.error();
            }
            parent = *parent_;
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*ident);
            c.belong(parent);
        });
        bool is_encoder_or_decoder = false;
        if (auto fmt = ast::as<ast::Format>(node->belong.lock())) {
            if (fmt->encode_fn.lock() == node || fmt->decode_fn.lock() == node) {
                is_encoder_or_decoder = true;
            }
        }
        if (node->return_type && !ast::as<ast::VoidType>(node->return_type)) {
            Storages s;
            if (is_encoder_or_decoder) {
                s.storages.push_back(Storage{.type = StorageType::CODER_RETURN});
                s.length.value(1);
            }
            auto err = define_storage(m, s, node->return_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
        }
        else if (is_encoder_or_decoder) {
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(Storages{
                    .length = varint(1).value(),
                    .storages = {Storage{.type = StorageType::CODER_RETURN}},
                });
            });
        }
        for (auto& p : node->parameters) {
            auto param_ident = m.lookup_ident(p->ident);
            if (!param_ident) {
                return param_ident.error();
            }
            Storages s;
            auto err = define_storage(m, s, p->field_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::DEFINE_PARAMETER, [&](Code& c) {
                c.ident(*param_ident);
                c.belong(*ident);
            });
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
            m.op(AbstractOp::END_PARAMETER);
        }
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_FUNCTION);
        return none;
    }

    template <>
    Error define<ast::State>(Module& m, std::shared_ptr<ast::State>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_STATE, [&](Code& c) {
            c.ident(*ident);
        });
        m.map_struct(node->body->struct_type, ident->value());
        for (auto& f : node->body->struct_type->fields) {
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_STATE);
        return none;
    }

    template <>
    Error define<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FORMAT, [&](Code& c) {
            c.ident(*ident);
        });
        m.map_struct(node->body->struct_type, ident->value());
        if (node->body->struct_type->type_map) {
            Storages s;
            auto err = define_storage(m, s, node->body->struct_type->type_map->type_literal, true);
            if (err) {
                return err;
            }
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
        }
        std::vector<std::shared_ptr<ast::Field>> bit_fields;
        std::set<std::shared_ptr<ast::Field>> bit_field_begin, bit_field_end;
        for (auto& f : node->body->struct_type->fields) {
            if (auto field = ast::as<ast::Field>(f)) {
                if (field->bit_alignment != field->eventual_bit_alignment) {
                    bit_fields.push_back(ast::cast_to<ast::Field>(f));
                    continue;
                }
                if (bit_fields.size()) {
                    bit_field_begin.insert(ast::cast_to<ast::Field>(bit_fields.front()));
                    bit_field_end.insert(ast::cast_to<ast::Field>(ast::cast_to<ast::Field>(f)));
                    bit_fields.clear();
                }
            }
        }
        std::vector<Varint> bit_field_ids;
        for (auto& f : node->body->struct_type->fields) {
            if (ast::as<ast::Field>(f)) {
                if (bit_field_begin.contains(ast::cast_to<ast::Field>(f))) {
                    auto new_id = m.new_id();
                    if (!new_id) {
                        return new_id.error();
                    }
                    m.op(AbstractOp::DEFINE_BIT_FIELD, [&](Code& c) {
                        c.ident(*new_id);
                        c.belong(ident.value());
                    });
                    m.map_struct(node->body->struct_type, new_id->value());  // temporary remap
                }
            }
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
            if (ast::as<ast::Field>(f)) {
                if (bit_field_end.contains(ast::cast_to<ast::Field>(f))) {
                    m.op(AbstractOp::END_BIT_FIELD);
                    m.map_struct(node->body->struct_type, ident->value());  // restore
                }
            }
        }
        m.op(AbstractOp::END_FORMAT);
        return none;
    }

    template <>
    Error define<ast::Enum>(Module& m, std::shared_ptr<ast::Enum>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_ENUM, [&](Code& c) {
            c.ident(*ident);
        });
        if (node->base_type) {
            Storages s;
            auto err = define_storage(m, s, node->base_type);
            if (err) {
                return err;
            }
            m.op(AbstractOp::SPECIFY_STORAGE_TYPE, [&](Code& c) {
                c.storage(std::move(s));
            });
        }
        for (auto& me : node->members) {
            auto ident = m.lookup_ident(me->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_ENUM_MEMBER, [&](Code& c) {
                c.ident(*ident);
            });
            auto ref = get_expr(m, me->value);
            if (!ref) {
                return error("Invalid enum member value");
            }
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(*ident);
                c.right_ref(*ref);
            });
            m.op(AbstractOp::END_ENUM_MEMBER);
        }
        m.op(AbstractOp::END_ENUM);
        return none;
    }

    template <>
    Error define<ast::Paren>(Module& m, std::shared_ptr<ast::Paren>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <>
    Error define<ast::Unary>(Module& m, std::shared_ptr<ast::Unary>& node) {
        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        auto target = get_expr(m, node->expr);
        if (!target) {
            return error("Invalid unary expression");
        }
        auto uop = UnaryOp(node->op);
        if (uop == UnaryOp::logical_not) {
            if (!ast::as<ast::BoolType>(node->expr->expr_type)) {
                uop = UnaryOp::bit_not;
            }
        }
        m.op(AbstractOp::UNARY, [&](Code& c) {
            c.ident(*ident);
            c.uop(uop);
            c.ref(*target);
        });
        return none;
    }

    template <>
    Error define<ast::Cond>(Module& m, std::shared_ptr<ast::Cond>& node) {
        Storages s;
        auto err = define_storage(m, s, node->expr_type);
        if (err) {
            return err;
        }
        auto new_id = m.new_id();
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
            c.ident(*new_id);
            c.storage(std::move(s));
        });
        auto tmp_var = define_tmp_var(m, *new_id, ast::ConstantLevel::variable);
        if (!tmp_var) {
            return tmp_var.error();
        }
        auto cond = get_expr(m, node->cond);
        if (!cond) {
            return error("Invalid conditional expression");
        }
        m.op(AbstractOp::IF, [&](Code& c) {
            c.ref(*cond);
        });
        auto then = get_expr(m, node->then);
        if (!then) {
            return error("Invalid then expression");
        }
        m.op(AbstractOp::ASSIGN, [&](Code& c) {
            c.left_ref(*tmp_var);
            c.right_ref(*then);
        });
        m.op(AbstractOp::ELSE);
        auto els = get_expr(m, node->els);
        if (!els) {
            return error("Invalid else expression");
        }
        m.op(AbstractOp::ASSIGN, [&](Code& c) {
            c.left_ref(*tmp_var);
            c.right_ref(*els);
        });
        m.op(AbstractOp::END_IF);
        m.set_prev_expr(tmp_var->value());
        return none;
    }

    template <>
    Error define<ast::Cast>(Module& m, std::shared_ptr<ast::Cast>& node) {
        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        std::vector<Varint> args;
        for (auto& p : node->arguments) {
            auto arg = get_expr(m, p);
            if (!arg) {
                return arg.error();
            }
            args.push_back(arg.value());
        }
        Storages s;
        auto err = define_storage(m, s, node->expr_type);
        if (err) {
            return err;
        }
        Param param;
        param.len_exprs = varint(args.size()).value();
        param.expr_refs = std::move(args);
        m.op(AbstractOp::CALL_CAST, [&](Code& c) {
            c.ident(*ident);
            c.storage(std::move(s));
            c.param(std::move(param));
        });
        m.set_prev_expr(ident->value());
        return none;
    }

    template <>
    Error define<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        if (node->op == ast::BinaryOp::define_assign ||
            node->op == ast::BinaryOp::const_assign) {
            auto ident = ast::cast_to<ast::Ident>(node->left);
            auto ident_ = m.lookup_ident(ident);
            if (!ident_) {
                return ident_.error();
            }
            auto right_ref = get_expr(m, node->right);
            if (!right_ref) {
                return error("Invalid binary expression: {}", right_ref.error().error());
            }
            m.op(AbstractOp::DEFINE_VARIABLE, [&](Code& c) {
                c.ident(*ident_);
                c.ref(*right_ref);
            });
            return none;
        }
        if (node->op == ast::BinaryOp::append_assign) {
            auto idx = ast::cast_to<ast::Index>(node->left);
            if (!idx) {
                return error("Invalid append assign expression");
            }
            auto left_ref = get_expr(m, idx->expr);
            if (!left_ref) {
                return error("Invalid binary expression");
            }
            auto right_ref = get_expr(m, node->right);
            if (!right_ref) {
                return error("Invalid binary expression");
            }
            m.op(AbstractOp::APPEND, [&](Code& c) {
                c.left_ref(*left_ref);
                c.right_ref(*right_ref);
            });
            return none;
        }
        auto left_ref = get_expr(m, node->left);
        if (!left_ref) {
            return error("Invalid binary expression: {}", left_ref.error().error());
        }
        auto right_ref = get_expr(m, node->right);
        if (!right_ref) {
            return error("Invalid binary expression");
        }
        if (node->op == ast::BinaryOp::comma) {  // comma operator
            m.set_prev_expr(right_ref->value());
            return none;
        }
        if (node->op == ast::BinaryOp::assign) {
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(*left_ref);
                c.right_ref(*right_ref);
            });
            return none;
        }

        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        auto bop = BinaryOp(node->op);
        bool should_assign = false;
        switch (bop) {
            case BinaryOp::div_assign:
                bop = BinaryOp::div;
                should_assign = true;
                break;
            case BinaryOp::mul_assign:
                bop = BinaryOp::mul;
                should_assign = true;
                break;
            case BinaryOp::mod_assign:
                bop = BinaryOp::mod;
                should_assign = true;
                break;
            case BinaryOp::add_assign:
                bop = BinaryOp::add;
                should_assign = true;
                break;
            case BinaryOp::sub_assign:
                bop = BinaryOp::sub;
                should_assign = true;
                break;
            case BinaryOp::left_logical_shift_assign:
                bop = BinaryOp::left_logical_shift;
                should_assign = true;
                break;
            case BinaryOp::right_logical_shift_assign:
                bop = BinaryOp::right_logical_shift;
                should_assign = true;
                break;
            case BinaryOp::right_arithmetic_shift_assign:
                bop = BinaryOp::right_arithmetic_shift;
                should_assign = true;
                break;
            case BinaryOp::left_arithmetic_shift_assign:  // TODO(on-keyday): is this needed?
                bop = BinaryOp::left_arithmetic_shift;
                should_assign = true;
                break;
            case BinaryOp::bit_and_assign:
                bop = BinaryOp::bit_and;
                should_assign = true;
                break;
            case BinaryOp::bit_or_assign:
                bop = BinaryOp::bit_or;
                should_assign = true;
                break;
            case BinaryOp::bit_xor_assign:
                bop = BinaryOp::bit_xor;
                should_assign = true;
                break;
            default:
                break;
        }
        m.op(AbstractOp::BINARY, [&](Code& c) {
            c.ident(*ident);
            c.bop(bop);
            c.left_ref(*left_ref);
            c.right_ref(*right_ref);
        });
        if (should_assign) {
            m.op(AbstractOp::ASSIGN, [&](Code& c) {
                c.left_ref(*left_ref);
                c.right_ref(*ident);
            });
        }
        else {
            m.set_prev_expr(ident->value());
        }
        return none;
    }

    template <>
    Error define<ast::Identity>(Module& m, std::shared_ptr<ast::Identity>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <>
    Error define<ast::Metadata>(Module& m, std::shared_ptr<ast::Metadata>& node) {
        auto node_name = m.lookup_string(node->name);
        if (!node_name) {
            return node_name.error();
        }
        std::vector<Varint> values;
        for (auto& value : node->values) {
            auto val = get_expr(m, value);
            if (!val) {
                return val.error();
            }
            values.push_back(*val);
        }
        Metadata md;
        md.name = *node_name;
        md.expr_refs = std::move(values);
        auto length = varint(md.expr_refs.size());
        if (!length) {
            return length.error();
        }
        md.len_exprs = length.value();
        m.op(AbstractOp::METADATA, [&](Code& c) {
            c.metadata(std::move(md));
        });
        return none;
    }

    template <>
    Error define<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        auto pid = m.new_id();
        if (!pid) {
            return pid.error();
        }
        m.op(AbstractOp::DEFINE_PROGRAM, [&](Code& c) {
            c.ident(*pid);
        });
        m.map_struct(node->struct_type, pid->value());
        auto err = foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_PROGRAM);
        return none;
    }

    template <>
    Error define<ast::ImplicitYield>(Module& m, std::shared_ptr<ast::ImplicitYield>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <class T>
    concept has_define = requires(Module& m, std::shared_ptr<T>& n) {
        define<T>(m, n);
    };

    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_define<T>) {
                err = define<T>(m, node);
            }
        });
        return err;
    }

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node) {
        Module m;
        auto err = convert_node_definition(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_encode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_decode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        return m;
    }
}  // namespace rebgn
