/*license*/
#include "stub/class_based.hpp"
#include "stub/structs.hpp"
#include <format>
#include <string>
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

    std::string lower(std::string_view s) {
        std::string result;
        for (auto c : s) {
            result += std::tolower(c);
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
                name += suffixes[suffix_before];
            }
            else if (kind & ContextClassKind_After) {
                name += suffixes[suffix_after];
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
            ContextClasses per_kind;
            per_kind.main().base = kind;
            per_kind.main().kind = ContextClassKind_Generic;
            add_common_visitor(per_kind.main());
            per_kind.main().fields.push_back(ContextClassField{.name = "in", .type = "ebm::" + std::string(kind)});
            per_kind.main().fields.push_back(ContextClassField{.name = "alias_ref", .type = "ebm::" + std::string(kind) + "Ref"});
            add_context_variant_for_before_after(per_kind);
            context_classes.push_back(std::move(per_kind));
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

    std::string deconstruct_macro_name(std::string_view ns_name, const ContextClass& cls) {
        return upper(ns_name) + "_DECONSTRUCT_" + cls.upper_class_name();
    }

    void deconstruct_context_fields_macro(CodeWriter& w, std::string_view ns_name, const ContextClass& cls) {
        w.writeln("// Deconstruct context fields");
        w.writeln("#define ", deconstruct_macro_name(ns_name, cls), "(instance_name) \\");
        for (auto& field : cls.fields) {
            w.write("auto& ", field.name, " = instance_name.", field.name, ";");
        }
        w.writeln();
    }

    void handle_hijack_logic(CodeWriter& w, std::string_view may_inject) {
        w.writeln("if (!", may_inject, ") {");
        {
            auto if_scope = w.indent_scope();
            w.writeln("if(!is_pass_error(", may_inject, ".error())) {");
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
        w.writeln("// This is for signaling continue normal processing without error");
        w.writeln("constexpr auto pass = futils::helper::either::unexpected{ebmgen::Error(futils::error::Category::lib,0xba55ba55)};");
        w.writeln("constexpr bool is_pass_error(const ebmgen::Error& err) {");
        {
            auto scope = w.indent_scope();
            w.writeln("return err.category() == futils::error::Category::lib && err.sub_category() == 0xba55ba55;");
        }
        w.writeln("}");
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

    void generate_visitor_common(CodeWriter& w) {
        w.writeln("ebmgen::MappingTable module_;");
        w.writeln("Flags& flags;");
        w.writeln("Output& output;");
        w.writeln("ebmcodegen::WriterManager<CodeWriter> wm;");
    }

    void generate_merged_visitor(CodeWriter& w, const std::vector<HookType>& hooks, const std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        w.writeln("struct MergedVisitor {");
        {
            auto scope = w.indent_scope();
            generate_visitor_common(w);
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

    void generate_Flags(CodeWriter& w, const IncludeLocations& flags) {
        auto with_flag_bind = [&](bool on_define) {
            constexpr auto ensure_c_ident = "static_assert(ebmcodegen::util::internal::is_c_ident(#name),\"name must be a valid C identifier\");";
            w.writeln("#define DEFINE_FLAG(type,name,default_,flag_name,flag_func,...) \\");
            if (on_define) {
                w.indent_writeln(ensure_c_ident, "type name = default_");
            }
            else {
                w.indent_writeln("ctx.flag_func(&name,flag_name,__VA_ARGS__)");
            }
            w.write("#define WEB_FILTERED(...) ");
            if (!on_define) {
                w.write("web_filtered.insert_range(std::set{__VA_ARGS__})");
            }
            w.writeln();
            auto map_name = [&](auto name, auto dst, auto src) {
                w.write("#define ", name, "(", src, ") ");
                if (!on_define) {
                    w.write(dst, " = ", src);
                }
                w.writeln();
            };
            map_name("WEB_UI_NAME", "ui_lang_name", "name");
            map_name("WEB_LSP_NAME", "lsp_name", "name");
            map_name("WEB_WORKER_NAME", "webworker_name", "name");
            w.write("#define FILE_EXTENSIONS(...) ");
            if (!on_define) {
                w.write("file_extensions = std::vector<std::string_view>{__VA_ARGS__}");
            }
            w.writeln();

            w.writeln("#define DEFINE_BOOL_FLAG(name,default_,flag_name,desc) DEFINE_FLAG(bool,name,default_,flag_name,VarBool,desc)");
            w.writeln("#define DEFINE_STRING_FLAG(name,default_,flag_name,desc,arg_desc) DEFINE_FLAG(std::string_view,name,default_,flag_name,VarString<true>,desc,arg_desc)");
            w.write("#define BEGIN_MAP_FLAG(name,MappedType,default_,flag_name,desc)");
            if (on_define) {
                w.write(ensure_c_ident, "MappedType name = default_;");
            }
            else {
                w.write("{ std::map<std::string,MappedType> map__; auto& target__ = name; auto flag_name__ = flag_name; auto desc__ = desc; std::string arg_desc__ = \"{\"; ");
            }
            w.writeln();
            w.write("#define MAP_FLAG_ITEM(key,value) ");
            if (!on_define) {
                w.write("map__[key] = value;");
                w.write("if (!arg_desc__.empty() && arg_desc__.back() != '{') { arg_desc__ += \",\"; }");
                w.write("arg_desc__ += key;");
            }
            w.writeln();
            w.write("#define END_MAP_FLAG() ");
            if (!on_define) {
                w.write("ctx.VarMap(&target__,flag_name__,desc__,arg_desc__ + \"}\",std::move(map__)); }");
            }
            w.writeln();

            // insert_include(w, prefixes[prefix_flags]);
            w.writeln("#undef DEFINE_FLAG");
            w.writeln("#undef WEB_FILTERED");
            w.writeln("#undef DEFINE_BOOL_FLAG");
            w.writeln("#undef DEFINE_STRING_FLAG");
            w.writeln("#undef BEGIN_MAP_FLAG");
            w.writeln("#undef MAP_FLAG_ITEM");
            w.writeln("#undef END_MAP_FLAG");
            w.writeln("#undef WEB_UI_NAME");
            w.writeln("#undef WEB_LSP_NAME");
            w.writeln("#undef WEB_WORKER_NAME");
            w.writeln("#undef FILE_EXTENSIONS");
        };

        w.writeln("struct Flags : ebmcodegen::Flags {");
        {
            auto flag_scope = w.indent_scope();
            with_flag_bind(true);
            // insert_include(w, prefixes[prefix_flags], suffixes[suffix_struct]);
            w.writeln("void bind(futils::cmdline::option::Context& ctx) {");
            auto nested_scope = w.indent_scope();
            w.writeln("lang_name = \"", flags.lang, "\";");
            w.writeln("ui_lang_name = lang_name;");
            w.writeln("lsp_name = lang_name;");
            w.writeln("webworker_name = \"", flags.program_name, "\";");
            w.writeln("file_extensions = {\".", flags.lang, "\"};");
            w.writeln("ebmcodegen::Flags::bind(ctx); // bind basis");
            with_flag_bind(false);
            // insert_include(w, prefixes[prefix_flags], suffixes[suffix_bind]);
            nested_scope.execute();
            w.writeln("}");
        }
        w.writeln("};");
    }

    void generate_Output(CodeWriter& w) {
        w.writeln("struct Output : ebmcodegen::Output {");
        // insert_include(w, prefixes[prefix_output]);
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

    void generate_list_dispatch_default(CodeWriter& w, const ContextClass& cls, bool is_codegen, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_List)) {
            return;
        }
        auto class_name = cls.class_name();
        w.writeln("template<typename Context>");
        w.writeln(result_type, " dispatch_", class_name, "_default(Context&& ctx,const ebm::", class_name, "& in) {");
        {
            auto list_scope = w.indent_scope();
            if (is_codegen) {
                w.writeln("CodeWriter w;");
            }
            w.writeln("for(auto& elem:in.container) {");
            {
                auto loop_scope = w.indent_scope();
                w.writeln("auto result = visit_", cls.base, "(ctx,elem);");
                w.writeln("if (!result) {");
                w.indent_writeln("return unexpect_error(std::move(result.error()));");
                w.writeln("}");
                if (is_codegen) {
                    w.writeln("w.write(result->to_writer());");
                }
            }
            w.writeln("}");
            if (is_codegen) {
                w.writeln("return w;");
            }
            else {
                w.writeln("return {};");  // Placeholder for non-codegen mode
            }
        }
        w.writeln("}");
    }

    void generate_generic_dispatch_default(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_Generic)) {
            return;
        }
        auto class_name = cls.class_name();
        w.writeln("template<typename Context>");
        w.writeln(result_type, " dispatch_", class_name, "_default(Context&& ctx,const ebm::", class_name, "& in,ebm::", cls.ref_name(), " alias_ref = {}) {");
        {
            auto scope = w.indent_scope();
            w.writeln("switch(in.body.kind) {");
            {
                auto switch_scope = w.indent_scope();
                auto do_for_each = [&](auto t) {
                    using T = std::decay_t<decltype(t)>;
                    for (size_t i = 0; to_string(T(i))[0]; i++) {
                        w.writeln("case ebm::", cls.base, "Kind::", to_string(T(i)), ": {");
                        {
                            auto case_scope = w.indent_scope();
                            w.writeln("return dispatch_", cls.base, "_", to_string(T(i)), "(std::forward<Context>(ctx),in,alias_ref);");
                        }
                        w.writeln("}");
                    }
                };
                if (cls.base == "Statement") {
                    do_for_each(ebm::StatementKind{});
                }
                else if (cls.base == "Expression") {
                    do_for_each(ebm::ExpressionKind{});
                }
                else if (cls.base == "Type") {
                    do_for_each(ebm::TypeKind{});
                }
                w.writeln("default: {");
                {
                    auto default_scope = w.indent_scope();
                    w.writeln("return unexpect_error(\"Unknown ", cls.base, " kind: {}\", to_string(in.body.kind));");
                }
                w.writeln("}");
            }
            w.writeln("}");
        }
        w.writeln("}");
    }

    void generate_generator_default_hook(CodeWriter& src, const HookType& hook, const ContextClass& cls, bool is_codegen, const std::string_view result_type) {
        auto type_name = hook.visitor_instance_name(cls);
        src.writeln("template <>");
        src.writeln("struct ", type_name, " {");
        {
            auto scope = src.indent_scope();
            src.writeln("template<typename Context>");
            src.writeln("auto visit(const Context& ctx) {");
            {
                auto visit_scope = src.indent_scope();
                if (cls.is_before_after()) {
                    src.writeln("return pass;");
                }
                else if (cls.has(ContextClassKind_List)) {
                    src.writeln("return dispatch_", cls.class_name(), "_default(ctx,ctx.in);");
                }
                else if (cls.has(ContextClassKind_Generic)) {
                    src.writeln("return dispatch_", cls.class_name(), "_default(ctx,ctx.in,ctx.alias_ref);");
                }
                else {
                    src.writeln("if (ctx.visitor.flags.debug_unimplemented) {");
                    {
                        auto scope = src.indent_scope();
                        if (is_codegen) {
                            src.writeln("return std::format(\"{{{{Unimplemented ", cls.class_name(), " {}}}}}\",get_id(ctx.item_id));");
                        }
                    }
                    src.writeln("}");
                    src.writeln("return ", result_type, "{}; // Unimplemented");
                }
            }
            src.writeln("}");
        }
        src.writeln("};");
    }

    void include_with_if_exists(CodeWriter& w, std::string_view header, auto&& then, auto&& otherwise) {
        w.writeln("#if __has_include(", header, ")");
        then();
        w.writeln("#else");
        otherwise();
        w.writeln("#endif");
    }

    auto define_local_macro(CodeWriter& w, std::string_view macro_name, std::string_view func_arg, std::string_view body) {
        w.writeln("#define ", macro_name, func_arg, " ", body);
        return futils::helper::defer([=, &w]() {
            w.writeln("#undef ", macro_name);
        });
    }

    constexpr auto macro_CODEGEN_VISITOR = "CODEGEN_VISITOR";
    constexpr auto macro_CODEGEN_CONTEXT_PARAMETERS = "CODEGEN_CONTEXT_PARAMETERS";
    constexpr auto macro_CODEGEN_CONTEXT = "CODEGEN_CONTEXT";

    void generate_dummy_macro_for_class(CodeWriter& w, std::string_view ns_name, const HookType& hook, const ContextClass& cls) {
        auto instance = std::format("{}::{}", ns_name, context_instance_name(cls));
        auto visitor = hook.visitor_instance_name(cls, ns_name);
        auto upper_ns = upper(ns_name);
        auto cls_name = cls.class_name();
        w.writeln("#define ", upper_ns, "_", macro_CODEGEN_VISITOR, "_", cls_name, " ", visitor);
        w.writeln("#define ", upper_ns, "_", macro_CODEGEN_CONTEXT_PARAMETERS, "_", cls_name, " ", cls.type_parameters_body());
        w.writeln("#define ", upper_ns, "_", macro_CODEGEN_CONTEXT, "_", cls_name, " ", instance);
    }

    void generate_undef_dummy_macros(CodeWriter& src) {
        src.writeln("#undef ", macro_CODEGEN_VISITOR);
        src.writeln("#undef ", macro_CODEGEN_CONTEXT_PARAMETERS);
        src.writeln("#undef ", macro_CODEGEN_CONTEXT);
    }

    void generate_dummy_macros(CodeWriter& hdr, std::string_view ns_name, const HookType& hook, const std::vector<ContextClasses>& context_classes) {
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                generate_dummy_macro_for_class(hdr, ns_name, hook, cls);
            }
        }
        hdr.writeln("#define ", macro_CODEGEN_VISITOR, "(name) ", upper(ns_name), "_", macro_CODEGEN_VISITOR, "_##name");
        hdr.writeln("#define ", macro_CODEGEN_CONTEXT_PARAMETERS, "(name) ", upper(ns_name), "_", macro_CODEGEN_CONTEXT_PARAMETERS, "_##name");
        hdr.writeln("#define ", macro_CODEGEN_CONTEXT, "(name) ", upper(ns_name), "_", macro_CODEGEN_CONTEXT, "_##name");
    }

    void generate_inlined_hook(CodeWriter& w,
                               std::string_view ns_name, std::string_view header_name, const HookType& hook, const ContextClass& cls, const std::string_view result_type) {
        auto instance = hook.visitor_instance_name(cls, ns_name);
        w.writeln("// Inlined hook for ", cls.class_name(), " for backward compatibility");
        w.writeln("template <>");
        w.writeln("struct ", instance, " {");
        {
            auto scope = w.indent_scope();
            auto context_name = context_instance_name(cls);
            w.writeln(cls.type_parameters());
            w.writeln(result_type, " visit(const ", context_name, "& ctx) {");
            {
                auto visit_scope = w.indent_scope();
                auto deconstruct_macro = deconstruct_macro_name(ns_name, cls);
                w.writeln(deconstruct_macro, "(ctx);");
                w.writeln("auto& module_ = ctx.visitor.module_;");
                w.writeln("#include ", header_name);
                if (cls.is_before_after()) {
                    w.writeln("return pass;");
                }
                else {
                    w.writeln("return {};");
                }
            }
            w.writeln("}");
        }
        w.writeln("};");
    }

    void generate_user_implemented_includes(CodeWriter& w, std::string_view ns_name, const std::vector<HookType>& hooks, const IncludeLocations& locations, const std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        assert(hooks.size() - 1 == locations.include_locations.size());
        // without last hidden hook
        for (auto i = 0; i < hooks.size() - 1; i++) {
            auto& hook = hooks[i];
            auto location = locations.include_locations[i];
            for (auto& cls_group : context_classes) {
                for (auto& cls : cls_group.classes) {
                    auto header = std::format("\"{}{}{}.hpp\"", location.location, cls.class_name(), location.suffix);
                    auto instance = hook.visitor_instance_name(cls, ns_name);
                    include_with_if_exists(
                        w, header,
                        [&] {
                            if (hook.name.contains("Inlined")) {
                                generate_inlined_hook(w, ns_name, header, hook, cls, result_type);
                                return;
                            }
                            auto current_class = define_local_macro(w, macro_CODEGEN_VISITOR, "(dummy_name)", instance);
                            auto current_context_parameters = define_local_macro(w, macro_CODEGEN_CONTEXT_PARAMETERS, "(dummy_name)", cls.type_parameters_body());
                            auto current_context = define_local_macro(w, macro_CODEGEN_CONTEXT, "(dummy_name)", context_instance_name(cls));
                            w.writeln("#include ", header);
                        },
                        [&] {
                            w.writeln("template <>");
                            w.writeln("struct ", instance, " {}; // Unimplemented");
                        });
                }
            }
        }
    }

    void generate_generic_visit_for_ui(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_Generic)) {
            return;
        }
        w.writeln("// generic visitor for ", cls.class_name());
        w.writeln("template<typename Context>");
        w.writeln(result_type, " visit_", cls.class_name(), "(Context&& ctx,const ebm::", cls.class_name(), "& in,ebm::", cls.ref_name(), " alias_ref = {}) {");
        {
            auto scope = w.indent_scope();
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return ", dispatch_fn, "(ctx,in,alias_ref);");
        }
        w.writeln("}");

        w.writeln("// short-hand visitor for ", cls.class_name());
        w.writeln("template<typename Context>");
        w.writeln(result_type, " visit_", cls.class_name(), "(Context&& ctx,const ebm::", cls.ref_name(), "& ref) {");
        {
            auto scope = w.indent_scope();
            w.writeln("MAYBE(elem, ctx.visitor.module_.get_", lower(cls.class_name()), "(ref));");
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return ", dispatch_fn, "(ctx,elem,ref);");
        }
        w.writeln("}");

        w.writeln("// for DSL convenience");
        w.writeln("template<typename Context>");
        w.writeln(result_type, " visit_Object(Context&& ctx,const ebm::", cls.class_name(), "& in, ebm::", cls.ref_name(), " alias_ref = {})  {");
        {
            auto scope = w.indent_scope();
            w.writeln("return visit_", cls.class_name(), "(ctx,in,alias_ref);");
        }
        w.writeln("}");

        w.writeln("// for DSL convenience");
        w.writeln("template<typename Context>");
        w.writeln(result_type, " visit_Object(Context&& ctx, ebm::", cls.ref_name(), " ref)  {");
        {
            auto scope = w.indent_scope();
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return visit_", cls.class_name(), "(ctx,ref);");
        }
        w.writeln("}");
    }

    void generate_user_interface(CodeWriter& w, std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        w.writeln("template<typename VisitorImpl>");
        w.writeln("struct InitialContext {");
        {
            auto scope = w.indent_scope();
            w.writeln("VisitorImpl& visitor;");
        }
        w.writeln("};");

        w.writeln("// for backward compatibility");
        w.writeln();

        for (auto& cls : context_classes) {
            generate_generic_visit_for_ui(w, cls.main(), result_type);
        }
    }

    void generate(const IncludeLocations& locations, CodeWriter& hdr, CodeWriter& src, std::map<std::string_view, Struct>& structs) {
        auto result_type = "expected<Result>";
        auto context_classes = generate_context_classes(structs);
        std::vector<HookType> hooks = {
            {.name = "UserHook"},
            {.name = "UserDSLHook"},
            {.name = "DefaultCodegenVisitorHook"},

            // for backward compatibility
            {.name = "UserInlinedHook"},
            {.name = "UserInlinedDSLHook"},
            {.name = "DefaultCodegenVisitorInlinedHook"},

            {.name = "GeneratorDefaultHook"},  // this is hidden
        };

        auto ns_name = locations.ns_name;
        auto is_codegen = locations.is_codegen;
        generate_namespace(hdr, ns_name, true);
        generate_undef_dummy_macros(src);
        generate_user_implemented_includes(src, ns_name, hooks, locations, context_classes, result_type);
        generate_namespace(src, ns_name, true);
        auto ns_scope_hdr = hdr.indent_scope();
        auto ns_scope_src = src.indent_scope();

        generate_namespace_injection(hdr);
        generate_Result(hdr, is_codegen);
        generate_Flags(hdr, locations);
        generate_Output(hdr);
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
                generate_generator_default_hook(src, hooks.back(), cls, is_codegen, result_type);
            }
            generate_dispatcher_function(hdr, src, cls_group, result_type);
            generate_list_dispatch_default(hdr, cls_group.main(), is_codegen, result_type);
            generate_generic_dispatch_default(hdr, cls_group.main(), result_type);
        }

        generate_merged_visitor(src, hooks, context_classes, result_type);

        generate_user_interface(hdr, context_classes, result_type);
        generate_dummy_macros(hdr, ns_name, hooks.front(), context_classes);

        ns_scope_hdr.execute();
        ns_scope_src.execute();
        generate_namespace(hdr, ns_name, false);
        generate_namespace(src, ns_name, false);
    }

}  // namespace ebmcodegen
