/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include <strutil/splits.h>
#include "glue_code.hpp"

namespace rebgn {

    std::vector<Flag> get_flags(Flags& flags) {
        std::vector<Flag> parsed_flag;
        may_write_from_hook(flags, bm2::HookFile::flags, [&](size_t i, futils::view::rvec line, bool is_last) {
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
        w.writeln("// Code generated by gen_template, DO NOT EDIT.");
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
        may_write_from_hook(w, flags, bm2::HookFile::flags, bm2::HookFileSub::pre_main);
        scope_bind.execute();
        w.writeln("}");
        scope_flags.execute();
        w.writeln("};");

        w.writeln("DEFINE_ENTRY(Flags,bm2", flags.lang_name, "::Output) {");
        auto scope_entry = w.indent_scope();
        may_write_from_hook(w, flags, bm2::HookFile::entry, false);
        w.writeln("bm2", flags.lang_name, "::to_", flags.lang_name, "(w, bm,flags.bm2", flags.lang_name, "_flags,output);");
        may_write_from_hook(w, flags, bm2::HookFile::entry, bm2::HookFileSub::after);
        w.writeln("return 0;");
        scope_entry.execute();
        w.writeln("}");
    }

    void write_code_header(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("/*license*/");
        w.writeln("// Code generated by gen_template, DO NOT EDIT.");
        w.writeln("#pragma once");
        w.writeln("#include <binary/writer.h>");
        w.writeln("#include <bm/binary_module.hpp>");
        w.writeln("#include <bm2/output.hpp>");
        may_write_from_hook(w, flags, bm2::HookFile::generator_top, bm2::HookFileSub::header);
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
        may_write_from_hook(w, flags, bm2::HookFile::flags, bm2::HookFileSub::header);
        scope_flags.execute();
        w.writeln("};");

        w.writeln("struct Output : bm2::Output {");
        auto scope_output = w.indent_scope();
        may_write_from_hook(w, flags, bm2::HookFile::outputs, false);
        scope_output.execute();
        w.writeln("};");

        w.writeln("void to_", flags.lang_name, "(::futils::binary::writer& w, const rebgn::BinaryModule& bm, const Flags& flags,Output& output);");
        scope.execute();
        w.writeln("}  // namespace bm2", flags.lang_name);
    }

    void write_code_cmake(bm2::TmpCodeWriter& w, Flags& flags) {
        w.writeln("#license");
        w.writeln("# Code generated by gen_template, DO NOT EDIT.");
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
        w.writeln("/*license*/");
        w.writeln("// Code generated by gen_template, DO NOT EDIT.");
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
        std::string lang_name = flags.worker_name();
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
)",
                                        lang_name,
                                        flags.lang_name));
        w.write_unformatted(std::format(R"(const bmgenWorker = new EmWorkContext(bmgenModule,requestCallback, () => {{
    console.log("bm2{} worker is ready");
}});)",
                                        flags.lang_name));
    }

    void write_code_js_glue_ui_and_generator_call(bm2::TmpCodeWriter& w, Flags& flags) {
        if (flags.mode != bm2::GenerateMode::js_ui_embed) {
            w.writeln("/*license*/");
            w.writeln("// Code generated by gen_template, DO NOT EDIT.");
        }
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
        w.writeln("const req = worker_mgr.getRequest(traceID,\"", workerName, "\",sourceCode);");
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

        w.writeln("function set", upperWorkerName, "UIConfig(ui) {");
        auto scope1 = w.indent_scope();
        may_write_from_hook(w, flags, bm2::HookFile::js_ui, bm2::HookFileSub::before);
        if (!may_write_from_hook(w, flags, bm2::HookFile::js_ui, false)) {
            w.writeln("ui.set_flags(\"", workerName, "\",(nest_setter) => {");
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
        }
        may_write_from_hook(w, flags, bm2::HookFile::js_ui, bm2::HookFileSub::after);
        scope1.execute();
        w.writeln("}");
    }

    void write_code_config(bm2::TmpCodeWriter& w, Flags& flags) {
        try {
            printf("write config\n");
            auto js = futils::json::convert_to_json<futils::json::OrderedJSON>(flags);
            printf("convert to json\n");
            auto out = futils::json::to_string<std::string>(js);
            w.writeln(out);
        } catch (std::exception& e) {
            printf("error: %s\n", e.what());
            throw;
        }
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
                            obj("variables", [&](auto& s) {
                                auto element = s.array();
                                for (auto& c3 : c2.second.variables) {
                                    element([&](auto& s) {
                                        auto field = s.object();
                                        field("var_name", c3.var_name);
                                        field("type", c3.type);
                                        field("initial_value", c3.initial_value);
                                        field("description", c3.description);
                                    });
                                }
                            });
                            obj("env_mappings", [&](auto& s) {
                                auto element = s.array();
                                for (auto& c3 : c2.second.env_mappings) {
                                    element([&](auto& s) {
                                        auto field = s.object();
                                        field("variable_name", c3.variable_name);
                                        field("mapping", c3.mapping);
                                        field("file_name", c3.file_name);
                                        field("func_name", c3.func_name);
                                        field("line", c3.line);
                                    });
                                }
                            });
                            obj("flag_usage_mappings", [&](auto& s) {
                                auto element = s.array();
                                for (auto& c3 : c2.second.flag_usage_mappings) {
                                    element([&](auto& s) {
                                        auto field = s.object();
                                        field("flag_name", c3.flag_name);
                                        field("flag_value", c3.flag_value);
                                        field("file_name", c3.file_name);
                                        field("func_name", c3.func_name);
                                        field("line", c3.line);
                                    });
                                }
                            });
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
            w.writeln("### placeholder");
            w.writeln("in some case, code generator placeholder in `config.json` can be replaced with context specific value by envrionment variable like format.");
            w.writeln("for example, config name `int_type` can take two env map `BIT_SIZE` and `ALIGNED_BIT_SIZE`.");
            w.writeln("and use like below:");
            w.writeln("```json");
            w.writeln("{");
            w.writeln("    \"int_type\": \"std::int${ALIGNED_BIT_SIZE}_t\"");
            w.writeln("}");
            w.writeln("```");
            w.writeln("and if you set `ALIGNED_BIT_SIZE` to 32, then `int_type` will be `std::int32_t`.");
            w.writeln("specially, `$DOLLAR` or `${DOLLAR}` is reserved for `$` character.");

            for (auto& c : flags.content) {
                w.writeln("## function `", to_string(c.first), "`");
                for (auto& c2 : c.second) {
                    w.writeln("### ", c2.first);
                    w.writeln("#### Variables: ");
                    for (auto& c3 : c2.second.variables) {
                        w.writeln("##### ", c3.var_name);
                        w.writeln("Type: ", c3.type);
                        w.writeln("Initial Value: ", c3.initial_value);
                        w.writeln("Description: ", c3.description);
                    }
                    w.writeln("#### Placeholder Mappings: ");
                    for (auto& c3 : c2.second.env_mappings) {
                        w.writeln("##### ", c3.variable_name);
                        for (auto& [k, v] : c3.mapping) {
                            w.writeln("Name: ", k);
                            w.writeln("Mapped Value: `", v, "`");
                            w.writeln("File: ", std::format("{}:{}", c3.file_name, c3.line));
                            w.writeln("Function Name: ", c3.func_name);
                        }
                    }
                    w.writeln("#### Flag Usage Mappings: ");
                    for (auto& c3 : c2.second.flag_usage_mappings) {
                        w.writeln("##### ", c3.flag_name);
                        w.writeln("Flag Value: `", c3.flag_value, "`");
                        w.writeln("File: ", std::format("{}:{}", c3.file_name, c3.line));
                        w.writeln("Function Name: ", c3.func_name);
                    }
                }
            }
        }
        else {
            futils::wrap::cerr_wrap() << "invalid template docs format\n";
        }
    }

    void write_cmptest_config(Flags& flags, bm2::TmpCodeWriter& w) {
        if (flags.mode == bm2::GenerateMode::cmptest_json) {
            futils::json::Stringer s;
            auto root = s.object();
            root("suffix", flags.file_suffix);
            root("test_template", "./testkit/" + flags.lang_name + "/test_template." + flags.file_suffix);
            root("replace_file_name", "stub." + flags.file_suffix);
            root("replace_struct_name", "TEST_CLASS");
            root("build_output_name", "build_output");
            root("build_input_name", "main." + flags.file_suffix);
            root("build_command",
                 std::vector<std::string>{
                     "python",
                     "./testkit/" + flags.lang_name + "/setup.py",
                     "build",
                     "$INPUT",
                     "$OUTPUT",
                     "$ORIGIN",
                     "$TMPDIR",
                     "$DEBUG",
                     "$CONFIG"});
            root("run_command",
                 std::vector<std::string>{
                     "python",
                     "./testkit/" + flags.lang_name + "/setup.py",
                     "run",
                     "$EXEC",
                     "$INPUT",
                     "$OUTPUT",
                 });
            root.close();
            w.write(s.out());
        }
        else if (flags.mode == bm2::GenerateMode::cmptest_build) {
            w.writeln("#!/usr/bin/env python3");
            w.writeln("# Code generated by gen_template, DO NOT EDIT.");
            w.writeln("import sys");
            w.writeln("import os");
            w.writeln("import subprocess");
            w.writeln("import shutil");
            w.writeln("import pathlib as pl");
            w.writeln("import json");
            w.writeln();
            w.writeln("def run_command(args):");
            auto scope_command = w.indent_scope();
            w.writeln("print(\"run command: \", args)");
            w.writeln("ret = subprocess.call(args,stdout = sys.stdout, stderr = sys.stderr)");
            w.writeln("if ret != 0:");
            auto scope_rc_if = w.indent_scope();
            w.writeln("print(f\"Command failed with exit code {ret}\")");
            w.writeln("exit(ret)");
            scope_rc_if.execute();
            scope_command.execute();
            w.writeln();
            w.writeln("def capture_command(args):");
            auto scope_capture = w.indent_scope();
            w.writeln("print(\"run command with capture: \", args)");
            w.writeln("ret = subprocess.run(args,stderr = sys.stderr,stdout = subprocess.PIPE)");
            w.writeln("if ret.returncode != 0:");
            auto scope_capture_if = w.indent_scope();
            w.writeln("print(f\"Command failed with exit code {ret.returncode}\")");
            w.writeln("exit(ret.returncode)");
            scope_capture_if.execute();
            w.writeln("return ret.stdout");
            scope_capture.execute();
            w.writeln();
            w.writeln("def copy_file(src, dst):");
            auto scope_copy = w.indent_scope();
            w.writeln("print(\"copy file: {} -> {}\".format(src, dst))");
            w.writeln("shutil.copyfile(src, dst)");
            scope_copy.execute();
            w.writeln("def copy_dir(src, dst):");
            auto scope_copy2 = w.indent_scope();
            w.writeln("print(\"copy dir: {} -> {}\".format(src, dst))");
            w.writeln("shutil.copytree(src, dst, dirs_exist_ok=True)");
            scope_copy2.execute();
            w.writeln("def move_file(src, dst):");
            auto scope_move = w.indent_scope();
            w.writeln("print(\"move file: {} -> {}\".format(src, dst))");
            w.writeln("shutil.move(src, dst)");
            scope_move.execute();
            w.writeln("def move_dir(src, dst):");
            auto scope_move2 = w.indent_scope();
            w.writeln("print(\"move dir: {} -> {}\".format(src, dst))");
            w.writeln("shutil.move(src, dst)");
            scope_move2.execute();

            w.writeln("def write_file(filename, content):");
            auto scope_write = w.indent_scope();
            w.writeln("print(\"write file: {}\".format(filename))");
            w.writeln("with open(filename, \"w\") as f:");
            auto scope_write2 = w.indent_scope();
            w.writeln("f.write(content)");
            scope_write2.execute();
            scope_write.execute();
            w.writeln("def read_file(filename):");
            auto scope_read = w.indent_scope();
            w.writeln("print(\"read file: {}\".format(filename))");
            w.writeln("with open(filename, \"r\") as f:");
            auto scope_read2 = w.indent_scope();
            w.writeln("return f.read()");
            scope_read2.execute();
            scope_read.execute();

            w.writeln("def get_env(name,default_ = ''):");
            auto scope_env = w.indent_scope();
            w.writeln("print(\"get env: {}\".format(name))");
            w.writeln("return os.environ.get(name, default_)");
            scope_env.execute();
            w.writeln("def get_env_int(name,default_ = 0):");
            auto scope_env_int = w.indent_scope();
            w.writeln("print(\"get env int: {}\".format(name))");
            w.writeln("return int(os.environ.get(name, default_))");
            scope_env_int.execute();
            w.writeln("def get_env_bool(name,default_ = False):");
            auto scope_env_bool = w.indent_scope();
            w.writeln("print(\"get env bool: {}\".format(name))");
            w.writeln("return os.environ.get(name, default_) == \"true\" or os.environ.get(name, default_) == \"True\"");
            scope_env_bool.execute();
            w.writeln("if __name__ == \"__main__\":");
            auto scope_main = w.indent_scope();
            w.writeln("MODE = sys.argv[1]");
            w.writeln("if MODE == \"build\":");
            auto scope = w.indent_scope();
            w.writeln("MAIN = sys.argv[2]");
            w.writeln("EXEC = sys.argv[3]");
            w.writeln("GENERATED = sys.argv[4]");
            w.writeln("TMPDIR = sys.argv[5]");
            w.writeln("DEBUG = sys.argv[6]");
            w.writeln("CONFIG = sys.argv[7]");
            w.writeln("CONFIG_DIR = os.path.dirname(CONFIG)");
            if (!may_write_from_hook(flags, bm2::HookFile::cmptest_build, [&](size_t i, futils::view::rvec line, bool is_last) {
                    w.writeln(line);
                })) {
                w.writeln("print(\"You have to implement build command in hook/cmptest_build.txt. see also testkit/", flags.lang_name, "/setup.py\")");
                w.writeln("exit(1)");
            }
            scope.execute();
            w.writeln("elif MODE == \"run\":");
            auto scope2 = w.indent_scope();
            w.writeln("EXEC = sys.argv[2]");
            w.writeln("INPUT = sys.argv[3]");
            w.writeln("OUTPUT = sys.argv[4]");
            if (!may_write_from_hook(flags, bm2::HookFile::cmptest_run, [&](size_t i, futils::view::rvec line, bool is_last) {
                    w.writeln(line);
                })) {
                w.writeln("exit(run_command([EXEC,INPUT,OUTPUT]))");
            }
            scope2.execute();
            w.writeln("else:");
            auto scope3 = w.indent_scope();
            w.writeln("print(\"invalid mode\")");
            w.writeln("exit(1)");
            scope3.execute();
            w.writeln();
        }
    }
}  // namespace rebgn
