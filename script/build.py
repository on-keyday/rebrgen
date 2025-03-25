import os
import subprocess
import sys

print("Building...", sys.argv)
BUILD_TYPE = sys.argv[2] if len(sys.argv) > 2 else "Debug"
BUILD_MODE = sys.argv[1] if len(sys.argv) > 1 else "native"
INSTALL_PREFIX = "."
FUTILS_DIR = os.getenv("FUTILS_DIR", "C:/workspace/utils_backup")

print("BUILD_TYPE:", BUILD_TYPE)
print("BUILD_MODE:", BUILD_MODE)
print("INSTALL_PREFIX:", INSTALL_PREFIX)
print("FUTILS_DIR:", FUTILS_DIR)

shell = os.getenv("SHELL")
if os.name == "posix":
    EMSDK_PATH = os.getenv("EMSDK_PATH", "C:/workspace/emsdk/emsdk_env.sh")
elif os.name == "nt":
    EMSDK_PATH = os.getenv("EMSDK_PATH", "C:/workspace/emsdk/emsdk_env.ps1")
else:
    EMSDK_PATH = os.getenv("EMSDK_PATH", "C:/workspace/emsdk/emsdk_env.sh")

os.environ["FUTILS_DIR"] = FUTILS_DIR
os.environ["BUILD_MODE"] = BUILD_MODE


def source_emsdk():
    copyEnv = os.environ.copy()
    copyEnv["EMSDK_QUIET"] = "1"
    if os.name == "posix":
        ENV = subprocess.check_output(
            ["source", EMSDK_PATH, ">", "/dev/null", ";", "env"],
            shell=True,
            env=copyEnv,
            stderr=sys.stderr,
        )
    elif os.name == "nt":
        ENV = subprocess.check_output(
            [
                "powershell",
                "$OutputEncoding = [System.Text.Encoding]::UTF8",
                ";",
                ".",
                EMSDK_PATH,
                "|",
                "Out-Null",
                ";",
                "Get-ChildItem Env:",
                "|",
                "ForEach-Object",
                "{",
                "$_.Name + '=' + $_.Value}",
            ],
            env=copyEnv,
            stderr=sys.stderr,
        )
    else:
        print("Unsupported environment")
        sys.exit(1)
    for line in ENV.splitlines():
        try:
            line = line.decode()
        except UnicodeDecodeError as e:
            print(e.start, e.end, line[e.start - 10 : e.end + 10])
            raise e
        if "=" in line:
            key, value = line.split("=", 1)
            os.environ[key] = value


if BUILD_MODE == "native":
    subprocess.run(
        [
            "cmake",
            "-D",
            "CMAKE_CXX_COMPILER=clang++",
            "-D",
            "CMAKE_C_COMPILER=clang",
            "-G",
            "Ninja",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}",
            f"-D",
            f"CMAKE_BUILD_TYPE={BUILD_TYPE}",
            "-S",
            ".",
            f"-B",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["cmake", "--build", f"./built/{BUILD_MODE}/{BUILD_TYPE}"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "Unspecified",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["cmake", "--install", "./built/native/Debug", "--component", "gen_template"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
elif BUILD_MODE == "web":
    source_emsdk()
    subprocess.run(
        [
            "emcmake",
            "cmake",
            "-G",
            "Ninja",
            f"-D",
            f"CMAKE_BUILD_TYPE={BUILD_TYPE}",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}/web",
            "-S",
            ".",
            f"-B",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
        ],
        shell=True,
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["cmake", "--build", f"./built/{BUILD_MODE}/{BUILD_TYPE}"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "Unspecified",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
else:
    print("Invalid build mode")
