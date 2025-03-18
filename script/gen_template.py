import subprocess as sp
import os
import sys

LANG_NAME = sys.argv[1]

# cmake --build ./built/native/Debug --target gen_template
DEFAULT_FUTILS_DIR = (
    "C:/workspace/utils_backup" if sys.platform == "win32" else "./utils"
)


def execute(command, env) -> bytes:
    return sp.check_output(command, env=env, check=True, stderr=sys.stderr)


execute(
    ["cmake", "--build", "./built/native/Debug", "--target", "gen_template"],
    env={
        "FUTILS_DIR": os.getenv(
            "FUTILS_DIR",
            DEFAULT_FUTILS_DIR,
        ),
    },
)

# cmake --install ./built/native/Debug --component gen_template
execute(
    ["cmake", "--install", "./built/native/Debug", "--component", "gen_template"],
    env={
        "FUTILS_DIR": os.getenv(
            "FUTILS_DIR",
            DEFAULT_FUTILS_DIR,
        ),
    },
)

# .\tool\gen_template --lang %LANG_NAME% --header --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\bm2%LANG_NAME%.hpp
HEADER = execute(
    [
        "./tool/gen_template",
        "--lang",
        LANG_NAME,
        "--mode",
        "header",
        "--hook-dir",
        f"src/bm2{LANG_NAME}/hook",
    ],
    None,
)

# .\tool\gen_template --config-file src\bm2%LANG_NAME%\config.json --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\bm2%LANG_NAME%.cpp
GENERATOR = execute(
    [
        "./tool/gen_template",
        "--mode",
        "generator",
        "--config-file",
        f"src/bm2{LANG_NAME}/config.json",
        "--hook-dir",
        f"src/bm2{LANG_NAME}/hook",
    ],
    None,
)


# .\tool\gen_template --lang %LANG_NAME% --main --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION%> src\bm2%LANG_NAME%\main.cpp
MAIN = execute(
    [
        "./tool/gen_template",
        "--lang",
        LANG_NAME,
        "--mode",
        "main",
        "--hook-dir",
        f"src/bm2{LANG_NAME}/hook",
    ],
    None,
)

# .\tool\gen_template --lang %LANG_NAME% --cmake --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\CMakeLists.txt
CMAKE = execute(
    [
        "./tool/gen_template",
        "--lang",
        LANG_NAME,
        "--mode",
        "cmake",
        "--hook-dir",
        f"src/bm2{LANG_NAME}/hook",
    ],
    None,
)

with open(f"src/bm2{LANG_NAME}/bm2{LANG_NAME}.hpp", "wb") as f:
    f.write(HEADER)

with open(f"src/bm2{LANG_NAME}/bm2{LANG_NAME}.cpp", "wb") as f:
    f.write(GENERATOR)

with open(f"src/bm2{LANG_NAME}/main.cpp", "wb") as f:
    f.write(MAIN)

with open(f"src/bm2{LANG_NAME}/CMakeLists.txt", "wb") as f:
    f.write(CMAKE)
