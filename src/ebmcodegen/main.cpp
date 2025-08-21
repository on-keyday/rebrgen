/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "../ebm/extended_binary_module.hpp"
#include "helper/template_instance.h"
#include <file/file_stream.h>  // Required for futils::file::FileStream
#include <binary/writer.h>     // Required for futils::binary::writer
#include <file/file_view.h>
#include <code/code_writer.h>
#include <ebmgen/common.hpp>
#include <ebmgen/mapping.hpp>
#include <string>
#include <string_view>
#include <set>

namespace ebmcodegen {
    std::map<ebm::StatementOp, std::set<std::string_view>> body_subset_StatementBody();
    std::map<ebm::TypeKind, std::set<std::string_view>> body_subset_TypeBody();
    std::map<ebm::ExpressionOp, std::set<std::string_view>> body_subset_ExpressionBody();
}  // namespace ebmcodegen

enum class VisitorTemplate {
    CodeGenerator,
    Interpreter,
};

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view lang = "cpp";
    bool body_validate = false;
    VisitorTemplate visitor_template = VisitorTemplate::CodeGenerator;
    std::string_view visitor_impl_dir = "visitor/";

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&lang, "l,lang", "language for output", "LANG");
        ctx.VarBool(&body_validate, "body-validate", "generate code for body validation");
        ctx.VarString<true>(&visitor_impl_dir, "d,visitor-impl-dir", "directory for visitor implementation", "DIR");
        ctx.VarMap(&visitor_template, "mode", "visitor template kind", "{codegen,interpret}",
                   std::map<std::string, VisitorTemplate>{
                       {"codegen", VisitorTemplate::CodeGenerator},
                       {"interpret", VisitorTemplate::Interpreter},
                   });
    }
};

auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();
enum TypeAttribute {
    NONE = 0,
    ARRAY = 0x1,
    PTR = 0x2,
    REF = 0x4,
};
struct StructField {
    std::string_view name;
    std::string_view type;
    TypeAttribute attr = NONE;
};

struct Struct {
    std::string_view name;
    std::vector<StructField> fields;
};

std::map<std::string_view, Struct> make_struct_map() {
    std::vector<Struct> structs;
    structs.push_back({
        ebm::ExtendedBinaryModule::visitor_name,
    });
    std::map<std::string_view, Struct> struct_map;

    ebm::ExtendedBinaryModule::visit_static([&](auto&& visitor, const char* name, auto tag, TypeAttribute dispatch = NONE) -> void {
        using T = typename decltype(tag)::type;
        if constexpr (ebmgen::has_visit<T, decltype(visitor)>) {
            structs.back().fields.push_back({
                name,
                T::visitor_name,
                dispatch,
            });
            if constexpr (!ebmgen::AnyRef<T>) {
                structs.push_back({
                    T::visitor_name,
                });
                T::visit_static(visitor);
                auto s = std::move(structs.back());
                structs.pop_back();
                struct_map[s.name] = std::move(s);
            }
        }
        else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
            using P = typename futils::helper::template_instance_of_t<T, std::vector>::template param_at<0>;
            visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::ARRAY));
        }
        else if constexpr (std::is_pointer_v<T>) {
            using P = std::remove_pointer_t<T>;
            visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::PTR));
        }
        else if constexpr (std::is_enum_v<T>) {
            constexpr const char* enum_name = visit_enum(T{});
            structs.back().fields.push_back({
                name,
                enum_name,
                dispatch,
            });
        }
        else if constexpr (std::is_same_v<T, std::uint64_t>) {
            structs.back().fields.push_back({
                name,
                "std::uint64_t",
                dispatch,
            });
        }
        else if constexpr (std::is_same_v<T, std::uint8_t>) {
            structs.back().fields.push_back({
                name,
                "std::uint8_t",
                dispatch,
            });
        }
        else if constexpr (std::is_same_v<T, bool>) {
            structs.back().fields.push_back({
                name,
                "bool",
                dispatch,
            });
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            structs.back().fields.push_back({
                name,
                "std::string",
                dispatch,
            });
        }
        else if constexpr (std::is_same_v<T, const char(&)[5]>) {  // skip
        }
        else {
            static_assert(std::is_same_v<T, void>, "Unsupported type");
        }
    });
    struct_map["ExtendedBinaryModule"] = std::move(structs[0]);
    return struct_map;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    using CodeWriter = futils::code::CodeWriter<std::string>;
    CodeWriter w;
    w.writeln("// Code generated by ebmcodegen at https://github.com/on-keyday/rebrgen");

    auto struct_map = make_struct_map();
    if (flags.body_validate) {
        w.writeln("#include <ebm/extended_binary_module.hpp>");
        w.writeln("#include <set>");
        w.writeln("#include <map>");
        w.writeln("namespace ebmcodegen {");
        {
            auto scope = w.indent_scope();
            auto do_visit_body = [&](auto t, Struct& s) {
                using T = std::decay_t<decltype(t)>;
                auto map_type = std::format("std::map<ebm::{},std::set<std::string_view>>", visit_enum(t));
                w.writeln(map_type, " body_subset_", s.name, "() {");
                auto scope2 = w.indent_scope();
                w.writeln(map_type, " subset_map;");
                w.writeln("for(size_t i = 0;to_string(ebm::", visit_enum(t), "(i))[0];i++) {");
                auto _scope = w.indent_scope();
                w.writeln("ebm::", s.name, " body;");
                w.writeln("body.kind = ebm::", visit_enum(t), "(i);");
                for (auto& f : s.fields) {
                    if (f.attr & TypeAttribute::PTR) {
                        w.writeln("body.", f.name, "({});");
                    }
                }
                w.writeln("std::set<std::string_view> subset;");
                w.writeln("body.visit([&](auto&& visitor,const char* name,auto&& value) {");
                auto _scope2 = w.indent_scope();
                w.writeln("using T = std::decay_t<decltype(value)>;");
                w.writeln("if constexpr (std::is_pointer_v<T>) {");
                w.indent_writeln("if (value) { subset.insert(name); }");
                w.writeln("}");
                w.writeln("else {");
                w.indent_writeln("subset.insert(name);");
                w.writeln("}");
                _scope2.execute();
                w.writeln("});");
                w.writeln("subset_map[ebm::", visit_enum(t), "(i)] = std::move(subset);");
                _scope.execute();
                w.writeln("}");
                w.writeln("return subset_map;");
                scope2.execute();
                w.writeln("}");
            };

            do_visit_body(ebm::StatementOp{}, struct_map["StatementBody"]);
            do_visit_body(ebm::ExpressionOp{}, struct_map["ExpressionBody"]);
            do_visit_body(ebm::TypeKind{}, struct_map["TypeBody"]);
        }
        w.writeln("} // namespace ebmcodegen");
        cout << w.out();
        return 0;
    }

    w.writeln("#include <ebmcodegen/stub/entry.hpp>");
    w.writeln("#include <ebmgen/common.hpp>");
    w.writeln("#include <ebmgen/convert/helper.hpp>");
    w.writeln("#include <ebmgen/mapping.hpp>");

    CodeWriter visitor_stub;
    CodeWriter visitor_assert;
    visitor_stub.writeln("struct Visitor {");
    auto visitor_scope = visitor_stub.indent_scope();
    visitor_stub.writeln("ebmgen::MappingTable module_;");
    if (flags.visitor_template == VisitorTemplate::CodeGenerator) {
        w.writeln("#include <code/code_writer.h>");
        visitor_stub.writeln("futils::code::CodeWriter<futils::binary::writer&> w;");
        visitor_stub.writeln("Visitor(const ebm::ExtendedBinaryModule& m,futils::binary::writer& w) : module_(m), w{w} {}");
    }
    else {
        visitor_stub.writeln("Visitor(const ebm::ExtendedBinaryModule& m) : module_(m) {}");
    }

    auto insert_include = [&](auto& w, auto&&... path) {
        w.writeln("#if __has_include(\"", flags.visitor_impl_dir, std::forward<decltype(path)>(path)..., "\")");
        w.writeln("#include \"", flags.visitor_impl_dir, std::forward<decltype(path)>(path)..., "\"");
        w.writeln("#endif");
    };
    visitor_stub.writeln("expected<void> entry() {");
    auto entry_scope = visitor_stub.indent_scope();
    insert_include(visitor_stub, "entry", ".hpp");
    visitor_stub.writeln("return {};");  // Placeholder for entry function
    entry_scope.execute();
    visitor_stub.writeln("}");
    w.writeln();  // line for end of #include

    w.writeln("struct Flags : ebmcodegen::Flags {");
    insert_include(w, "Flags", ".hpp");
    w.writeln("};");
    w.writeln("struct Output : ebmcodegen::Output {");
    insert_include(w, "Output", ".hpp");
    w.writeln("};");

    auto ns_name = std::format("ebm{}gen", flags.lang);

    w.writeln("namespace ", ns_name, " {");
    auto ns_scope = w.indent_scope();
    w.writeln("using namespace ebmgen;");

    auto dispatcher = [&](auto t, std::string_view kind, auto subset) {
        using T = std::decay_t<decltype(t)>;
        CodeWriter stmt_dispatcher;
        w.writeln("template<typename Visitor>");
        w.writeln("expected<void> visit_", kind, "(Visitor&& visitor, ebm::", kind, "& in);");
        stmt_dispatcher.writeln("template <typename Visitor>");
        stmt_dispatcher.writeln("expected<void> visit_", kind, "(Visitor&& visitor, ebm::", kind, "& in) {");
        auto scope_ = stmt_dispatcher.indent_scope();
        stmt_dispatcher.writeln("switch (in.body.kind) {");

        for (size_t i = 0; to_string(T(i))[0]; i++) {
            auto body_name = std::string(kind) + "Body";
            auto ref_name = std::string(kind) + "Ref";
            auto& body = struct_map[body_name];
            auto add_arguments = [&] {
                w.write("std::declval<ebm::", ref_name, "&>()");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].contains(field.name)) {
                        continue;
                    }
                    if (field.attr & TypeAttribute::PTR) {
                        w.write(",*std::declval<ebm::", body_name, "&>().", field.name, "()");
                    }
                    else {
                        w.write(",std::declval<ebm::", body_name, "&>().", field.name);
                    }
                }
            };
            auto call_arguments = [&] {
                w.write("in.id");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].contains(field.name)) {
                        continue;
                    }
                    w.write(",", field.name);
                }
            };
            auto concept_name = std::format("has_visitor_{}_{}", kind, to_string(T(i)));
            auto concept_call_name = concept_name + "_call";
            auto visitor_func_name = std::format("visit_{}_{}", kind, to_string(T(i)));
            {
                w.writeln("template<typename Visitor>");
                w.writeln("concept ", concept_name, " = requires(Visitor v) {");
                auto requires_scope = w.indent_scope();

                w.write(" { v.", visitor_func_name, "(");
                add_arguments();
                w.writeln(") } -> std::convertible_to<expected<void>>;");
                requires_scope.execute();
                w.writeln("};");
            }
            {
                w.writeln("template<typename Visitor>");
                w.writeln("concept ", concept_call_name, " = requires(Visitor fn) {");
                auto requires_scope = w.indent_scope();
                w.write(" { fn(");
                add_arguments();
                w.writeln(") } -> std::convertible_to<expected<void>>;");
                requires_scope.execute();
                w.writeln("};");
            }
            w.writeln("template<typename Visitor>");
            w.writeln("expected<void> ", visitor_func_name, "(Visitor&& visitor, ebm::", kind, "& in) {");
            {
                auto scope = w.indent_scope();
                for (auto& field : body.fields) {
                    if (!subset[T(i)].contains(field.name)) {
                        continue;
                    }
                    if (field.attr & TypeAttribute::PTR) {
                        w.writeln("if (!in.body.", field.name, "()) {");
                        w.indent_writeln("return unexpect_error(\"Unexpected null pointer for ", body_name, "::", field.name, "\");");
                        w.writeln("}");
                        w.writeln("auto& ", field.name, " = *in.body.", field.name, "();");
                    }
                    else {
                        w.writeln("auto& ", field.name, " = in.body.", field.name, ";");
                    }
                }
                w.writeln("if constexpr (", concept_name, "<Visitor>) {");
                {
                    auto scope2 = w.indent_scope();
                    w.write("MAYBE_VOID(result, visitor.", visitor_func_name, "(");
                    call_arguments();
                    w.writeln("));");
                }
                w.writeln("}");
                w.writeln("else if constexpr (", concept_call_name, "<Visitor>) {");
                {
                    auto scope3 = w.indent_scope();
                    w.write("MAYBE_VOID(result,visitor(");
                    call_arguments();
                    w.writeln("));");
                }
                w.writeln("}");
                w.writeln("return {};");
            }
            w.writeln("}");
            {
                stmt_dispatcher.writeln("case ebm::", visit_enum(t), "::", to_string(T(i)), ":");
                stmt_dispatcher.indent_writeln("return ", visitor_func_name, "(in);");
            }
            {
                visitor_stub.write("expected<void> ", visitor_func_name, "(ebm::", ref_name, "& item_id");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].contains(field.name)) {
                        continue;
                    }
                    std::string typ;
                    if (field.type.contains("std::") || field.type == "bool") {
                        typ = field.type;
                    }
                    else {
                        typ = std::format("ebm::{}", field.type);
                    }
                    if (field.attr & TypeAttribute::ARRAY) {
                        typ = std::format("std::vector<{}>", typ);
                    }
                    visitor_stub.write(",", typ, "& ", field.name);
                }
                visitor_stub.writeln(") {");
                auto stub_include = visitor_stub.indent_scope();
                insert_include(visitor_stub, kind, "_", to_string(T(i)), ".hpp");
                visitor_stub.writeln("return {};");
                stub_include.execute();
                visitor_stub.writeln("}");
                visitor_assert.writeln("static_assert(", concept_name, "<Visitor>, \"Visitor does not implement ", visitor_func_name, "\");");
            }
        }
        stmt_dispatcher.writeln("default:");
        stmt_dispatcher.indent_writeln("return unexpect_error(\"Unknown ", kind, " kind: {}\", to_string(in.body.kind));");
        stmt_dispatcher.writeln("}");
        scope_.execute();
        stmt_dispatcher.writeln("}");

        w.write_unformatted(stmt_dispatcher.out());
    };

    dispatcher(ebm::StatementOp{}, "Statement", ebmcodegen::body_subset_StatementBody());
    dispatcher(ebm::ExpressionOp{}, "Expression", ebmcodegen::body_subset_ExpressionBody());
    dispatcher(ebm::TypeKind{}, "Type", ebmcodegen::body_subset_TypeBody());

    visitor_scope.execute();
    visitor_stub.writeln("};");
    w.write_unformatted(visitor_stub.out());
    w.write_unformatted(visitor_assert.out());

    ns_scope.execute();
    w.writeln("}  // namespace ", ns_name);

    w.writeln("DEFINE_ENTRY(Flags,Output) {");
    auto main_scope = w.indent_scope();
    if (flags.visitor_template == VisitorTemplate::CodeGenerator) {
        w.writeln(ns_name, "::Visitor visitor{ebm,w};");
    }
    else {
        w.writeln(ns_name, "::Visitor visitor{ebm};");
    }
    insert_include(w, "pre_entry", ".hpp");
    w.writeln("auto result = visitor.entry();");
    w.writeln("if (!result) {");
    auto err_scope = w.indent_scope();
    w.writeln("futils::wrap::cerr_wrap() << \"error: \" << result.error();");
    w.writeln("return 1;");
    err_scope.execute();
    w.writeln("}");
    insert_include(w, "post_entry", ".hpp");
    w.writeln("return 0;");
    main_scope.execute();
    w.writeln("}");
    cout << w.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags,
        [](auto&& str, bool err) {
            if (err)
                cerr << str;
            else
                cout << str;
        },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}