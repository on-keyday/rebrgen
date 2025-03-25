import subprocess as sp
import os
import json


def search_config_files(root_dir):
    config_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "config.json":
                config_files.append(os.path.join(root, file))
    return config_files


def generate_cmptest_glue(config_file):
    hook_dir = os.path.join(os.path.dirname(config_file), "hook")
    CONFIG = dict[str, str](
        json.loads(
            sp.check_output(
                [
                    "tool/gen_template",
                    "--config-file",
                    config_file,
                    "--mode",
                    "config",
                ]
            )
        )
    )
    LANG_NAME = CONFIG["lang"]
    CMPTEST_JSON = dict[str, object](
        json.loads(
            sp.check_output(
                [
                    "tool/gen_template",
                    "--config-file",
                    config_file,
                    "--mode",
                    "cmptest-json",
                    "--hook-dir",
                    hook_dir,
                ]
            )
        )
    )
    CMPTEST_PY = sp.check_output(
        [
            "tool/gen_template",
            "--config-file",
            config_file,
            "--mode",
            "cmptest-build",
            "--hook-dir",
            hook_dir,
        ]
    )
    return LANG_NAME, CMPTEST_JSON, CMPTEST_PY


def generate_cmptest_glue_files(config_dir, output_dir):
    config_files = search_config_files(config_dir)
    conf_names = []
    for config_file in config_files:
        LANG_NAME, CMPTEST_JSON, CMPTEST_PY = generate_cmptest_glue(config_file)
        os.makedirs(os.path.join(output_dir, LANG_NAME), exist_ok=True)
        conf_name = os.path.join(output_dir, LANG_NAME, f"cmptest.json")
        with open(conf_name, "w") as f:
            json.dump(CMPTEST_JSON, f, indent=4)
        with open(os.path.join(output_dir, LANG_NAME, f"setup.py"), "wb") as f:
            f.write(CMPTEST_PY)
        conf_names.append(conf_name)
    INPUTS = []
    if os.path.exists(os.path.join(output_dir, "inputs.json")):
        with open(os.path.join(output_dir, "inputs.json"), "r") as f:
            INPUTS = json.load(f)
    with open(os.path.join(output_dir, "cmptest.json"), "w") as f:
        json.dump(
            {"inputs": INPUTS, "runners": [{"file": x} for x in conf_names]},
            f,
            indent=4,
        )


generate_cmptest_glue_files("src", "testkit")
