/*license*/
#pragma once
#include "convert.hpp"
#include <core/ast/traverse.h>
#include <format>

namespace rebgn {

    constexpr auto none = Error();

    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n);

    Error define_IntLiteral(Module& m, std::shared_ptr<ast::IntLiteral>& node) {
        auto n = node->parse_as<std::uint64_t>();
        if (!n) {
            return error("Invalid integer literal: {}", node->value);
        }
        m.op(AbstractOp::IMMEDIATE_INT, [&](auto& c) {
            c.int_value(*n);
        });
        return none;
    }

    Error define_storage(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ) {
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
        if (auto i = ast::as<ast::IntType>(typ)) {
            return typ_with_size(i->is_signed ? StorageType::INT : StorageType::UINT, i);
        }
        if (auto f = ast::as<ast::FloatType>(typ)) {
            return typ_with_size(StorageType::FLOAT, f);
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return define_storage(m, s, base_type);
        }
        if (auto i = ast::as<ast::StructType>(typ)) {
            auto l = i->base.lock();
            if (auto member = ast::as<ast::Member>(l)) {
                auto ident = m.lookup_ident(member->ident);
                if (!ident) {
                    return ident.error();
                }
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(*ident);
                });
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
            return define_storage(m, s, base_type);
        }
        if (auto su = ast::as<ast::StructUnionType>(typ)) {
            auto ident = m.new_id();
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_UNION, [&](Code& c) {
                c.ident(*ident);
            });
            auto size = su->structs.size();
            push(StorageType::VARIANT, [&](Storage& c) {
                c.size(*varint(size));
            });
            for (auto& st : su->structs) {
                auto ident = m.new_id();
                if (!ident) {
                    return ident.error();
                }
                m.op(AbstractOp::DEFINE_UNION_MEMBER, [&](Code& c) {
                    c.ident(*ident);
                });
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(*ident);
                });
                m.op(AbstractOp::END_UNION_MEMBER);
            }
            m.op(AbstractOp::END_UNION);
            return none;
        }
        return error("unsupported type: {}", node_type_to_string(typ->node_type));
    }

    Error define_Field(Module& m, std::shared_ptr<ast::Field>& node) {
        auto str_ref = m.lookup_ident(node->ident);
        if (!str_ref) {
            return str_ref.error();
        }
        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FIELD, [&](auto& c) {
            c.ident(*ident);
            c.str_ref(*str_ref);
        });
        m.op(AbstractOp::END_FIELD);
    }

    Error define_Format(Module& m, std::shared_ptr<ast::Format>& node) {
        auto str_ref = m.lookup_ident(node->ident);
        if (!str_ref) {
            return str_ref.error();
        }
        auto ident = m.new_id();
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FORMAT, [&](auto& c) {
            c.str_ref(*str_ref);
            c.ident(*ident);
        });
        for (auto& f : node->body->elements) {
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FORMAT);
        return none;
    }

    Error define_Binary(Module& m, std::shared_ptr<ast::Binary>& node) {
        if (node->op == ast::BinaryOp::logical_and ||
            node->op == ast::BinaryOp::logical_or) {
            auto err = convert_node_definition(m, node->left);
            if (err) {
                return err;
            }
        }
    }

    Error define_Metadata(Module& m, std::shared_ptr<ast::Metadata>& node) {
        node->values;
    }

    Error define_Program(Module& m, std::shared_ptr<ast::Program>& node) {
        for (auto& n : node->elements) {
            auto err = convert_node_definition(m, n);
            if (err) {
                return err;
            }
        }
    }

#define SWITCH_CASE \
    SWITCH          \
    CASE(Program)   \
    CASE(Metadata)  \
    CASE(Binary)    \
    CASE(Format)
#define SWITCH   \
    if (false) { \
    }
#define CASE(U)                                     \
    else if constexpr (std::is_same_v<T, ast::U>) { \
        return define_##U(m, node);                 \
    }
    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n) {
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::param_at<0>;
            SWITCH_CASE
        });
    }

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node) {
        Module m;
        auto err = convert_node_definition(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        return m;
    }
}  // namespace rebgn
