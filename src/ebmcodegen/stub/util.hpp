/*license*/
#pragma once
#include <functional>
#include <string>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/common.hpp>
#include "ebmgen/mapping.hpp"

namespace ebmcodegen::util {
    // remove top level brace
    inline std::string tidy_condition_brace(std::string&& brace) {
        if (brace.starts_with("(") && brace.ends_with(")")) {
            // check it is actually a brace
            // for example, we may encounter (1 + 1) + (2 + 2)
            // so ensure that brace is actually (<expr>)
            size_t level = 0;
            for (size_t i = 0; i < brace.size(); ++i) {
                if (brace[i] == '(') {
                    ++level;
                }
                else if (brace[i] == ')') {
                    --level;
                }
                if (level == 0) {
                    if (i == brace.size() - 1) {
                        // the last char is the matching ')'
                        brace = brace.substr(1, brace.size() - 2);
                        return std::move(brace);
                    }
                    break;  // not a top-level brace
                }
            }
        }
        return std::move(brace);
    }

    ebmgen::expected<std::string> get_size_str(auto&& visitor, const ebm::Size& s) {
        if (auto size = s.size()) {
            return std::format("{}", size->value());
        }
        else if (auto ref = s.ref()) {
            MAYBE(expr, visit_Expression(visitor, *ref));
            return expr.value;
        }
        return ebmgen::unexpect_error("unsupported size: {}", to_string(s.unit));
    }

    struct DefaultValueOption {
        std::string_view object_init = "{}";
        std::string_view vector_init = "[]";
    };

    ebmgen::expected<std::string> get_default_value(auto&& visitor, ebm::TypeRef ref, const DefaultValueOption& option = {}) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(type, module_.get_type(ref));
        switch (type.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT: {
                ebm::Expression int_literal;
                int_literal.body.kind = ebm::ExpressionKind::LITERAL_INT;
                int_literal.body.int_value(*ebmgen::varint(0));
                MAYBE(val, visit_Expression(visitor, int_literal));
                return val.value;
            }
            case ebm::TypeKind::BOOL: {
                ebm::Expression bool_literal;
                bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
                bool_literal.body.bool_value(0);
                MAYBE(val, visit_Expression(visitor, bool_literal));
                return val.value;
            }
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::STRUCT:
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                MAYBE(id, type.body.id());
                return std::format("{}{}", module_.get_identifier_or(id), option.object_init);
            }
            case ebm::TypeKind::ARRAY: {
                return std::format("{}", option.vector_init);
            }
            case ebm::TypeKind::VECTOR: {
                return std::format("{}", option.vector_init);
            }
            default: {
                return ebmgen::unexpect_error("unsupported default: {}", to_string(type.body.kind));
            }
        }
    }
}  // namespace ebmcodegen::util
