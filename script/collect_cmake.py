import os
import pathlib as pl
import json


def collect_cmake_files(root_dir):
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if root == root_dir:
                continue
            if root == "src/bm2" or root == "src/bmgen":
                continue
            if file == "CMakeLists.txt":
                obj = {
                    "name": pl.Path(os.path.join(root)).relative_to("src").as_posix()
                }
                obj["lang"] = obj["name"][3:]
                configPath = os.path.join(root, "config.json")
                obj["has_config"] = pl.Path(configPath).exists()
                if obj["has_config"]:
                    with open(configPath, "r") as f:
                        print(f"Loading config: {configPath}")
                        obj["config"] = json.load(f)
                        if obj["config"].get("suffix") is not None:
                            obj["suffix"] = obj["config"]["suffix"]
                        else:
                            obj["suffix"] = obj["lang"]
                else:
                    obj["suffix"] = obj["lang"]
                cmake_files.append(obj)
    return cmake_files


src = collect_cmake_files("src")

#print(json.dumps(src))
#exit(0)

output = "src/CMakeLists.txt"

with open(output, "w") as f:
    for file in src:
        f.write(f'add_subdirectory("{file["name"]}")\n')

output = "script/run_generated.py"
with open(output, "w") as f:
    f.write("import os\n")
    f.write("import subprocess as sp\n")
    f.write("import sys\n")
    f.write("\n")
    f.write("sp.run(\n")
    f.write("    [\"script/build\"],\n")
    f.write("    check=True,\n")
    f.write("    shell=True,\n")
    f.write("    stdout=sys.stdout,\n")
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write("save = sp.check_output(\n")
    f.write("    [\"tool/bmgen\", \"-p\", \"-i\", \"save/sample.json\", \"-o\", \"save/save.bin\", \"-c\", \"save/save.dot\"],\n")
    f.write("    stdout=sys.stdout,\n")
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write("with open(\"save/save.txt\", \"wb\") as f:\n")
    f.write("    f.write(save)\n")
    f.write("save = sp.check_output(\n")
    f.write("    [\"tool/bmgen\", \"-p\", \"-i\", \"save/sample.json\", \"--print-only-op\"],\n")
    f.write("    stdout=sys.stdout,\n")
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write("with open(\"save/save_op.txt\", \"wb\") as f:\n")
    f.write("    f.write(save)\n")
    f.write("\n")
    
    for file in src:
        if file["lang"] == "hexmap":
            continue  # skip this currently
        f.write(f"src = sp.check_output(\n")
        f.write(f"    [\"tool/{file["name"]}\", \"-i\", \"save/save.bin\"],\n")
        f.write(f"    stdout=sys.stdout,\n")
        f.write(f"    stderr=sys.stderr,\n")
        f.write(f")\n")
        f.write(f"with open(\"save/{file["lang"]}/save.{file["suffix"]}\", \"wb\") as f:\n")
        f.write(f"    f.write(src)\n")


output = "script/run_generated.bat"

with open(output, "w") as f:
    f.write("@echo off\n")
    f.write("setlocal\n")
    f.write("python script/run_generated.py\n")

output = "script/run_generated.sh"

with open(output, "wb") as f:
    f.write("#!/bin/bash\n".encode())
    f.write("python script/run_generated.py\n".encode())
output = "script/run_generated.ps1"

with open(output, "w") as f:
    f.write("$ErrorActionPreference = 'Stop'\n")
    f.write("python script/run_generated.py\n")
    