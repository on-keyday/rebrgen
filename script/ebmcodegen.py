import subprocess as sp
import sys
import os
import argparse

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


def execute(command, env, capture=True) -> bytes:
    passEnv = os.environ.copy()
    if env is not None:
        passEnv.update(env)
    if capture:
        return sp.check_output(command, env=passEnv, stderr=sys.stderr)
    else:
        return sp.check_call(command, env=passEnv, stdout=sys.stdout, stderr=sys.stderr)


TOOL_PATH = "tool/ebmcodegen"
if mode == "interpret":
    PARENT_DIR_NAME = "ebmip"
else:
    PARENT_DIR_NAME = "ebmcg"

PARENT_CMAKE_PATH = f"src/{PARENT_DIR_NAME}/CMakeLists.txt"
OUTPUT_DIR = f"src/{PARENT_DIR_NAME}/ebm2{lang_name}"

CMAKE = execute([TOOL_PATH, "--mode", "cmake", "--lang", lang_name], None)
CODE_GENERATOR = execute([TOOL_PATH, "--mode", mode, "--lang", lang_name], None)

os.makedirs(OUTPUT_DIR, exist_ok=True)
VISITOR_DIR = os.path.join(OUTPUT_DIR, "visitor")
os.makedirs(VISITOR_DIR, exist_ok=True)

with open(os.path.join(OUTPUT_DIR, "CMakeLists.txt"), "wb") as f:
    f.write(CMAKE)
with open(os.path.join(OUTPUT_DIR, "main.cpp"), "wb") as f:
    f.write(CODE_GENERATOR)

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
