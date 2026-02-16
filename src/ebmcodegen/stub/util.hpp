/*license*/
#pragma once
#include <optional>
#include <string>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/common.hpp>
#include <string_view>
#include <type_traits>
#include "code/loc_writer.h"
#include "core/sequencer.h"
#include "ebmgen/access.hpp"
#include "helper/template_instance.h"
#include "output.hpp"
#include "ebmgen/mapping.hpp"
#include "comb2/composite/range.h"
#include <unordered_set>
#include <vector>

namespace ebmcodegen::util {
    using CodeWriter = futils::code::LocWriter<std::string, std::vector, ebm::AnyRef>;

    template <typename T>
    concept has_get_visitor_from_context = requires(T ctx) {
        { get_visitor_arg_from_context(ctx) };
    };

    auto& get_visitor(auto&& ctx) {
        if constexpr (has_get_visitor_from_context<decltype(ctx)>) {
            return get_visitor_arg_from_context(ctx);
        }
        else {
            return ctx;
        }
    }

    template <class T>
    concept has_to_writer = requires(T t) {
        { t.to_writer() };
    };

    template <class CodeWriter>
    auto code_write(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_write. Please convert them to string");
        static_assert(!(... || has_to_writer<decltype(args)>), "Result type is not supported in code_write. Please convert using to_writer()");
        CodeWriter w;
        w.write(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_write(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_write. Please convert them to string");
        CodeWriter w;
        w.write_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_writeln. Please convert them to string");
        CodeWriter w;
        w.writeln(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_writeln. Please convert them to string");
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

    inline std::string tidy_condition_brace(const std::string& brace) {
        return tidy_condition_brace(std::string(brace));
    }

    ebmgen::expected<CodeWriter> get_size_str(auto&& ctx, const ebm::Size& s) {
        if (auto size = s.size()) {
            return CODE(std::format("{}", size->value()));
        }
        else if (auto ref = s.ref()) {
            MAYBE(expr, visit_Expression(ctx, *ref));
            return expr.to_writer();
        }
        return ebmgen::unexpect_error("unsupported size: {}", to_string(s.unit));
    }

    auto as_IDENTIFIER(auto&& visitor, ebm::StatementRef stmt) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::IDENTIFIER;
        expr.body.id(to_weak(stmt));
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
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
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
        if (candidate == BytesType::both || candidate == BytesType::array) {  // for backward compatibility
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
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(ref));
        switch (type.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::FLOAT: {
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

    enum class LayerState {
        root,
        as_type,
        as_expr,
    };

    ebmgen::expected<std::vector<std::pair<ebm::StatementKind, std::string>>> get_identifier_layer(auto&& ctx, ebm::StatementRef stmt, LayerState state = LayerState::root) {
        auto& visitor = get_visitor(ctx);
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
                MAYBE(desc, type.body.variant_desc());
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(desc.related_field), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
                if (state == LayerState::as_expr) {
                    return layers;
                }
            }
        }
        if (const ebm::FieldDecl* field = statement.body.field_decl()) {
            if (!is_nil(field->parent_struct)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(field->parent_struct), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        if (const ebm::PropertyDecl* prop = statement.body.property_decl()) {
            if (!is_nil(prop->parent_format)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(prop->parent_format), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        layers.emplace_back(statement.body.kind, ident);
        return layers;
    }

    ebmgen::expected<std::string> get_identifier_layer_str(auto&& ctx, ebm::StatementRef stmt, std::string_view joint = "::", LayerState state = LayerState::root) {
        MAYBE(layers, get_identifier_layer(ctx, stmt, state));
        std::vector<std::string> layer_strs;
        for (auto& [kind, name] : layers) {
            layer_strs.push_back(name);
        }
        return join(joint, layer_strs);
    }

    auto get_struct_union_members(auto&& visitor, ebm::TypeRef variant) -> ebmgen::expected<std::vector<ebm::StatementRef>> {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<ebm::StatementRef> result;
        if (type.body.kind == ebm::TypeKind::VARIANT) {
            auto varint_desc = type.body.variant_desc();
            if (!is_nil(varint_desc->related_field)) {
                auto& members = varint_desc->members;
                for (auto& mem : members.container) {
                    MAYBE(member_type, module_.get_type(mem));
                    MAYBE(stmt_id, member_type.body.id());
                    result.push_back(stmt_id.id);
                }
            }
        }
        return result;
    }

    auto struct_union_members(auto&& visitor, ebm::TypeRef variant) -> ebmgen::expected<std::vector<std::pair<ebm::StatementRef, std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>>>> {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<std::pair<ebm::StatementRef, std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>>> result;
        if (type.body.kind == ebm::TypeKind::VARIANT) {
            auto varint_desc = type.body.variant_desc();
            if (!is_nil(varint_desc->related_field)) {
                auto& members = varint_desc->members;
                for (auto& mem : members.container) {
                    MAYBE(member_type, module_.get_type(mem));
                    MAYBE(stmt_id, member_type.body.id());
                    MAYBE(stmt_str, visit_Statement(visitor, from_weak(stmt_id)));
                    result.push_back({stmt_id.id, std::move(stmt_str)});
                }
            }
        }
        return result;
    }

    void convert_location_info(auto&& ctx, auto&& loc_writer) {
        auto& visitor = get_visitor(ctx);
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
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
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

    ebmgen::expected<void> handle_fields(auto&& ctx, const ebm::Block& fields, bool recurse_composite, auto&& callback) {
        auto& visitor = get_visitor(ctx);
        for (auto& field_ref : fields.container) {
            MAYBE(field, visitor.module_.get_statement(field_ref));
            if (auto comp = field.body.composite_field_decl()) {
                if (recurse_composite) {
                    MAYBE_VOID(res, handle_fields(ctx, comp->fields, recurse_composite, callback));
                    continue;
                }
            }
            auto result = callback(field_ref, field);
            if (!result) {
                return ebmgen::unexpect_error(std::move(result.error()));
            }
        }
        return {};
    }

    ebmgen::expected<std::pair<std::string, std::string>> first_enum_name(auto&& visitor, ebm::TypeRef enum_type) {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
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
            }
        }
    }

    ebmgen::expected<size_t> get_variant_index(auto&& visitor, ebm::TypeRef variant_type, ebm::TypeRef candidate_type) {
        auto& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant_type));
        if (type.body.kind != ebm::TypeKind::VARIANT) {
            return ebmgen::unexpect_error("not a variant type");
        }
        MAYBE(desc, type.body.variant_desc());
        for (size_t i = 0; i < desc.members.container.size(); ++i) {
            if (get_id(desc.members.container[i]) == get_id(candidate_type)) {
                return i;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

    ebmgen::expected<ebm::TypeRef> get_variant_member_from_field(auto&& visitor, ebm::StatementRef field_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(field_stmt, module_.get_statement(field_ref));
        MAYBE(field_decl, field_stmt.body.field_decl());
        MAYBE(parent_struct, module_.get_statement(field_decl.parent_struct));
        MAYBE(struct_decl, parent_struct.body.struct_decl());
        MAYBE(rel_var, struct_decl.related_variant());
        MAYBE(type, module_.get_type(rel_var));
        if (type.body.kind != ebm::TypeKind::VARIANT) {
            return ebmgen::unexpect_error("not a variant type");
        }
        MAYBE(desc, type.body.variant_desc());
        for (auto& member_type_ref : desc.members.container) {
            MAYBE(member_type, module_.get_type(member_type_ref));
            MAYBE(member_stmt_id, member_type.body.id());
            if (get_id(member_stmt_id) == get_id(field_decl.parent_struct)) {
                return member_type_ref;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

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

// for class based
#define DEFINE_VISITOR(dummy_name)                                                                                                                     \
    static_assert(std::string_view(__FILE__).contains(#dummy_name "_class.hpp") || std::string_view(__FILE__).contains(#dummy_name "_dsl_class.hpp")); \
    template <>                                                                                                                                        \
    struct CODEGEN_VISITOR(dummy_name) {                                                                                                               \
        ebmgen::expected<CODEGEN_NAMESPACE::Result> visit(CODEGEN_CONTEXT(dummy_name) & ctx);                                                          \
    };                                                                                                                                                 \
                                                                                                                                                       \
    inline ebmgen::expected<CODEGEN_NAMESPACE::Result> CODEGEN_VISITOR(dummy_name)::visit(CODEGEN_CONTEXT(dummy_name) & ctx)

#define DEFINE_VISITOR_CLASS(dummy_name)                                                                                                               \
    static_assert(std::string_view(__FILE__).contains(#dummy_name "_class.hpp") || std::string_view(__FILE__).contains(#dummy_name "_dsl_class.hpp")); \
    template <>                                                                                                                                        \
    struct CODEGEN_VISITOR(dummy_name)

#define DEFINE_VISITOR_FUNCTION(dummy_name) ebmgen::expected<CODEGEN_NAMESPACE::Result> visit(CODEGEN_CONTEXT(dummy_name) & ctx)

    // This is for signaling continue normal processing without error
    constexpr auto pass = futils::helper::either::unexpected{ebmgen::Error(futils::error::Category::lib, 0xba55ba55)};
    constexpr bool is_pass_error(const ebmgen::Error& err) {
        return err.category() == futils::error::Category::lib && err.sub_category() == 0xba55ba55;
    }

    // CALL_OR_PASS macro: call a function that returns expected<T>, if it returns pass error, continue, otherwise return the error or value
#define CALL_OR_PASS(uniq, func_call)                                     \
    auto uniq##_res = (func_call);                                        \
    if (!uniq##_res) {                                                    \
        if (ebmcodegen::util::is_pass_error(uniq##_res.error())) {        \
        }                                                                 \
        else {                                                            \
            return ebmgen::unexpect_error(std::move(uniq##_res.error())); \
        }                                                                 \
    }                                                                     \
    else {                                                                \
        return uniq##_res.value();                                        \
    }

    template <class ResultType>
    struct MainLogicWrapper {
       private:
        ebmgen::expected<ResultType> (*main_logic_)(void*);
        void* arg_;

        template <class F>
        constexpr static ebmgen::expected<ResultType> invoke_main_logic(void* arg) {
            auto* self = static_cast<F*>(arg);
            return (*self)();
        }

       public:
        template <class F>
        constexpr MainLogicWrapper(F&& f)
            : main_logic_(&invoke_main_logic<std::decay_t<F>>), arg_(static_cast<void*>(std::addressof(f))) {}

        constexpr ebmgen::expected<ResultType> operator()() {
            return main_logic_(arg_);
        }
    };

    template <class D>
    concept has_item_id = requires(D d) {
        { d.item_id };
    };

    template <class D, class T>
    concept has_item_id_typed = requires(D d) {
        { d.item_id } -> std::convertible_to<T>;
    };

    template <class D>
    concept has_type = requires(D d) {
        { d.type } -> std::convertible_to<ebm::TypeRef>;
    };

    template <class D>
    struct ContextBase {
       private:
        D& derived() {
            return static_cast<D&>(*this);
        }

        const D& derived() const {
            return static_cast<const D&>(*this);
        }

       public:
        decltype(auto) identifier() const
            requires has_item_id<D>
        {
            return derived().visitor.module_.get_associated_identifier(derived().item_id);
        }

        decltype(auto) identifier(ebm::StatementRef stmt) const {
            return derived().visitor.module_.get_associated_identifier(stmt);
        }

        decltype(auto) identifier(ebm::WeakStatementRef stmt) const {
            return derived().visitor.module_.get_associated_identifier(stmt);
        }

        decltype(auto) identifier(ebm::ExpressionRef expr) const {
            return derived().visitor.module_.get_associated_identifier(expr);
        }

        decltype(auto) identifier(ebm::TypeRef type) const {
            return derived().visitor.module_.get_associated_identifier(type);
        }

        auto& module() const {
            return derived().visitor.module_;
        }

        // alias for visitor
        auto& config() const {
            return derived().visitor;
        }

        auto get_writer() {
            return derived().visitor.wm.get_writer();
        }

        auto add_writer() {
            return derived().visitor.wm.add_writer();
        }

        decltype(auto) get_entry_point() const {
            return derived().visitor.module_.get_entry_point();
        }

        auto& flags() const {
            return derived().visitor.flags;
        }

        auto& output() {
            return derived().visitor.output;
        }

        decltype(auto) visit(ebm::TypeRef type_ref) const {
            return visit_Type(derived(), type_ref);
        }

        decltype(auto) visit(const ebm::Type& type) const {
            return visit_Type(derived(), type);
        }

        decltype(auto) visit(ebm::ExpressionRef expr_ref) const {
            return visit_Expression(derived(), expr_ref);
        }

        decltype(auto) visit(const ebm::Expression& expr) const {
            return visit_Expression(derived(), expr);
        }

        decltype(auto) visit(ebm::StatementRef stmt_ref) const {
            return visit_Statement(derived(), stmt_ref);
        }

        decltype(auto) visit(const ebm::Statement& stmt) const {
            return visit_Statement(derived(), stmt);
        }

        decltype(auto) visit(const ebm::Block& block) const {
            return visit_Block(derived(), block);
        }

        decltype(auto) visit(const ebm::Expressions& exprs) const {
            return visit_Expressions(derived(), exprs);
        }

        decltype(auto) visit(const ebm::Types& types) const {
            return visit_Types(derived(), types);
        }

        decltype(auto) visit() const
            requires has_item_id<D>
        {
            return visit(derived().item_id);
        }

        decltype(auto) get(ebm::TypeRef type_ref) const {
            return derived().module().get_type(type_ref);
        }

        decltype(auto) get(ebm::StatementRef stmt_ref) const {
            return derived().module().get_statement(stmt_ref);
        }

        decltype(auto) get(ebm::WeakStatementRef stmt_ref) const {
            return derived().module().get_statement(stmt_ref);
        }

        decltype(auto) get(ebm::ExpressionRef expr_ref) const {
            return derived().module().get_expression(expr_ref);
        }

        decltype(auto) get(ebm::StringRef string_ref) const {
            return derived().module().get_string_literal(string_ref);
        }

        decltype(auto) get(ebm::IdentifierRef ident_ref) const {
            return derived().module().get_identifier(ident_ref);
        }

        decltype(auto) get_kind(ebm::TypeRef type_ref) const {
            return derived().module().get_type_kind(type_ref);
        }

        decltype(auto) get_kind(ebm::StatementRef stmt_ref) const {
            return derived().module().get_statement_kind(stmt_ref);
        }

        decltype(auto) get_kind(ebm::WeakStatementRef stmt_ref) const {
            return derived().module().get_statement_kind(from_weak(stmt_ref));
        }

        decltype(auto) get_kind(ebm::ExpressionRef expr_ref) const {
            return derived().module().get_expression_kind(expr_ref);
        }

        bool is(ebm::TypeKind kind, ebm::TypeRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::StatementKind kind, ebm::StatementRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::StatementKind kind, ebm::WeakStatementRef ref) const {
            return get_kind(from_weak(ref)) == kind;
        }

        bool is(ebm::ExpressionKind kind, ebm::ExpressionRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::TypeKind kind) const
            requires has_type<D> || has_item_id_typed<D, ebm::TypeRef>
        {
            if constexpr (has_type<D>) {
                return get_kind(derived().type) == kind;
            }
            else {
                return get_kind(derived().item_id) == kind;
            }
        }

        bool is(ebm::StatementKind kind) const
            requires has_item_id_typed<D, ebm::StatementRef>
        {
            return get_kind(derived().item_id) == kind;
        }

        bool is(ebm::ExpressionKind kind) const
            requires has_item_id_typed<D, ebm::ExpressionRef>
        {
            return get_kind(derived().item_id) == kind;
        }

        template <ebmgen::FieldNames<> V>
        decltype(auto) get_field() const
            requires has_item_id<D>
        {
            return ebmgen::access_field<V>(module(), derived().item_id);
        }

        template <ebmgen::FieldNames<> V>
        decltype(auto) get_field(auto&& root) const {
            return ebmgen::access_field<V>(module(), root);
        }

        template <size_t N, ebmgen::FieldNames<N> V>
        decltype(auto) get_field(auto&& root) const {
            return ebmgen::access_field<N, V>(module(), root);
        }

        template <class R = void, class F>
        R get_impl(F&& f) const {
            return get_visitor_impl<R>(derived(), f);
        }

        static constexpr bool is_before_or_after() {
            constexpr std::string_view name = D::context_name;
            return name.ends_with("_before") || name.ends_with("_after");
        }

        template <class R = void>
        ebmgen::expected<R> visit(auto&& visitor, auto&& c) {
            return visit_Object<R>(visitor, std::forward<decltype(c)>(c));
        }
    };

    // template <class D, class V, class R = void>
    // struct TraversalVisitorBase {
    // TODO: because of HasVisitor concept doesn't work correctly in some compilers
    //       when CRTP is involved, we use macro to define TraversalVisitorBase
#define TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(D, V) \
    V & visitor;                                  \
    D(V& v)                                       \
        : visitor(v) {}
#define TRAVERSAL_VISITOR_BASE(D, V, R)                             \
    TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(D, V)                       \
    template <class Ctx>                                            \
    ebmgen::expected<R> visit(Ctx&& ctx) {                          \
        if (ctx.is_before_or_after()) {                             \
            return pass;                                            \
        }                                                           \
        return traverse_children<R>(*this, std::forward<Ctx>(ctx)); \
    }
    //};

    // 0: most outer last index: most inner
    ebmgen::expected<std::vector<const ebm::Type*>> get_type_tree(auto&& ctx, ebm::TypeRef type_ref) {
        ebmgen::MappingTable& m = get_visitor(ctx).module_;
        MAYBE(type, m.get_type(type_ref));
        std::vector<const ebm::Type*> result;
        result.push_back(&type);
        const ebm::Type* current_type = &type;
        while (true) {
            switch (current_type->body.kind) {
                case ebm::TypeKind::PTR: {
                    MAYBE(pointee_type_ref, current_type->body.pointee_type());
                    MAYBE(pointee_type, m.get_type(pointee_type_ref));
                    result.push_back(&pointee_type);
                    current_type = &pointee_type;
                    break;
                }
                case ebm::TypeKind::OPTIONAL: {
                    MAYBE(inner_type_ref, current_type->body.inner_type());
                    MAYBE(inner_type, m.get_type(inner_type_ref));
                    result.push_back(&inner_type);
                    current_type = &inner_type;
                    break;
                }
                case ebm::TypeKind::ARRAY:
                case ebm::TypeKind::VECTOR: {
                    MAYBE(element_type_ref, current_type->body.element_type());
                    MAYBE(element_type, m.get_type(element_type_ref));
                    result.push_back(&element_type);
                    current_type = &element_type;
                    break;
                }
                case ebm::TypeKind::RANGE: {
                    MAYBE(value_type_ref, current_type->body.base_type());
                    MAYBE(value_type, m.get_type(value_type_ref));
                    result.push_back(&value_type);
                    current_type = &value_type;
                    break;
                }
                case ebm::TypeKind::ENUM: {
                    auto base_type_ref = *current_type->body.base_type();
                    if (is_nil(base_type_ref)) {
                        return result;
                    }
                    else {
                        MAYBE(base_type, m.get_type(base_type_ref));
                        result.push_back(&base_type);
                        current_type = &base_type;
                        break;
                    }
                }
                default: {
                    return result;
                }
            }
        }
        return result;
    }

    ebmgen::expected<ebm::StatementRef> parent_struct_to_parent_format(auto&& visitor, ebm::StatementRef struct_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(struct_stmt, module_.get_statement(struct_ref));
        auto p = struct_stmt.body.struct_decl();
        ebm::StatementRef stmt_ref = struct_ref;
        while (p) {
            auto rel = p->related_variant();
            if (!rel) {
                return stmt_ref;
            }
            MAYBE(variant_type, module_.get_type(*rel));
            MAYBE(variant_desc, variant_type.body.variant_desc());
            MAYBE(parent_field_stmt, module_.get_statement(variant_desc.related_field));
            MAYBE(field_decl, parent_field_stmt.body.field_decl());
            stmt_ref = from_weak(field_decl.parent_struct);
            MAYBE(parent_struct_stmt, module_.get_statement(stmt_ref));
            p = parent_struct_stmt.body.struct_decl();
        }
        return ebmgen::unexpect_error("cannot find parent format");
    }

    bool variant_candidate_equal(auto&& visitor, ebm::TypeRef candidate, ebm::TypeRef target) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        if (get_id(candidate) == get_id(target)) {
            return true;
        }
        // if candidate is VARIANT, check its members
        auto maybe_candidate_type = module_.get_type(candidate);
        if (!maybe_candidate_type) {
            return false;
        }
        auto& candidate_type = *maybe_candidate_type;
        if (candidate_type.body.kind != ebm::TypeKind::VARIANT) {
            return false;
        }
        auto candidate_desc = candidate_type.body.variant_desc();
        if (!candidate_desc) {
            return false;
        }
        for (auto& member_ref : candidate_desc->members.container) {
            if (get_id(member_ref) == get_id(target)) {
                return true;
            }
        }
        return false;
    }

    struct UnwrapIndexResult {
        std::vector<ebm::ExpressionRef> index_layers;  // first element is the most outer, last element is the most inner
        std::vector<ebm::ExpressionRef> index;         // first element is the most outer, last element is the most inner
        ebm::ExpressionRef base_expression;
    };

    // first element is the most outer, last element is the most inner
    ebmgen::expected<UnwrapIndexResult> unwrap_index(auto&& visitor, ebm::ExpressionRef expr_ref) {
        UnwrapIndexResult result;
        result.base_expression = expr_ref;
        auto do_unwrap = [&](auto&& self, ebm::ExpressionRef ref) -> ebmgen::expected<void> {
            result.index_layers.push_back(ref);
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(expr, module_.get_expression(ref));
            if (expr.body.kind == ebm::ExpressionKind::INDEX_ACCESS) {
                MAYBE(base_expr_ref, expr.body.base());
                MAYBE(index, expr.body.index());
                result.index.push_back(index);
                return self(self, base_expr_ref);
            }
            else {
                return {};
            }
        };
        MAYBE_VOID(_, do_unwrap(do_unwrap, expr_ref));
        return result;
    }

    struct Metadata {
        const std::string_view name;
        const ebm::Metadata* const data;

        ebmgen::expected<std::string_view> get_string(auto&& visitor, size_t index) const {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            if (!data->values.container.size()) {
                return ebmgen::unexpect_error("metadata {} has no values", name);
            }
            auto expr = module_.get_expression(data->values.container[index]);
            if (!expr) {
                return ebmgen::unexpect_error("metadata {} value expression not found", name);
            }
            auto str_value = expr->body.string_value();
            if (!str_value) {
                return ebmgen::unexpect_error("metadata {} value is not string literal", name);
            }
            MAYBE(string_lit, module_.get_string_literal(*str_value));
            return string_lit.body.data;
        }
    };

    struct MetadataSet {
        std::unordered_map<std::string_view, std::vector<Metadata>> items;

        const std::vector<Metadata>* get(std::string_view name) const {
            auto it = items.find(name);
            if (it != items.end()) {
                return &it->second;
            }
            return nullptr;
        }

        const Metadata* get_first(std::string_view name) const {
            auto it = items.find(name);
            if (it != items.end() && !it->second.empty()) {
                return &it->second[0];
            }
            return nullptr;
        }

        ebmgen::expected<void> try_add_metadata(auto&& visitor, const ebm::Metadata* meta_decl) {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(key_str, module_.get_identifier(meta_decl->name));
            items[key_str.body.data].push_back(Metadata{key_str.body.data, meta_decl});
            return {};
        }

        ebmgen::expected<void> try_add_metadata(auto&& visitor, ebm::StatementRef meta_ref) {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(meta_stmt, module_.get_statement(meta_ref));
            auto meta_decl = meta_stmt.body.metadata();
            if (!meta_decl) {
                return {};
            }
            MAYBE(key_str, module_.get_identifier(meta_decl->name));
            items[key_str.body.data].push_back(Metadata{key_str.body.data, meta_decl});
            return {};
        }
    };

    ebmgen::expected<MetadataSet> get_metadata(auto&& visitor, ebm::StatementRef stmt_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(statement, module_.get_statement(stmt_ref));
        MAYBE(block, statement.body.block());
        MetadataSet result;
        for (auto& meta_ref : block.container) {
            MAYBE_VOID(_, result.try_add_metadata(visitor, meta_ref));
        }
        return result;
    }

    ebmgen::expected<MetadataSet> get_global_metadata(auto&& visitor) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(entry_point, module_.get_entry_point());
        return get_metadata(visitor, entry_point.id);
    }

    constexpr bool is_composite_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::COMPOSITE_GETTER:
            case ebm::FunctionKind::COMPOSITE_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_property_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_GETTER:
            case ebm::FunctionKind::PROPERTY_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_setter_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_SETTER:
            case ebm::FunctionKind::COMPOSITE_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_getter_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_GETTER:
            case ebm::FunctionKind::COMPOSITE_GETTER:
                return true;
            default:
                return false;
        }
    }
}  // namespace ebmcodegen::util
