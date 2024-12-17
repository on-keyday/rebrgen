/*license*/
#pragma once
#include "convert.hpp"
#include <core/ast/traverse.h>
#include <format>

namespace rebgn {

    constexpr auto none = Error();

    expected<Varint> varint(std::uint64_t n) {
        Varint v;
        if (n < 0x80) {
            v.prefix(0);
            v.value(n);
        }
        if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        if (n < 0x200000) {
            v.prefix(2);
            v.value(n);
        }
        if (n < 0x10000000) {
            v.prefix(3);
            v.value(n);
        }
    }

    Error convert_node(Module& m, const std::shared_ptr<ast::Node>& n);

    template <typename... Args>
    Error error(std::format_string<Args...> fmt, Args&&... args) {
        return futils::error::StrError{
            std::format(fmt, args),
        };
    }

    template <typename... Args>
    futils::helper::either::unexpected<Error> unexpect_error(std::format_string<Args...> fmt, Args&&... args) {
        return futils::helper::either::unexpected(error(fmt, args...));
    }

    futils::helper::either::unexpected<Error> unexpect_error(Error&& err) {
        return futils::helper::either::unexpected(std::forward<decltype(err)>(err));
    }

    Error convert_IntLiteral(Module& m, std::shared_ptr<ast::IntLiteral>& node) {
        auto n = node->parse_as<std::uint64_t>();
        if (!n) {
            return error("Invalid integer literal: {}", node->value);
        }
        m.op(AbstractOp::IMMEDIATE_INT, [&](auto& c) {
            c.int_value(*n);
        });
        return none;
    }

    Error convert_storage(Module& m, Storages& s, std::shared_ptr<ast::Type>& typ) {
        auto push = [&](StorageType t, auto& set) {
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
            auto ident = varint(m.lookup_ident(i->ident));
            if (!ident) {
                return ident.error();
            }
            push(StorageType::STRUCT, [&](Storage& c) {
                c.ident(*ident);
            });
            return none;
        }
    }

    Error convert_Field(Module& m, std::shared_ptr<ast::Field>& node) {
        auto ident = varint(m.lookup_ident(node->ident));
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FIELD, [&](auto& c) {
            c.ident(*ident);
        });
        m.op(AbstractOp::END_FIELD);
    }

    Error convert_Format(Module& m, std::shared_ptr<ast::Format>& node) {
        auto ident = varint(m.lookup_ident(node->ident));
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FORMAT, [&](auto& c) {
            c.ident(*ident);
        });
        for (auto& f : node->body->elements) {
            auto err = convert_node(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_FORMAT);
        return none;
    }

    Error convert_Binary(Module& m, std::shared_ptr<ast::Binary>& node) {
        if (node->op == ast::BinaryOp::logical_and ||
            node->op == ast::BinaryOp::logical_or) {
            auto err = convert_node(m, node->left);
            if (err) {
                return err;
            }
        }
    }

    Error convert_Metadata(Module& m, std::shared_ptr<ast::Metadata>& node) {
        node->values;
    }

    Error convert_Program(Module& m, std::shared_ptr<ast::Program>& node) {
        for (auto& n : node->elements) {
            auto err = convert_node(m, n);
            if (err) {
                return err;
            }
        }
    }

    Error convert_node(Module& m, const std::shared_ptr<ast::Node>& n) {
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::param_at<0>;
#define SWITCH   \
    if (false) { \
    }
#define CASE(T)                                       \
    else if constexpr (std::is_same_v<T, ast::T>) {   \
        return convert_##T(m, ast::as<ast::T>(node)); \
    }
            SWITCH
            CASE(Program)
            CASE(Metadata)
            CASE(Binary)
            CASE(Format)
        });
    }

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node) {
        Module m;
        auto err = convert_node(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        return m;
    }
}  // namespace rebgn
