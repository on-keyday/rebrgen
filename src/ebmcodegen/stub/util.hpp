/*license*/
#pragma once
#include <optional>
#include <string>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/common.hpp>
#include <string_view>
#include <type_traits>
#include "core/sequencer.h"
#include "helper/template_instance.h"
#include "output.hpp"
#include "ebmgen/mapping.hpp"
#include "comb2/composite/range.h"
#include <unordered_set>

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
            return expr.to_string();
        }
        return ebmgen::unexpect_error("unsupported size: {}", to_string(s.unit));
    }

    auto as_IDENTIFIER(auto&& visitor, ebm::StatementRef stmt) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::IDENTIFIER;
        expr.body.id(stmt);
        return visit_Expression(visitor, expr);
    }

    auto as_DEFAULT_VALUE(auto&& visitor, ebm::TypeRef typ) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::DEFAULT_VALUE;
        expr.body.type = typ;
        return visit_Expression(visitor, expr);
    }

    struct DefaultValueOption {
        std::string_view object_init = "{}";
        std::string_view vector_init = "[]";
        std::string_view bytes_init;
        std::string_view pointer_init = "nullptr";
        std::string_view optional_init = "std::nullopt";
        std::string_view encoder_return_init = "{}";
        std::string_view decoder_return_init = "{}";
    };

    enum class BytesType {
        both,
        array,
        vector,
    };

    std::optional<BytesType> is_bytes_type(auto&& visitor, ebm::TypeRef type_ref, BytesType candidate = BytesType::both) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        auto type = module_.get_type(type_ref);
        if (!type) {
            return std::nullopt;
        }
        std::optional<BytesType> is_candidate;
        if (candidate == BytesType::both || candidate == BytesType::vector) {
            if (type->body.kind == ebm::TypeKind::VECTOR) {
                is_candidate = BytesType::vector;
            }
        }
        if (candidate == BytesType::both || candidate == BytesType::array) {
            if (type->body.kind == ebm::TypeKind::ARRAY) {
                is_candidate = BytesType::array;
            }
        }
        if (is_candidate) {
            auto elem_type_ref = type->body.element_type();
            if (!elem_type_ref) {
                return std::nullopt;
            }
            auto elem_type = module_.get_type(*elem_type_ref);
            if (elem_type && elem_type->body.kind == ebm::TypeKind::UINT && elem_type->body.size() && elem_type->body.size()->value() == 8) {
                return is_candidate;
            }
        }
        return std::nullopt;
    }

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
                return val.to_string();
            }
            case ebm::TypeKind::BOOL: {
                ebm::Expression bool_literal;
                bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
                bool_literal.body.bool_value(0);
                MAYBE(val, visit_Expression(visitor, bool_literal));
                return val.to_string();
            }
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::STRUCT:
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                MAYBE(ident, visit_Type(visitor, ref));
                return std::format("{}{}", ident.to_string(), option.object_init);
            }
            case ebm::TypeKind::ARRAY:
            case ebm::TypeKind::VECTOR: {
                MAYBE(elem_type_ref, type.body.element_type());
                MAYBE(elem_type, module_.get_type(elem_type_ref));
                if (elem_type.body.kind == ebm::TypeKind::UINT && elem_type.body.size() && elem_type.body.size()->value() == 8) {
                    if (option.bytes_init.size()) {
                        return std::format("{}", option.bytes_init);
                    }
                    // use default vector init
                }
                return std::format("{}", option.vector_init);
            }
            case ebm::TypeKind::PTR: {
                return std::string(option.pointer_init);
            }
            case ebm::TypeKind::OPTIONAL: {
                return std::string(option.optional_init);
            }
            case ebm::TypeKind::ENCODER_RETURN: {
                return std::string(option.encoder_return_init);
            }
            case ebm::TypeKind::DECODER_RETURN: {
                return std::string(option.decoder_return_init);
            }
            default: {
                return ebmgen::unexpect_error("unsupported default: {}", to_string(type.body.kind));
            }
        }
    }

    ebmgen::expected<std::string> get_bool_literal(auto&& visitor, bool v) {
        ebm::Expression bool_literal;
        bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
        bool_literal.body.bool_value(v ? 1 : 0);
        MAYBE(expr, visit_Expression(visitor, bool_literal));
        return expr.to_string();
    }

    enum class LayerState {
        root,
        as_type,
        as_expr,
    };

    ebmgen::expected<std::vector<std::pair<ebm::StatementKind, std::string>>> get_identifier_layer(auto&& visitor, ebm::StatementRef stmt, LayerState state = LayerState::root) {
        MAYBE(statement, visitor.module_.get_statement(stmt));
        if (state == LayerState::root) {
            if (statement.body.kind == ebm::StatementKind::STRUCT_DECL ||
                statement.body.kind == ebm::StatementKind::ENUM_DECL) {
                state = LayerState::as_type;
            }
            else {
                state = LayerState::as_expr;
            }
        }
        auto ident = visitor.module_.get_associated_identifier(stmt);
        std::vector<std::pair<ebm::StatementKind, std::string>> layers;
        if (const ebm::StructDecl* decl = statement.body.struct_decl()) {
            if (auto related_varint = decl->related_variant()) {
                MAYBE(type, visitor.module_.get_type(*related_varint));
                MAYBE(upper_field, type.body.related_field());
                MAYBE(upper_layers, get_identifier_layer(visitor, upper_field, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
                if (state == LayerState::as_expr) {
                    return layers;
                }
            }
        }
        if (const ebm::FieldDecl* field = statement.body.field_decl()) {
            if (!ebmgen::is_nil(field->parent_struct)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, field->parent_struct, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        if (const ebm::PropertyDecl* prop = statement.body.property_decl()) {
            if (!ebmgen::is_nil(prop->parent_format)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, prop->parent_format, state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        layers.emplace_back(statement.body.kind, ident);
        return layers;
    }

    auto struct_union_members(auto&& visitor, ebm::TypeRef variant) -> ebmgen::expected<std::vector<std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>>> {
        const ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>> result;
        if (type.body.kind == ebm::TypeKind::VARIANT) {
            if (!ebmgen::is_nil(*type.body.related_field())) {
                auto& members = *type.body.members();
                for (auto& mem : members.container) {
                    MAYBE(member_type, module_.get_type(mem));
                    MAYBE(stmt_id, member_type.body.id());
                    MAYBE(stmt_str, visit_Statement(visitor, stmt_id));
                    result.push_back(std::move(stmt_str));
                }
            }
        }
        return result;
    }

    std::string join(auto&& joint, auto&& vec) {
        std::string result;
        bool first = true;
        for (auto&& v : vec) {
            if (!first) {
                result += joint;
            }
            result += v;
            first = false;
        }
        return result;
    }

    void merge_result(auto&& visitor, auto& w, auto&& result) {
        w.merge(std::move(result.to_writer()));
    }

    void convert_location_info(auto&& visitor, auto&& loc_writer) {
        auto& m = visitor.module_;
        for (auto& loc : loc_writer.locs_data()) {
            const ebm::Loc* l = m.get_debug_loc(loc.loc);
            if (!l) {
                continue;
            }
            visitor.output.line_maps.push_back(brgen::ast::LineMap{

                .loc = brgen::lexer::Loc{
                    .pos = {
                        .begin = static_cast<size_t>(l->start.value()),
                        .end = static_cast<size_t>(l->end.value()),
                    },
                    .file = static_cast<size_t>(l->file_id.value()),
                    .line = static_cast<size_t>(l->line.value()),
                    .col = static_cast<size_t>(l->column.value()),
                },
                .line = loc.start.line,
            });
        }
    }

    bool is_u8(auto&& visitor, ebm::TypeRef type_ref) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        if (auto type = module_.get_type(type_ref)) {
            if (type->body.kind == ebm::TypeKind::UINT) {
                if (auto size = type->body.size()) {
                    if (size->value() == 8) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    ebmgen::expected<void> handle_fields(auto&& visitor, auto&& fields, bool recurse_composite, auto&& callback) {
        ebmgen::expected<void> result = {};
        for (auto& field_ref : fields.container) {
            MAYBE(field, visitor.module_.get_statement(field_ref));
            if (auto comp = field.body.composite_field_decl()) {
                if (recurse_composite) {
                    MAYBE_VOID(res, handle_fields(visitor, comp->fields, recurse_composite, callback));
                    continue;
                }
            }
            result = callback(field_ref, field);
            if (!result) {
                return ebmgen::unexpect_error(std::move(result.error()));
            }
        }
        return result;
    }

    ebmgen::expected<std::pair<std::string, std::string>> first_enum_name(auto&& visitor, ebm::TypeRef enum_type) {
        const ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(type, module_.get_type(enum_type));
        if (auto enum_id = type.body.id()) {
            MAYBE(enum_decl, module_.get_statement(*enum_id));
            if (auto enum_ = enum_decl.body.enum_decl()) {
                return std::make_pair(module_.get_associated_identifier(*enum_id), module_.get_associated_identifier(enum_->members.container[0]));
            }
        }
        return ebmgen::unexpect_error("enum type has no members");
    }

    void modify_keyword_identifier(ebm::ExtendedBinaryModule& m, std::unordered_set<std::string_view> keyword_list, auto&& change_rule) {
        for (auto& ident : m.identifiers) {
            if (keyword_list.contains(ident.body.data)) {
                ident.body.data = change_rule(ident.body.data);
                break;
            }
        }
    }

    ebmgen::expected<size_t> get_variant_index(auto&& visitor, ebm::TypeRef variant_type, ebm::TypeRef candidate_type) {
        auto& module_ = visitor.module_;
        MAYBE(type, module_.get_type(variant_type));
        if (type.body.kind != ebm::TypeKind::VARIANT) {
            return ebmgen::unexpect_error("not a variant type");
        }
        MAYBE(members, type.body.members());
        for (size_t i = 0; i < members.container.size(); ++i) {
            if (ebmgen::get_id(members.container[i]) == ebmgen::get_id(candidate_type)) {
                return i;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

    ebmgen::expected<ebm::TypeRef> get_variant_member_from_field(auto&& visitor, ebm::StatementRef field_ref) {
        ebmgen::MappingTable& module_ = visitor.module_;
        MAYBE(field_stmt, module_.get_statement(field_ref));
        MAYBE(field_decl, field_stmt.body.field_decl());
        MAYBE(parent_struct, module_.get_statement(field_decl.parent_struct));
        MAYBE(struct_decl, parent_struct.body.struct_decl());
        MAYBE(rel_var, struct_decl.related_variant());
        MAYBE(type, module_.get_type(rel_var));
        if (type.body.kind != ebm::TypeKind::VARIANT) {
            return ebmgen::unexpect_error("not a variant type");
        }
        MAYBE(members, type.body.members());
        for (auto& member_type_ref : members.container) {
            MAYBE(member_type, module_.get_type(member_type_ref));
            MAYBE(member_stmt_id, member_type.body.id());
            if (ebmgen::get_id(member_stmt_id) == ebmgen::get_id(field_decl.parent_struct)) {
                return member_type_ref;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

    template <class CodeWriter>
    auto code_write(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        CodeWriter w;
        w.write(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_write(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        CodeWriter w;
        w.write_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        CodeWriter w;
        w.writeln(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        CodeWriter w;
        w.writeln_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(auto&& joint, auto&& vec) {
        CodeWriter w;
        bool first = true;
        using T = std::decay_t<decltype(vec)>;
        for (auto&& v : vec) {
            if (!first) {
                w.write(joint);
            }
            w.write(v);
            first = false;
        }
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(auto&& joint, size_t count, auto&& func) {
        CodeWriter w;
        bool first = true;
        for (size_t i = 0; i < count; ++i) {
            if (!first) {
                w.write(joint);
            }
            if constexpr (std::is_invocable_v<decltype(func), size_t>) {
                w.write(func(i));
            }
            else {
                w.write(func);
            }
            first = false;
        }
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(ebm::AnyRef loc, auto&&... vec) {
        CodeWriter w;
        auto loc_scope = w.with_loc_scope(loc);
        w.write(code_joint_write<CodeWriter>(std::forward<decltype(vec)>(vec)...));
        return w;
    }

#define CODE(...) (code_write<CodeWriter>(__VA_ARGS__))
#define CODELINE(...) (code_writeln<CodeWriter>(__VA_ARGS__))

#define SEPARATED(...) (code_joint_write<CodeWriter>(__VA_ARGS__))

    namespace internal {
        constexpr bool is_c_ident(const char* ident) {
            auto seq = futils::make_ref_seq(ident);
            auto res = futils::comb2::composite::c_ident(seq, 0, 0);
            return res == futils::comb2::Status::match && seq.eos();
        }

        constexpr auto force_wrap_expected(auto&& value) {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>> || futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, futils::helper::either::expected>) {
                return value;
            }
            else {
                return ebmgen::expected<std::decay_t<decltype(value)>>{std::forward<decltype(value)>(value)};
            }
        }
    }  // namespace internal
}  // namespace ebmcodegen::util
