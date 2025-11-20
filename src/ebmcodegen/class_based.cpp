/*license*/
#include "stub/structs.hpp"
#include <format>
#include "stub/hooks.hpp"

namespace ebmcodegen {

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
}  // namespace ebmcodegen
