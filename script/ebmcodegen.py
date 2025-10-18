import subprocess as sp
import sys
import os
import argparse
import json

parser = argparse.ArgumentParser(description="Generate code generator or interpreter.")
parser.add_argument("lang_name", help="Language name")
parser.add_argument(
    "--mode",
    choices=["codegen", "interpret"],
    default="codegen",
    help="Mode of operation",
)
args = parser.parse_args()

lang_name = args.lang_name
mode = args.mode


sys.path.append(os.path.dirname(__file__))
from util import execute

TOOL_PATH = "tool/ebmcodegen"
if mode == "interpret":
    PARENT_DIR_NAME = "ebmip"
else:
    PARENT_DIR_NAME = "ebmcg"

PARENT_CMAKE_PATH = f"src/{PARENT_DIR_NAME}/CMakeLists.txt"
OUTPUT_DIR = f"src/{PARENT_DIR_NAME}/ebm2{lang_name}"
TEST_CONFIG_PATH = "test/unictest.json"

CMAKE = execute([TOOL_PATH, "--mode", "cmake", "--lang", lang_name], None)
CODE_GENERATOR = execute([TOOL_PATH, "--mode", mode, "--lang", lang_name], None)

os.makedirs(OUTPUT_DIR, exist_ok=True)
VISITOR_DIR = os.path.join(OUTPUT_DIR, "visitor")
os.makedirs(VISITOR_DIR, exist_ok=True)

with open(os.path.join(OUTPUT_DIR, "CMakeLists.txt"), "wb") as f:
    f.write(CMAKE)
with open(os.path.join(OUTPUT_DIR, "main.cpp"), "wb") as f:
    f.write(CODE_GENERATOR)
# add test script file if not exists
TEST_SCRIPT_PATH = os.path.join(OUTPUT_DIR, "unictest.py")
if not os.path.exists(TEST_SCRIPT_PATH):
    with open(TEST_SCRIPT_PATH, "w") as f:
        f.write("#!/usr/bin/env python3\n")
        f.write("# Test logic for ebm2" + lang_name + "\n")
        f.write("import sys\n")
        f.write("def main():\n")
        f.write("    TEST_TARGET_FILE = sys.argv[1]\n")
        f.write("    INPUT_FILE = sys.argv[2]\n")
        f.write("    OUTPUT_FILE = sys.argv[3]\n")
        f.write("    TEST_TARGET_FORMAT = sys.argv[4]\n")
        f.write("    # Test logic goes here\n")
        f.write(
            "    print(f'Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}')\n"
        )
        f.write("    # This is a placeholder for actual test implementation\n")
        f.write("    with open(INPUT_FILE, 'rb') as f:\n")
        f.write("        data = f.read()\n")
        f.write("    # For demonstration, just write the same data to output\n")
        f.write("    with open(OUTPUT_FILE, 'wb') as f:\n")
        f.write("        f.write(data)\n")
        f.write("if __name__ == '__main__':\n")
        f.write("    main()\n")
    print(f"Created test script: {TEST_SCRIPT_PATH}")
else:
    print(f"Test script already exists: {TEST_SCRIPT_PATH}")

with open(TEST_CONFIG_PATH, "r") as f:
    test_config = json.load(f)

runners = test_config.get("runners", [])

if not any(r["name"] == "ebm2" + lang_name for r in runners):
    new_name = "ebm2" + lang_name
    new_runner = {
        "name": new_name,
        "source_setup_command": [
            "python",
            "$WORK_DIR/script/unictest_setup.py",
            "setup",
            new_name,
            "txt",
        ],
        "run_command": [
            "python",
            "$WORK_DIR/script/unictest_setup.py",
            "test",
            new_name,
            "txt",
        ],
    }
    runners.append(new_runner)
    test_config["runners"] = runners
    with open(TEST_CONFIG_PATH, "w") as f:
        json.dump(test_config, f, indent=4)
    print(f"Added runner for ebm2{lang_name} to {TEST_CONFIG_PATH}")
else:
    print(f"Runner for ebm2{lang_name} already exists in {TEST_CONFIG_PATH}")


if not os.path.exists(PARENT_CMAKE_PATH):
    os.makedirs(os.path.dirname(PARENT_CMAKE_PATH), exist_ok=True)
    with open(PARENT_CMAKE_PATH, "w") as f:
        f.write("")

with open(PARENT_CMAKE_PATH, "r") as f:
    content = f.read()

if content.find(f"add_subdirectory(ebm2{lang_name})") != -1:
    print(
        f"Directory ebm2{lang_name} already exists in {PARENT_CMAKE_PATH}. Skipping update."
    )
else:
    with open(PARENT_CMAKE_PATH, "w") as f:
        new_content = content + f"\nadd_subdirectory(ebm2{lang_name})\n"
        new_content = "\n".join(
            [x.strip() for x in new_content.splitlines() if x.strip() != ""]
        )
        f.write(new_content)
    print(f"Added subdirectory ebm2{lang_name} to {PARENT_CMAKE_PATH}")

print("Code generation completed successfully.")
print(f"Generated files are located in: {OUTPUT_DIR}")
