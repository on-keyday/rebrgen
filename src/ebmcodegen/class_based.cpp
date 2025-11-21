/*license*/
#include "stub/structs.hpp"
#include <format>
#include "stub/hooks.hpp"

namespace ebmcodegen {

    struct ContextClassField {
        std::string_view name;
        std::string type;
        StructField* base = nullptr;
    };

    enum ContextClassKind {
        ContextClassKind_Normal = 0,
        ContextClassKind_Before = 1,
        ContextClassKind_After = 2,
        ContextClassKind_VariantMask = 3,

        ContextClassKind_List = 4,
        ContextClassKind_Generic = 8,  // like Statement, Expression, Type
    };

    std::string upper(std::string_view s) {
        std::string result;
        for (auto c : s) {
            result += std::toupper(c);
        }
        return result;
    }

    struct ContextClass {
        std::string_view base;     // like Statement, Expression, Type
        std::string_view variant;  // like BLOCK, ASSIGNMENT, etc
        ContextClassKind kind = ContextClassKind_Normal;
        std::vector<std::string_view> type_params;
        std::vector<ContextClassField> fields;

        std::string body_name() const {
            return std::string(base) + "Body";
        }

        std::string ref_name() const {
            return std::string(base) + "Ref";
        }

        std::string type_parameters_body() const {
            std::string result;
            for (size_t i = 0; i < type_params.size(); i++) {
                if (i != 0) {
                    result += ",";
                }
                result += "typename " + std::string(type_params[i]);
            }
            return result;
        }

        std::string type_parameters() const {
            if (type_params.size() == 0) {
                return "";
            }
            std::string result = "template <";
            result += type_parameters_body();
            result += ">";
            return result;
        }

        bool has(ContextClassKind k) const {
            return (kind & k) != 0;
        }

        ContextClassKind is_before_after() const {
            return ContextClassKind(kind & ContextClassKind_VariantMask);
        }

        std::string type_param_arguments() const {
            std::string result;
            for (size_t i = 0; i < type_params.size(); i++) {
                if (i != 0) {
                    result += ",";
                }
                result += std::string(type_params[i]);
            }
            return result;
        }

        std::string class_name_with_type_params() const {
            std::string name = class_name();
            if (type_params.size()) {
                name += "<";
                name += type_param_arguments();
                name += ">";
            }
            return name;
        }

        std::string class_name() const {
            std::string name;
            if (kind & ContextClassKind_List) {
                if (base == "Statement") {
                    name = "Block";
                }
                else {
                    name = std::string(base) + "s";
                }
            }
            else {
                name = std::string(base);
            }
            if (variant.size()) {
                name += "_";
                name += variant;
            }
            if (kind & ContextClassKind_Before) {
                name += "_before";
            }
            else if (kind & ContextClassKind_After) {
                name += "_after";
            }
            return name;
        }

        std::string upper_class_name() const {
            return upper(class_name());
        }
    };

    struct ContextClasses {
        ContextClass classes[3];  // main, before, after
        ContextClass& main() {
            return classes[0];
        }

        ContextClass& before() {
            return classes[1];
        }

        ContextClass& after() {
            return classes[2];
        }

        const ContextClass& main() const {
            return classes[0];
        }

        const ContextClass& before() const {
            return classes[1];
        }

        const ContextClass& after() const {
            return classes[2];
        }
    };

    std::string visitor_tag_name(const ContextClass& cls) {
        return "VisitorTag_" + cls.class_name();
    }

    std::string visitor_requirements_name(const ContextClass& cls) {
        return "has_visitor_" + cls.class_name();
    }

    std::string context_requirements_name(const ContextClass& cls) {
        return "has_context_" + cls.class_name();
    }

    struct HookType {
        std::string_view name;

        std::string visitor_instance_name(const ContextClass& cls, std::string_view ns_name = "") const {
            auto may_with_ns = [&](std::string_view n) {
                if (ns_name.size()) {
                    return std::string(ns_name) + "::" + std::string(n);
                }
                return std::string(n);
            };
            auto visitor_tag = visitor_tag_name(cls);
            auto ns_visitor_tag = may_with_ns(visitor_tag);
            auto hook_type_template = may_with_ns(std::format("{}<{}>", name, ns_visitor_tag));
            return may_with_ns(std::format("Visitor<{}>", hook_type_template));
        }

        std::string visitor_instance_holder_name(const ContextClass& cls) const {
            return std::format("visitor_{}_{}", cls.class_name(), name);
        }
    };

    std::vector<ContextClasses> generate_context_classes(std::map<std::string_view, Struct>& struct_map) {
        std::vector<ContextClasses> context_classes;
        std::string result_type = "expected<Result>";
        auto add_common_visitor = [&](ContextClass& context_class) {
            context_class.type_params.push_back("Visitor");
            context_class.fields.push_back(ContextClassField{.name = "visitor", .type = "Visitor&"});
        };
        auto add_context_variant_for_before_after = [&](ContextClasses& result) {
            auto& base = result.main();
            auto for_before = base;
            for_before.kind = ContextClassKind(base.kind | ContextClassKind_Before);
            for_before.type_params.push_back("MainLogic");
            for_before.fields.push_back(ContextClassField{.name = "main_logic", .type = "MainLogic&"});
            result.before() = std::move(for_before);
            auto for_after = base;
            for_after.kind = ContextClassKind(base.kind | ContextClassKind_After);
            for_after.type_params.push_back("MainLogic");
            for_after.fields.push_back(ContextClassField{.name = "main_logic", .type = "MainLogic&"});
            for_after.fields.push_back(ContextClassField{.name = "result", .type = result_type + "&"});
            result.after() = std::move(for_after);
        };

        auto convert = [&](auto t, std::string_view kind, auto subset) {
            using T = std::decay_t<decltype(t)>;
            ContextClasses per_kind;
            per_kind.main().base = kind;
            per_kind.main().kind = ContextClassKind_Generic;
            add_common_visitor(per_kind.main());
            per_kind.main().fields.push_back(ContextClassField{.name = "in", .type = "ebm::" + std::string(kind)});
            per_kind.main().fields.push_back(ContextClassField{.name = "alias_ref", .type = "ebm::" + std::string(kind) + "Ref"});
            add_context_variant_for_before_after(per_kind);
            context_classes.push_back(std::move(per_kind));
            for (size_t i = 0; to_string(T(i))[0]; i++) {
                ContextClasses per_variant;
                per_variant.main().base = kind;
                per_variant.main().variant = to_string(T(i));
                add_common_visitor(per_variant.main());
                per_variant.main().fields.push_back(ContextClassField{.name = "item_id", .type = "ebm::" + std::string(kind) + "Ref"});
                auto& body = struct_map[std::string(kind) + "Body"];
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    std::string typ;
                    if (field.type.contains("std::") || field.type == "bool") {
                        typ = field.type;
                    }
                    else {
                        typ = std::format("ebm::{}", field.type);
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                        typ = std::format("std::vector<{}>", typ);
                    }
                    typ = "const " + typ + "&";
                    per_variant.main().fields.push_back(ContextClassField{.name = field.name, .type = typ, .base = &field});
                }
                add_context_variant_for_before_after(per_variant);
                context_classes.push_back(std::move(per_variant));
            }
            ContextClasses per_list;
            per_list.main().base = kind;
            per_list.main().kind = ContextClassKind_List;
            add_common_visitor(per_list.main());
            per_list.main().fields.push_back(ContextClassField{.name = "in", .type = "ebm::" + per_list.main().class_name() + "&"});
            add_context_variant_for_before_after(per_list);
            context_classes.push_back(std::move(per_list));
        };
        convert(ebm::StatementKind{}, prefixes[prefix_statement], body_subset_StatementBody());
        convert(ebm::ExpressionKind{}, prefixes[prefix_expression], body_subset_ExpressionBody());
        convert(ebm::TypeKind{}, prefixes[prefix_type], body_subset_TypeBody());
        return context_classes;
    }

    std::vector<std::string_view> make_args_with_injected(const ContextClass& cls, std::vector<std::string_view> injected_args) {
        std::vector<std::string_view> args;
        size_t injected_index = 0;
        for (auto& field : cls.fields) {
            if (!field.base) {
                assert(injected_index < injected_args.size());
                args.push_back(injected_args[injected_index++]);
            }
            else {
                args.push_back(field.name);
            }
        }
        assert(injected_index == injected_args.size());
        return args;
    }

    std::string context_name(const ContextClass& cls) {
        return "Context_" + cls.class_name();
    }

    std::string context_instance_name(const ContextClass& cls) {
        return std::format("{}<{}>", context_name(cls), cls.type_param_arguments());
    }

    void construct_instance(CodeWriter& w, const ContextClass& cls, std::string_view instance_name, std::vector<std::string_view> type_params, std::vector<std::string_view> args) {
        assert(type_params.size() == cls.type_params.size());
        assert(args.size() == cls.fields.size());
        w.write(context_name(cls));
        if (type_params.size() > 0) {
            w.write("<");
            for (size_t i = 0; i < type_params.size(); i++) {
                if (i != 0) {
                    w.write(", ");
                }
                w.write(type_params[i]);
            }
            w.write(">");
        }
        w.writeln(" ", instance_name, "{");
        {
            auto scope = w.indent_scope();
            for (size_t i = 0; i < cls.fields.size(); i++) {
                w.writeln(".", cls.fields[i].name, " = ", args[i], ",");
            }
        }
        w.writeln("};");
    }

    void deconstruct_base_struct(CodeWriter& w, const ContextClass& cls, std::string_view instance_name) {
        for (auto& field : cls.fields) {
            if (!field.base) {
                continue;
            }
            if (field.base->attr & ebmcodegen::TypeAttribute::PTR) {
                w.writeln("if (!in.body.", field.name, "()) {");
                w.indent_writeln("return unexpect_error(\"Unexpected null pointer for ", cls.body_name(), "::", field.name, "\");");
                w.writeln("}");
                w.writeln("auto& ", field.name, " = *in.body.", field.name, "();");
            }
            else {
                w.writeln("auto& ", field.name, " = in.body.", field.name, ";");
            }
        }
    }

    void deconstruct_context_fields_macro(CodeWriter& w, std::string_view ns_name, const ContextClass& cls) {
        w.writeln("// Deconstruct context fields");
        w.writeln("#define ", upper(ns_name), "_DECONSTRUCT_", cls.upper_class_name(), "(instance_name) \\");
        for (auto& field : cls.fields) {
            w.write("auto& ", field.name, " = instance_name.", field.name, ";");
        }
        w.writeln();
    }

    void handle_hijack_logic(CodeWriter& w, std::string_view may_inject) {
        constexpr auto pass_error = "PassError";
        w.writeln("if (!", may_inject, ") {");
        {
            auto if_scope = w.indent_scope();
            w.writeln("if(!", may_inject, ".error().template as<", pass_error, ">()) {");
            {
                auto if_scope2 = w.indent_scope();
                w.writeln("return unexpect_error(std::move(", may_inject, ".error())); // for trace");
            }
            w.writeln("}");
        }
        w.writeln("}");
        w.writeln("else { // if hijacked");
        {
            auto else_scope = w.indent_scope();
            w.writeln("return ", may_inject, ";");
        }
        w.writeln("}");
    }

    void generate_visitor_requirements(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        w.writeln("template<typename VisitorImpl,", cls.type_parameters_body(), ">");
        w.writeln("concept ", visitor_requirements_name(cls), " = requires(VisitorImpl v) {");
        {
            auto requires_scope = w.indent_scope();
            auto instance = context_instance_name(cls);
            w.writeln(" { v.visit(std::declval<", instance, ">()) } -> std::convertible_to<", result_type, ">;");
        }
        w.writeln("};");
    }

    std::string dispatch_fn_name(const ContextClass& cls) {
        return "dispatch_" + cls.class_name();
    }

    void generate_dispatch_fn_signature(CodeWriter& w, const ContextClass& cls, const std::string_view result_type, bool is_decl) {
        auto fn_name = dispatch_fn_name(cls);
        if (cls.has(ContextClassKind_List)) {
            w.writeln("template<typename Context>");
            w.write(result_type, " ", fn_name, "(Context&& ctx,const ebm::", cls.class_name(), "& in)");
            if (is_decl) {
                w.writeln(";");
            }
        }
        else {
            w.writeln("template<typename Context>");
            w.write(result_type, " ", fn_name, "(Context&& ctx,const ebm::", cls.base, "& in,ebm::", cls.ref_name(), " alias_ref");
            if (is_decl) {
                w.writeln(" = {});");
            }
            else {
                w.write(")");
            }
        }
    }

    void generate_dispatcher_function(CodeWriter& hdr, CodeWriter& src, const ContextClasses& clss, const std::string_view result_type) {
        auto& cls = clss.main();
        generate_dispatch_fn_signature(hdr, cls, result_type, true);
        auto visitor_arg = "ctx.visitor";
        auto id_arg = "is_nil(alias_ref) ? in.id : alias_ref";
        auto make_args = [&](const ContextClass& cls, std::vector<std::string_view> additional_args) {
            std::vector<std::string_view> args;
            if (cls.has(ContextClassKind_Generic)) {
                args = {visitor_arg, "in", "alias_ref"};
            }
            else if (cls.has(ContextClassKind_List)) {
                args = {visitor_arg, "in"};
            }
            else {
                args = {visitor_arg, id_arg};
            }
            args.insert(args.end(), additional_args.begin(), additional_args.end());
            return make_args_with_injected(cls, args);
        };
        {
            generate_dispatch_fn_signature(src, cls, result_type, false);
            src.writeln("{");
            {
                auto scope = src.indent_scope();
                deconstruct_base_struct(src, cls, "in");
                src.writeln("auto main_logic = [&]() -> ", result_type, "{");
                {
                    auto main_scope = src.indent_scope();
                    auto args = make_args(cls, {});
                    construct_instance(src, clss.main(), "new_ctx", {"std::decay_t<decltype(ctx.visitor)>"}, args);
                    src.writeln("return ctx.visitor.visit(new_ctx);");
                }
                src.writeln("};");
                auto before_args = make_args(clss.before(), {"main_logic"});
                construct_instance(src, clss.before(), "before_ctx", {"std::decay_t<decltype(ctx.visitor)>", "decltype(main_logic)"}, before_args);
                src.writeln(result_type, " before_result = ctx.visitor.visit(before_ctx);");
                handle_hijack_logic(src, "before_result");
                src.writeln(result_type, " main_result = main_logic();");
                auto after_args = make_args(clss.after(), {"main_logic", "main_result"});
                construct_instance(src, clss.after(), "after_ctx", {"std::decay_t<decltype(ctx.visitor)>", "decltype(main_logic)"}, after_args);
                src.writeln(result_type, " after_result = ctx.visitor.visit(after_ctx);");
                handle_hijack_logic(src, "after_result");
                src.writeln("return main_result;");
            }
            src.writeln("}");
        }
    }

    void generate_context_class(CodeWriter& w, const ContextClass& cls) {
        w.writeln(cls.type_parameters());
        w.writeln("struct ", context_name(cls), " {");
        {
            auto scope = w.indent_scope();
            for (auto& field : cls.fields) {
                w.writeln(field.type, " ", field.name, ";");
            }
        }
        w.writeln("};");
    }

    void generate_visitor_tag(CodeWriter& w, const ContextClass& cls) {
        w.writeln("struct ", visitor_tag_name(cls), " {};");
    }

    void generate_hook_tag(CodeWriter& w, const HookType& hook) {
        w.writeln("template <typename Tag>");
        w.writeln("struct ", hook.name, " {}; // Hook tag");
    }

    void generate_visitor_customization_point(CodeWriter& w) {
        w.writeln("template<typename Tag>");
        w.writeln("struct Visitor; // Customization point struct");
    }

    void generate_pass_error_type(CodeWriter& w) {
        w.writeln("struct PassError { void error(auto&& pb) { futils::strutil::append(pb,\"THIS IS NOT AN ERROR, but a marker for before or after hook that is not needed to be hijacked\"); } };");
        w.writeln("constexpr auto pass = futils::helper::either::unexpected{PassError{}};");
    }

    void hook_with_fallback_single(CodeWriter& w, std::string_view self, const HookType& hook, const ContextClass& cls) {
        auto instance_name = hook.visitor_instance_holder_name(cls);
        if (!self.empty()) {
            instance_name = std::format("{}.{}", self, instance_name);
        }
        auto visitor_requirements = visitor_requirements_name(cls);
        w.writeln("if constexpr (", visitor_requirements, "<decltype(", instance_name, "),", cls.type_param_arguments(), ">) {");
        {
            auto if_scope = w.indent_scope();
            w.writeln("return ", instance_name, ";");
        }
        w.writeln("}");
    }

    std::string get_hook_fn_name(const ContextClass& cls) {
        return "get_visitor_" + cls.class_name();
    }

    void generate_always_false_template(CodeWriter& w) {
        w.writeln("template<typename T>");
        w.writeln("constexpr bool dependent_false = false;");
    }

    void generate_get_hook(CodeWriter& w, const std::vector<HookType>& hooks, const ContextClass& cls) {
        auto fn_name = get_hook_fn_name(cls);
        w.writeln(cls.type_parameters());
        auto arg_type = context_instance_name(cls);
        w.writeln("auto& ", fn_name, "(const ", arg_type, "&) {");
        {
            auto scope = w.indent_scope();
            bool first = true;
            for (auto& hook : hooks) {
                if (!first) {
                    w.write("else ");
                }
                first = false;
                hook_with_fallback_single(w, "", hook, cls);
            }
            w.writeln("else {");
            {
                auto else_scope = w.indent_scope();
                w.writeln("static_assert(dependent_false<", context_instance_name(cls), ">, \"No suitable visitor hook found for ", cls.class_name(), "\");");
            }
            w.writeln("}");
        }
        w.writeln("}");
    }

    void generate_visitor_implementation(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        w.writeln(cls.type_parameters());
        w.writeln(result_type, " visit(const ", context_instance_name(cls), "& ctx) {");
        {
            auto scope = w.indent_scope();
            w.writeln("auto& visitor = ", get_hook_fn_name(cls), "(ctx);");
            w.writeln("return visitor.visit(ctx);");
        }
        w.writeln("}");
    }

    void generate_merged_visitor(CodeWriter& w, const std::vector<HookType>& hooks, const std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        w.writeln("struct MergedVisitor {");
        {
            auto scope = w.indent_scope();
            for (auto& cls_group : context_classes) {
                for (auto& cls : cls_group.classes) {
                    for (auto& hook : hooks) {
                        auto instance_name = hook.visitor_instance_holder_name(cls);
                        w.writeln(hook.visitor_instance_name(cls), " ", instance_name, ";");
                    }
                    generate_get_hook(w, hooks, cls);
                    generate_visitor_implementation(w, cls, result_type);
                }
            }
        }
        w.writeln("};");
    }

    void generate_Result(CodeWriter& w, bool is_codegen) {
        w.writeln();
        w.writeln("struct Result {");
        auto result_scope = w.indent_scope();
        if (is_codegen) {
            w.writeln("private: CodeWriter value;");
            w.writeln("public: Result(std::string v) { value.write(v); }");
            w.writeln("Result(const char* v) { value.write(v); }");
            w.writeln("Result(CodeWriter&& v) : value(std::move(v)) {}");
            w.writeln("Result() = default;");
            w.writeln("constexpr std::string to_string() const {");
            w.indent_writeln("return value.to_string();");
            w.writeln("}");
            w.writeln("constexpr const CodeWriter& to_writer() const {");
            w.indent_writeln("return value;");
            w.writeln("}");
            w.writeln("constexpr CodeWriter& to_writer() {");
            w.indent_writeln("return value;");
            w.writeln("}");
        }
        // insert_include(w, prefixes[prefix_result]);
        result_scope.execute();
        w.writeln("};");
    }

    void generate_namespace_injection(CodeWriter& w) {
        w.writeln("using namespace ebmgen;");
        w.writeln("using namespace ebmcodegen::util;");
        w.writeln("using CodeWriter = futils::code::LocWriter<std::string,std::vector,ebm::AnyRef>;");
    }

    void generate_namespace(CodeWriter& w, std::string_view ns_name, bool open) {
        if (open) {
            w.writeln("namespace ", ns_name, " {");
        }
        else {
            w.writeln("}  // namespace ", ns_name);
        }
    }

    void generate(std::string_view ns_name, CodeWriter& hdr, CodeWriter& src, std::map<std::string_view, Struct>& structs, bool is_codegen) {
        auto result_type = "expected<Result>";
        auto context_classes = generate_context_classes(structs);
        std::vector<HookType> hooks = {
            {.name = "UserHook"},
            {.name = "UserDSLHook"},
            {.name = "DefaultCodegenVisitorHook"},
            {.name = "GeneratorDefaultHook"},  // this is hidden
        };

        generate_namespace(hdr, ns_name, true);
        generate_namespace(src, ns_name, true);
        auto ns_scope_hdr = hdr.indent_scope();
        auto ns_scope_src = src.indent_scope();

        generate_namespace_injection(hdr);
        generate_Result(hdr, is_codegen);
        generate_pass_error_type(hdr);

        for (size_t i = 0; i < hooks.size() - 1; i++) {
            generate_hook_tag(hdr, hooks[i]);
        }
        generate_hook_tag(src, hooks.back());  // hidden hook in source

        generate_visitor_customization_point(hdr);
        generate_always_false_template(src);
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                generate_context_class(hdr, cls);
                generate_visitor_tag(hdr, cls);
                generate_visitor_requirements(hdr, cls, result_type);
                deconstruct_context_fields_macro(hdr, ns_name, cls);
            }
            generate_dispatcher_function(hdr, src, cls_group, result_type);
        }

        generate_merged_visitor(src, hooks, context_classes, result_type);

        ns_scope_hdr.execute();
        ns_scope_src.execute();
        generate_namespace(hdr, ns_name, false);
        generate_namespace(src, ns_name, false);
    }

    /*
    auto make_dispatch_func_name(std::string_view kind, auto t) {
        return std::format("dispatch_{}_{}", kind, to_string(t));
    }

    auto write_visit_entry(auto& w, auto&& result_type, std::string_view kind, bool decl, auto&&... name) {
        w.writeln("template<typename Context>");
        w.write(result_type, " ", name..., "(Context&& ctx,const ebm::", kind, "& in,ebm::", kind, "Ref alias_ref", decl ? " = {}" : "", ")");
    };

    template <class T>
    void common_dispatch(CodeWriter& dispatch_decl, std::string_view kind, std::string_view ref_name, bool is_codegen, std::string_view result_type) {
        {
            // generating dispatcher function
            write_visit_entry(dispatch_decl, result_type, kind, false, "visit_", kind, "_default");
            dispatch_decl.writeln(" {");
            auto scope_ = dispatch_decl.indent_scope();
            dispatch_decl.writeln("switch (in.body.kind) {");
            // generating entry point of dispatch function
            for (size_t i = 0; to_string(T(i))[0]; i++) {
                auto dispatch_func_name = make_dispatch_func_name(kind, T(i));
                dispatch_decl.writeln("case ebm::", visit_enum(T(i)), "::", to_string(T(i)), ":");
                dispatch_decl.indent_writeln("return ", dispatch_func_name, "(ctx,in,alias_ref);");
            }
            dispatch_decl.writeln("default:");
            dispatch_decl.indent_writeln("return unexpect_error(\"Unknown ", kind, " kind: {}\", to_string(in.body.kind));");
            dispatch_decl.writeln("}");
            scope_.execute();
            dispatch_decl.writeln("}");
        }

        // generating dispatcher declaration for short-hand visitor
        auto lowered_kind = std::string(kind);
        lowered_kind[0] = kind[0] - 'A' + 'a';
        dispatch_decl.writeln("// short-hand visitor for ", ref_name);
        dispatch_decl.writeln("template<typename Visitor>");
        dispatch_decl.writeln(result_type, " visit_", kind, "(Visitor&& visitor,const ebm::", ref_name, "& ref) {");
        {
            auto scope = dispatch_decl.indent_scope();
            dispatch_decl.writeln("MAYBE(elem, visitor.module_.get_", lowered_kind, "(ref));");
            dispatch_decl.writeln("return visit_", kind, "(visitor,elem,ref);");
        }
        dispatch_decl.writeln("}");

        // generic visitor
        dispatch_decl.writeln("// generic visitor for ", ref_name);
        dispatch_decl.writeln("template<typename Context>");
        dispatch_decl.writeln(result_type, " visit_Object(Context&& ctx,const ebm::", ref_name, "& ref) {");
        {
            auto scope = dispatch_decl.indent_scope();
            dispatch_decl.writeln("return visit_", kind, "(ctx,ref);");
        }
        dispatch_decl.writeln("}");

        auto list_name = std::format("{}s", kind);
        if (kind == "Statement") {
            list_name = "Block";
        }

        // generating dispatcher function for list
        dispatch_decl.writeln("template<typename Context>");
        dispatch_decl.writeln(result_type, " visit_", list_name, "_default(Context&& ctx,const ebm::", list_name, "& in) {");
        {
            auto list_scope = dispatch_decl.indent_scope();
            if (is_codegen) {
                dispatch_decl.writeln("CodeWriter w;");
            }
            dispatch_decl.writeln("for(auto& elem:in.container) {");
            {
                auto loop_scope = dispatch_decl.indent_scope();
                dispatch_decl.writeln("auto result = visit_", kind, "(ctx,elem);");
                dispatch_decl.writeln("if (!result) {");
                dispatch_decl.indent_writeln("return unexpect_error(std::move(result.error()));");
                dispatch_decl.writeln("}");
                if (is_codegen) {
                    dispatch_decl.writeln("w.write(result.to_writer());");
                }
            }
            dispatch_decl.writeln("}");
            if (is_codegen) {
                dispatch_decl.writeln("return w;");
            }
            else {
                dispatch_decl.writeln("return {};");  // Placeholder for non-codegen mode
            }
        }
        dispatch_decl.writeln("}");
        // generic visitor
        {
            dispatch_decl.writeln("// generic visitor for ", list_name);
            dispatch_decl.writeln("template<typename Context>");
            dispatch_decl.writeln(result_type, " visit_Object(Context&& ctx,const ebm::", list_name, "& in) {");
            {
                auto scope = dispatch_decl.indent_scope();
                dispatch_decl.writeln("return visit_", list_name, "(ctx,in);");
            }
            dispatch_decl.writeln("}");
        }
    }

    void generate_class_based(std::string_view ns_name, CodeWriter& header, CodeWriter& source, std::map<std::string_view, Struct>& struct_map, bool is_codegen) {
        // in merge order
        CodeWriter contexts_decl;
        CodeWriter dispatch_decl;
        CodeWriter user_logic_includes;
        CodeWriter dispatch_impl;
        CodeWriter actual_visitor_impl;

        actual_visitor_impl.writeln("struct VisitorImpl : ");
        auto actual_visitor_impl_scope = actual_visitor_impl.indent_scope();

        std::string result_type = "expected<Result>";
        std::string default_visitor_impl_dir = "ebmcodegen/default_codegen_class_visitor/";
        std::string visitor_impl_dir = "visitor/";
        std::string visitor_dsl_impl_dir = "visitor/dsl/";

        dispatch_decl.writeln("// Customization point for visitor implementation");
        dispatch_decl.writeln("template<class Tag>");
        dispatch_decl.writeln("struct Visitor;");

        auto define_tag_variant = [&](auto&& name) {
            dispatch_decl.writeln("template<class TagDetail>");
            dispatch_decl.writeln("struct ", name, "{};");
        };

        define_tag_variant("UserHook");
        define_tag_variant("UserDSL");
        define_tag_variant("DefaultCodegen");

        auto concat = [](auto&&... parts) {
            std::string result;
            ((result += parts), ...);
            return result;
        };

        auto define_tag = [&](auto&& w, auto&&... tag) {
            w.writeln("struct ", tag..., " {};");
            actual_visitor_impl.writeln("public ", tag..., ",");
        };
        auto context_of = [&](auto&& kind, auto&&... suffix) -> std::string {
            return concat("Context_", kind, concat(suffix...));
        };

        auto tag_of = [&](auto&& kind, auto&&... suffix) -> std::string {
            return concat("VisitorTag_", kind, concat(suffix...));
        };

        auto with_variant = [&](std::string_view variant, auto&& tag) -> std::string {
            return concat(ns_name, "::", variant, "<", ns_name, "::", tag, ">");
        };

        auto visitor_of = [&](auto&& tag) {
            return concat("Visitor<", tag, ">");
        };

        auto define_visit = [&](auto&& w, auto&& func_name, auto&& ctx_point, auto&& inner) {
            w.writeln("template<typename Visitor>");
            w.write(result_type, " ", func_name, "(", context_of(ctx_point), "<Visitor>& ctx)");
            w.writeln(" {");
            {
                auto indent = w.indent_scope();
                inner();
            }
            w.writeln("}");
        };

        auto insert_include = [&](auto&& w, auto&& impl_dir, auto&& logic_path, auto&& class_name, auto&& more_requirements) {
            w.writeln("#if __has_include(\"", impl_dir, logic_path, ".hpp", "\")");
            w.writeln("#include \"", impl_dir, logic_path, ".hpp", "\"");
            w.writeln("static_assert(requires { sizeof(", ns_name, "::", class_name, "); },\"MUST implement ", class_name, "\");");
            more_requirements();
            w.writeln("#else");
            w.writeln("// stub implementation");
            w.writeln("template<>");
            w.writeln("struct ", ns_name, "::", class_name, " {};");
            w.writeln("#endif");
        };

        auto requires_implementation = [&](std::string_view self, std::string_view class_name, std::string_view method_name, auto&&... args) {
            return std::format("requires {{ {}.{}::{}({}); }}", self, class_name, method_name, concat(args...));
        };

        auto insert_invoke = [&](auto&& w, auto&& class_name, auto&& method_name, auto&& args, auto&& result_handle) {
            w.writeln("if constexpr (", requires_implementation("ctx.visitor", class_name, method_name, args), ") {");
            {
                auto indent = w.indent_scope();
                w.writeln(result_handle, " ctx.visitor.", class_name, "::", method_name, "(", args, ");");
            }
            w.writeln("}");
        };

        auto dispatcher = [&](auto t, std::string_view kind, auto subset) {
            using T = std::decay_t<decltype(t)>;
            auto body_name = std::string(kind) + "Body";
            auto ref_name = std::string(kind) + "Ref";

            write_visit_entry(dispatch_decl, result_type, kind, true, "visit_", kind);
            dispatch_decl.writeln(";");

            auto& body = struct_map[body_name];

            auto insert_context_definition = [&](auto& w, T t) {
                w.writeln("template<typename Visitor>");
                w.writeln("struct ", context_of(kind, "_", to_string(t)), " {");
                {
                    auto scope = w.indent_scope();
                    w.writeln("ebm::", ref_name, " item_id;");
                    for (auto& field : body.fields) {
                        if (!subset[t].first.contains(field.name)) {
                            continue;
                        }
                        std::string typ;
                        if (field.type.contains("std::") || field.type == "bool") {
                            typ = field.type;
                        }
                        else {
                            typ = std::format("ebm::{}", field.type);
                        }
                        if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                            typ = std::format("std::vector<{}>", typ);
                        }
                        w.writeln("const ", typ, "& ", field.name, ";");
                    }
                    w.writeln("Visitor& visitor;");
                }
                w.writeln("};");
            };

            auto construct_context = [&](auto& w, T t) {
                w.writeln(context_of(kind, "_", to_string(t)), "<std::decay_t<decltype(ctx.visitor)>> new_ctx{");
                {
                    auto scope = w.indent_scope();
                    w.writeln("is_nil(alias_ref) ? in.id : alias_ref,");
                    for (auto& field : body.fields) {
                        if (!subset[t].first.contains(field.name)) {
                            continue;
                        }
                        w.writeln(field.name, ",");
                    }
                    w.writeln("ctx.visitor,");
                }
                w.writeln("};");
            };

            auto deconstruct_context_macro = [&](auto& w, T t) {
                std::string upper_kind = std::string(kind);
                for (auto& c : upper_kind) {
                    c = std::toupper(c);
                }
                w.writeln("#define DECONSTRUCT_CONTEXT_", upper_kind, "_", to_string(t), "(ctx) \\");
                w.write("auto& item_id = ctx.item_id;");
                for (auto& field : body.fields) {
                    if (!subset[t].first.contains(field.name)) {
                        continue;
                    }
                    w.writeln(" \\");
                    w.write("auto& ", field.name, " = ctx.", field.name, ";");
                }
                w.writeln();
                w.writeln();
            };

            auto validate_body = [&](auto& w, T t) {
                for (auto& field : body.fields) {
                    if (!subset[t].first.contains(field.name)) {
                        continue;
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::PTR) {
                        w.writeln("if (!in.body.", field.name, "()) {");
                        w.indent_writeln("return unexpect_error(\"Unexpected null pointer for ", body_name, "::", field.name, "\");");
                        w.writeln("}");
                        w.writeln("auto& ", field.name, " = *in.body.", field.name, "();");
                    }
                    else {
                        w.writeln("auto& ", field.name, " = in.body.", field.name, ";");
                    }
                }
            };

            auto insert_dispatch_function = [&](auto& w, auto&& fn_sig_action, auto&& target, auto&& entry_action, auto&& orig_arg, auto&& default_action) {
                fn_sig_action();
                w.writeln(" {");
                {
                    auto scope = w.indent_scope();
                    entry_action();
                    auto main_path = concat(target);
                    auto main_tag = tag_of(main_path);
                    auto before_path = concat(target, suffixes[suffix_before]);
                    auto before_tag = tag_of(before_path);
                    auto after_path = concat(target, suffixes[suffix_after]);
                    auto after_tag = tag_of(after_path);

                    auto insert_includes = [&](auto&& tag, auto&& path, auto&& args1, auto&& args2, auto&& result_handle) {
                        auto user_hook = visitor_of(with_variant("UserHook", tag));
                        auto user_hook_dsl = visitor_of(with_variant("UserDSL", tag));
                        auto default_hook = visitor_of(with_variant("DefaultCodegen", tag));
                        auto make_declval = [&](auto&& hook_ty) {
                            return std::format("std::declval<{}::{}>()", ns_name, hook_ty);
                        };
                        insert_include(user_logic_includes, visitor_impl_dir, path, user_hook, [&] {
                            auto requires_impl = requires_implementation(make_declval(user_hook), user_hook, "visit", args1);
                            if (std::string_view(args2) != "") {
                                requires_impl += " || " + requires_implementation(make_declval(user_hook), user_hook, "visit", args2);
                            }
                            user_logic_includes.writeln("static_assert(", requires_impl, ",\"MUST implement visit method in ", user_hook, "\");");
                        });
                        insert_include(user_logic_includes, visitor_dsl_impl_dir, path, user_hook_dsl, [&] {
                            auto requires_impl = requires_implementation(make_declval(user_hook_dsl), user_hook_dsl, "visit", args1);
                            if (std::string_view(args2) != "") {
                                requires_impl += " || " + requires_implementation(make_declval(user_hook_dsl), user_hook_dsl, "visit", args2);
                            }
                            user_logic_includes.writeln("static_assert(", requires_impl, ",\"MUST implement visit method in ", user_hook_dsl, "\");");
                        });
                        insert_include(user_logic_includes, default_visitor_impl_dir, path, default_hook, [&] {
                            auto requires_impl = requires_implementation(make_declval(default_hook), default_hook, "visit", args1);
                            if (std::string_view(args2) != "") {
                                requires_impl += " || " + requires_implementation(make_declval(default_hook), default_hook, "visit", args2);
                            }
                            user_logic_includes.writeln("static_assert(", requires_impl, ",\"MUST implement visit method in ", default_hook, "\");");
                        });
                        insert_invoke(w, user_hook, "visit", args1, result_handle);
                        w.write("else ");
                        if (std::string_view(args2) != "") {
                            insert_invoke(w, user_hook, "visit", args2, result_handle);
                            w.write("else ");
                        }
                        insert_invoke(w, user_hook_dsl, "visit", args1, result_handle);
                        w.write("else ");
                        if (std::string_view(args2) != "") {
                            insert_invoke(w, user_hook_dsl, "visit", args2, result_handle);
                            w.write("else ");
                        }
                        insert_invoke(w, default_hook, "visit", args1, result_handle);
                        if (std::string_view(args2) != "") {
                            w.write("else ");
                            insert_invoke(w, default_hook, "visit", args2, result_handle);
                        }
                        if (tag == main_tag) {
                            w.writeln("else {");
                            {
                                auto indent = w.indent_scope();
                                default_action();
                            }
                            w.writeln("}");
                        }
                    };
                    w.writeln("auto main_logic = [&]() -> ", result_type, " {");
                    {
                        auto indent = w.indent_scope();
                        insert_includes(main_tag, main_path, orig_arg, "", "return ");
                    }
                    w.writeln("};");
                    w.writeln("std::optional<", result_type, "> result_before;");
                    insert_includes(before_tag, before_path, concat(orig_arg, ", main_logic"), orig_arg, "result_before = ");
                    w.writeln("if(result_before) {");
                    w.indent_writeln("return *result_before;");
                    w.writeln("}");
                    w.writeln("auto result = main_logic();");
                    w.writeln("std::optional<", result_type, "> result_after;");
                    insert_includes(after_tag, after_path, concat(orig_arg, ", result"), orig_arg, "result_after = ");
                    w.writeln("if(result_after) {");
                    w.indent_writeln("return *result_after;");
                    w.writeln("}");
                    w.writeln("return result;");
                }
                w.writeln("}");
            };

            define_tag(dispatch_decl, tag_of(kind));
            define_tag(dispatch_decl, tag_of(kind, suffixes[suffix_before]));
            define_tag(dispatch_decl, tag_of(kind, suffixes[suffix_after]));

            for (size_t i = 0; to_string(T(i))[0]; i++) {
                insert_context_definition(contexts_decl, T(i));
                deconstruct_context_macro(contexts_decl, T(i));
                auto t_kind = concat(kind, "_", to_string(T(i)));
                define_tag(dispatch_decl, tag_of(t_kind));
                define_tag(dispatch_decl, tag_of(t_kind, suffixes[suffix_before]));
                define_tag(dispatch_decl, tag_of(t_kind, suffixes[suffix_after]));
                write_visit_entry(dispatch_decl, result_type, kind, true, make_dispatch_func_name(kind, T(i)));
                dispatch_decl.writeln(";");
                insert_dispatch_function(
                    dispatch_impl,
                    [&] {
                        write_visit_entry(dispatch_impl, result_type, kind, false, make_dispatch_func_name(kind, T(i)));
                    },
                    concat(kind, "_", to_string(T(i))),
                    [&] {
                        validate_body(dispatch_impl, T(i));
                        construct_context(dispatch_impl, T(i));
                    },
                    "new_ctx",
                    [&] {
                    if (is_codegen) {
                        dispatch_impl.writeln("if (ctx.visitor.flags.debug_unimplemented) {");
                        {
                            auto indent = dispatch_impl.indent_scope();

                            dispatch_impl.writeln("return std::format(\"{{{{Unimplemented ", kind, "_", to_string(T(i)), " {}}}}}\",get_id(new_ctx.item_id));");
                        }
                        dispatch_impl.writeln("}");
                    }
                    dispatch_impl.writeln("return expected<Result>{}; // default empty result"); });
            }

            common_dispatch<T>(dispatch_decl, kind, ref_name, is_codegen, result_type);

            insert_dispatch_function(
                dispatch_impl,
                [&] {
                    write_visit_entry(dispatch_impl, result_type, kind, false, concat("visit_", kind));
                },
                kind,
                [] {},
                "ctx, in, alias_ref",
                [&] {
                    return dispatch_impl.writeln("return visit_", kind, "_default(ctx, in, alias_ref);");
                });
            auto list_name = std::format("{}s", kind);
            if (kind == "Statement") {
                list_name = "Block";
            }
            define_tag(dispatch_decl, tag_of(list_name));
            define_tag(dispatch_decl, tag_of(list_name, suffixes[suffix_before]));
            define_tag(dispatch_decl, tag_of(list_name, suffixes[suffix_after]));

            insert_dispatch_function(
                dispatch_impl,
                [&] {
                    dispatch_impl.writeln("template<typename Context>");
                    dispatch_impl.write(result_type, " visit_", list_name, "(Context&& ctx,const ebm::", list_name, "& in)");
                },
                list_name,
                [] {},
                "ctx, in",
                [&] {
                    return dispatch_impl.writeln("return visit_", list_name, "_default(ctx, in);");
                });
        };

        dispatcher(ebm::StatementKind{}, prefixes[prefix_statement], ebmcodegen::body_subset_StatementBody());
        dispatcher(ebm::ExpressionKind{}, prefixes[prefix_expression], ebmcodegen::body_subset_ExpressionBody());
        dispatcher(ebm::TypeKind{}, prefixes[prefix_type], ebmcodegen::body_subset_TypeBody());

        actual_visitor_impl.writeln(" std::monostate {};");
        actual_visitor_impl_scope.execute();
        header.writeln("namespace ", ns_name, " {");
        {
            auto ns_scope = header.indent_scope();
            header.writeln("using namespace ebmgen;");
            header.writeln("using namespace ebmcodegen::util;");
            header.writeln("using CodeWriter = futils::code::LocWriter<std::string,std::vector,ebm::AnyRef>;");

            generate_Result(header, is_codegen);

            header.write_unformatted(contexts_decl.out());
            header.writeln();
            header.write_unformatted(dispatch_decl.out());
            header.writeln();
        }
        header.writeln("} // namespace ", ns_name);
        source.write_unformatted(user_logic_includes.out());
        source.writeln();
        source.writeln("namespace ", ns_name, " {");
        {
            auto ns_scope = source.indent_scope();
            source.write_unformatted(dispatch_impl.out());
            source.writeln();
            source.write_unformatted(actual_visitor_impl.out());
            source.writeln();
        }
        source.writeln("} // namespace ", ns_name);
    }
    */
}  // namespace ebmcodegen
