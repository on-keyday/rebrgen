/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include <strutil/splits.h>
#include "glue_code.hpp"

namespace rebgn {

    std::vector<Flag> get_flags(Flags& flags) {
        std::vector<Flag> parsed_flag;
        may_write_from_hook(flags, bm2::HookFile::flags, [&](size_t i, futils::view::rvec line) {
            auto split = futils::strutil::split<futils::view::rvec>(line, "|", 4);
            if (split.size() < 4) {
                futils::wrap::cerr_wrap() << "invalid flag line: " << line << " at " << i << ":split count is " << split.size() << "\n";
                return;
            }
            auto to_str = [&](auto&& str) { return std::string(str.as_char(), str.size()); };
            Flag flag;
            flag.option = to_str(split[0]);
            flag.bind_target = to_str(split[1]);
            flag.type = to_str(split[2]);
            flag.default_value = to_str(split[3]);
            if (split.size() > 4) {
                if (flag.type == "bool") {
                    flag.description = to_str(split[4]);
                }
                else {
                    auto split2 = futils::strutil::split<futils::view::rvec>(split[4], "|", 1);
                    if (split2.size() > 1) {
                        flag.place_holder = to_str(split2[0]);
                        flag.description = to_str(split2[1]);
                    }
                    else {
                        flag.place_holder = to_str(split2[0]);
                    }
                }
            }
            else if (flag.type != "bool") {
                flag.place_holder = "VALUE";
            }
            parsed_flag.push_back(std::move(flag));
        });
        return parsed_flag;
    }

    void code_main(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("#include \"bm2", flags.lang_name, ".hpp\"");
        w.writeln("#include <bm2/entry.hpp>");
        w.writeln("struct Flags : bm2::Flags {");
        auto scope_flags = w.indent_scope();
        w.writeln("bm2", flags.lang_name, "::Flags bm2", flags.lang_name, "_flags;");
        w.writeln("void bind(futils::cmdline::option::Context& ctx) {");
        auto scope_bind = w.indent_scope();
        auto parsed_flag = get_flags(flags);
        w.writeln("bm2::Flags::bind(ctx);");
        for (auto&& flag : parsed_flag) {
            if (flag.type == "bool") {
                w.writeln("ctx.VarBool(&bm2", flags.lang_name, "_flags.", flag.bind_target, ", \"", flag.option, "\", \"", flag.description, "\");");
            }
            else if (flag.type == "string") {
                w.writeln("ctx.VarString(&bm2", flags.lang_name, "_flags.", flag.bind_target, ", \"", flag.option, "\", \"", flag.description, "\", \"", flag.place_holder, "\");");
            }
        }
        scope_bind.execute();
        w.writeln("}");
        scope_flags.execute();
        w.writeln("};");

        w.writeln("DEFINE_ENTRY(Flags) {");
        auto scope_entry = w.indent_scope();
        may_write_from_hook(w, flags, bm2::HookFile::entry, false);
        w.writeln("bm2", flags.lang_name, "::to_", flags.lang_name, "(w, bm,flags.bm2", flags.lang_name, "_flags);");
        w.writeln("return 0;");
        scope_entry.execute();
        w.writeln("}");
    }

    void write_code_header(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("#pragma once");
        w.writeln("#include <binary/writer.h>");
        w.writeln("#include <bm/binary_module.hpp>");
        w.writeln("namespace bm2", flags.lang_name, " {");
        auto scope = w.indent_scope();
        auto parsed_flag = get_flags(flags);
        w.writeln("struct Flags {");
        auto scope_flags = w.indent_scope();
        for (auto&& flag : parsed_flag) {
            if (flag.type != "bool" && flag.type != "string") {
                continue;
            }
            w.writeln(flag.type, " ", flag.bind_target, " = ", flag.default_value, ";");
        }
        scope_flags.execute();
        w.writeln("};");

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags);");
        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);
    }

    void write_code_cmake(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("#license");
        w.writeln("cmake_minimum_required(VERSION 3.25)");
        w.writeln("project(bm2", flags.lang_name, ")");
        w.writeln("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool)");
        w.writeln("add_executable(bm2", flags.lang_name, " main.cpp bm2", flags.lang_name, ".cpp)");
        w.writeln("target_link_libraries(bm2", flags.lang_name, " futils bm)");
        w.writeln("install(TARGETS bm2", flags.lang_name, " DESTINATION tool)");
        w.writeln("if (\"$ENV{BUILD_MODE}\" STREQUAL \"web\")");
        w.writeln("  install(FILES \"${CMAKE_BINARY_DIR}/tool/bm2", flags.lang_name, ".wasm\" DESTINATION tool)");
        w.writeln("endif()");
    }

    void write_code_js_glue_worker(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("import * as bmgen  from \"./bm2", flags.lang_name, ".js\";");
        w.writeln("import { EmWorkContext } from \"../../s2j/em_work_ctx.js\";");
        w.write_unformatted(R"(export const base64ToUint8Array = (base64) => {
    const base64Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    //const base64URLCharacters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
    let cleanedBase64 = base64.replace(/-/g, "+").replace(/_/g, "/").trim();
    const padding = (4 - (cleanedBase64.length % 4)) % 4;
    cleanedBase64 += "=".repeat(padding);
    const rawLength = cleanedBase64.length;
    const decodedLength = (rawLength * 3) / 4 - padding;
    const uint8Array = new Uint8Array(decodedLength);
    let byteIndex = 0;
    for (let i = 0; i < rawLength; i += 4) {
        const encoded1 = base64Characters.indexOf(cleanedBase64[i]);
        const encoded2 = base64Characters.indexOf(cleanedBase64[i + 1]);
        const encoded3 = base64Characters.indexOf(cleanedBase64[i + 2]);
        const encoded4 = base64Characters.indexOf(cleanedBase64[i + 3]);
        if (encoded1 < 0 || encoded2 < 0 || encoded3 < 0 || encoded4 < 0) {
            return new Error("invalid base64 string");
        }
        const decoded1 = (encoded1 << 2) | (encoded2 >> 4);
        const decoded2 = ((encoded2 & 15) << 4) | (encoded3 >> 2);
        const decoded3 = ((encoded3 & 3) << 6) | encoded4;
        uint8Array[byteIndex++] = decoded1;
        if (encoded3 !== 64)
            uint8Array[byteIndex++] = decoded2;
        if (encoded4 !== 64)
            uint8Array[byteIndex++] = decoded3;
    }
    return uint8Array;
};)");
        w.writeln("const bmgenModule = bmgen.default /*as EmscriptenModuleFactory<MyEmscriptenModule>*/;");
        std::string lang_name = flags.worker_request_name;
        if (lang_name.empty()) {
            lang_name = flags.lang_name;
        }
        w.write_unformatted(std::format(R"(
const requestCallback = (e /*JobRequest*/, m /* MyEmscriptenModule */) => {{
    switch (e.lang /*as string*/) {{
        case "{}":
            const bm = base64ToUint8Array(e.sourceCode);
            if(bm instanceof Error) {{
                return bm;
            }}
            m.FS.writeFile("/editor.bm",  bm);
            return ["bm2{}","-i", "/editor.bm"];
        default:
            return new Error("unknown message type");
    }}
}};

const bmgenWorker = new EmWorkContext(bmgenModule,requestCallback, () => {{
    console.log("bm2{} worker is ready");
}});
)",
                                        lang_name,
                                        flags.lang_name, flags.lang_name));
    }

    void write_code_js_glue_ui_and_generator_call(bm2::TmpCodeWriter& w, Flags& flags) {
        auto flag = get_flags(flags);
        auto workerName = flags.worker_name();
        auto upperWorkerName = workerName;
        upperWorkerName[0] = std::toupper(upperWorkerName[0]);
        if (flags.mode != bm2::GenerateMode::js_ui_embed) {
            upperWorkerName = "";
        }
        w.writeln("const convert", upperWorkerName, "OptionToFlags = (opt) => {");
        auto scope = w.indent_scope();
        w.writeln("const flags = [];");
        for (auto&& f : flag) {
            if (f.type == "bool") {
                w.writeln("if (opt.", f.bind_target, ") {");
                auto if_block = w.indent_scope();
                w.writeln("flags.push(\"--", f.option, "\");");
                if_block.execute();
                w.writeln("}");
            }
            else if (f.type == "string") {
                w.writeln("if (opt.", f.bind_target, " !== \"", f.default_value, "\") {");
                auto if_block = w.indent_scope();
                w.writeln("flags.push(\"--", f.option, "\", opt.", f.bind_target, ");");
                if_block.execute();
                w.writeln("}");
            }
        }
        w.writeln("return flags;");
        scope.execute();
        w.writeln("};");

        if (upperWorkerName.empty()) {
            w.writeln("export ");
        }

        w.writeln("const generate", upperWorkerName, " = async (factory,traceID,opt,sourceCode) => {");
        auto scope_generate = w.indent_scope();
        w.writeln("const worker_mgr = factory.getWorker(\"", workerName, "\");");
        w.writeln("const flags = convert", upperWorkerName, "OptionToFlags(opt);");
        w.writeln("const req = worker_mgr.getRequest(traceID,\"", workerName, "\",sourceCode,flags);");
        w.writeln("req.arguments = convert", upperWorkerName, "OptionToFlags(opt);");
        w.writeln("return worker_mgr.doRequest(req);");
        scope_generate.execute();
        w.writeln("};");

        if (upperWorkerName.empty()) {
            w.writeln("export ");
        }

        w.writeln("const convert", upperWorkerName, "UIConfigToOption = (ui) => {");
        auto scope_convert = w.indent_scope();
        w.writeln("const opt = {};");
        for (auto&& f : flag) {
            w.writeln("opt.", f.bind_target, " = ui.getLanguageConfig(\"", workerName, "\",\"", f.bind_target, "\");");
        }
        w.writeln("return opt;");
        scope_convert.execute();
        w.writeln("};");

        if (upperWorkerName.empty()) {
            w.writeln("export ");
        }

        w.writeln("function set", upperWorkerName, "UIConfig(setter) {");
        auto scope1 = w.indent_scope();
        w.writeln("setter(\"", workerName, "\",(nest_setter) => {");
        auto scope2 = w.indent_scope();
        for (auto&& f : flag) {
            w.writeln("nest_setter(\"", f.bind_target, "\",{");
            auto scope_flag = w.indent_scope();
            if (f.type == "bool") {
                w.writeln("type: \"checkbox\",");
                w.writeln("value: ", f.default_value, ",");
            }
            else if (f.type == "string") {
                w.writeln("type: \"text\",");
                w.writeln("value: \"", f.default_value, "\",");
            }
            scope_flag.execute();
            w.writeln("});");
        }
        scope2.execute();
        w.writeln("});");
        scope1.execute();
        w.writeln("}");
    }

    void write_code_config(bm2::TmpCodeWriter& w, Flags& flags) {
        auto js = futils::json::convert_to_json<futils::json::OrderedJSON>(flags);
        auto out = futils::json::to_string<std::string>(js);
        w.writeln(out);
    }

    void write_template_document(Flags& flags, bm2::TmpCodeWriter& w) {
        if (flags.mode == bm2::GenerateMode::docs_json) {
            futils::json::Stringer s;
            auto root = s.object();
            for (auto& c : flags.content) {
                root(to_string(c.first), [&](auto& s) {
                    auto obj = s.object();
                    for (auto& c2 : c.second) {
                        obj(c2.first, [&](auto& s) {
                            auto element = s.array();
                            for (auto& c3 : c2.second) {
                                element([&](auto& s) {
                                    auto field = s.object();
                                    field("var_name", c3.var_name);
                                    field("type", c3.type);
                                    field("initial_value", c3.initial_value);
                                    field("description", c3.description);
                                });
                            }
                        });
                    }
                });
            }
            root.close();
            w.write(s.out());
        }
        else if (flags.mode == bm2::GenerateMode::docs_markdown) {
            w.writeln("# Template Document");
            w.writeln("This document describes the variables that can be used in the code generator-generator hooks.");
            w.writeln("Code generator-generator hooks are pieces of C++ code that are inserted into the generator code.");
            w.writeln("## Words");
            w.writeln("### reference");
            w.writeln("A reference to a several object in the binary module.");
            w.writeln("They are represented as rebgn::Varint. they are not supposed to be used directly.");
            w.writeln("### type reference");
            w.writeln("A reference to a type in the binary module.");
            w.writeln("They are represented as rebgn::StorageRef. they are not supposed to be used directly.");
            w.writeln("### identifier");
            w.writeln("An identifier of a several object (e.g. function, variable, types, etc.)");
            w.writeln("They are represented as std::string. use them for generating code.");
            w.writeln("### EvalResult");
            w.writeln("result of eval() function. it contains the result of the expression evaluation.");
            for (auto& c : flags.content) {
                w.writeln("## function `", to_string(c.first), "`");
                for (auto& c2 : c.second) {
                    w.writeln("### ", c2.first);
                    for (auto& c3 : c2.second) {
                        w.writeln("#### ", c3.var_name);
                        w.writeln("Type: ", c3.type);
                        w.writeln("Initial Value: ", c3.initial_value);
                        w.writeln("Description: ", c3.description);
                    }
                }
            }
        }
        else {
            futils::wrap::cerr_wrap() << "invalid template docs format\n";
        }
    }

}  // namespace rebgn