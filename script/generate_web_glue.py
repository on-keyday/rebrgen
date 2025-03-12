import subprocess as sp
import os
import json


def generate_web_glue(config_file):
    CONFIG = dict[str, str](
        json.loads(
            sp.check_output(
                ["tool/gen_template", "--config-file", config_file, "--config"]
            )
        )
    )
    LANG_NAME = str(
        CONFIG["worker_request_name"]
        if CONFIG["worker_request_name"] != ""
        else CONFIG["lang"]
    )
    print(f"Generating web glue for {LANG_NAME} ({config_file})")
    LANG_NAME = LANG_NAME[0].upper() + LANG_NAME[1:]
    UI_CODE = sp.check_output(
        ["tool/gen_template", "--config-file", config_file, "--js-glue", "ui-embed"]
    )
    WORKER_CODE = sp.check_output(
        ["tool/gen_template", "--config-file", config_file, "--js-glue", "worker"]
    )
    CALL_WORKER_FUNC = "generate" + LANG_NAME
    CALL_UI_FUNC = "set" + LANG_NAME + "UIConfig"
    CALL_UI_TO_OPT_FUNC = "convert" + LANG_NAME + "UIConfigToOption"
    return {
        "lang_name": LANG_NAME,
        "worker_name": "bm2" + CONFIG["lang"],
        "ui_code": UI_CODE,
        "worker_code": WORKER_CODE,
        "call_worker_func": CALL_WORKER_FUNC,
        "call_ui_func": CALL_UI_FUNC,
        "call_ui_to_opt_func": CALL_UI_TO_OPT_FUNC,
    }


def search_config_files(root_dir):
    config_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "config.json":
                config_files.append(os.path.join(root, file))
    return config_files


def generate_web_glue_files(config_dir, output_dir):
    config_files = search_config_files(config_dir)
    UI_GLUE = b""
    UI_CALLS = b""
    for config_file in config_files:
        web_glue = generate_web_glue(config_file)
        with open(f"{output_dir}/{web_glue['worker_name']}_worker.js", "wb") as f:
            f.write(web_glue["worker_code"])
        UI_GLUE += web_glue["ui_code"]
        UI_CALLS += f"case {web_glue['lang_name']}: return {web_glue['call_worker_func']}(factory,traceID,{web_glue["call_ui_to_opt_func"]}(ui),sourceCode);".encode()
    with open(f"{output_dir}/ui_embed.js", "wb") as f:
        f.write(UI_GLUE)
        f.write(
            b"function (ui,traceID,lang,sourceCode) {"
            + b"switch(lang) {"
            + UI_CALLS
            + b"default: throw new Error('Unsupported language: ' + lang);"
            + b"}"
            + b"}"
            + b"export {getWorker};"  # export the function
        )


if __name__ == "__main__":
    generate_web_glue_files("src", "web")
