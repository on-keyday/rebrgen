import subprocess as sp
import sys
import os

if len(sys.argv) < 2:
    print("Usage: python update_ebm.py <lang_name>")
    sys.exit(1)
lang_name = sys.argv[1]


def execute(command, env, capture=True) -> bytes:
    passEnv = os.environ.copy()
    if env is not None:
        passEnv.update(env)
    if capture:
        return sp.check_output(command, env=passEnv, stderr=sys.stderr)
    else:
        return sp.check_call(command, env=passEnv, stdout=sys.stdout, stderr=sys.stderr)


TOOL_PATH = "tool/ebmcodegen"
PARENT_CMAKE_PATH = "src/ebmcg/CMakeLists.txt"
OUTPUT_DIR = "src/ebmcg/ebm2" + lang_name
CMAKE = execute([TOOL_PATH, "--mode", "cmake", "--lang", lang_name], None)
CODE_GENERATOR = execute([TOOL_PATH, "--mode", "codegen", "--lang", lang_name], None)

os.makedirs(OUTPUT_DIR, exist_ok=True)
VISITOR_DIR = os.path.join(OUTPUT_DIR, "visitor")
os.makedirs(VISITOR_DIR, exist_ok=True)

with open(os.path.join(OUTPUT_DIR, "CMakeLists.txt"), "wb") as f:
    f.write(CMAKE)
with open(os.path.join(OUTPUT_DIR, "main.cpp"), "wb") as f:
    f.write(CODE_GENERATOR)

with open(PARENT_CMAKE_PATH, "r") as f:
    content = f.read()

if content.find(f"add_subdirectory(ebm2{lang_name})") != -1:
    print(
        f"Directory ebm2{lang_name} already exists in {PARENT_CMAKE_PATH}. Skipping update."
    )
else:
    with open(PARENT_CMAKE_PATH, "w") as f:
        f.write(content + f"\nadd_subdirectory(ebm2{lang_name})\n")
    print(f"Added subdirectory ebm2{lang_name} to {PARENT_CMAKE_PATH}")

print("Code generation completed successfully.")
print(f"Generated files are located in: {OUTPUT_DIR}")
