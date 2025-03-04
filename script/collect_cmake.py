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
                obj["has_config"] = pl.Path(os.path.join(root, "config.json")).exists()
                if obj["has_config"]:
                    with open(os.path.join(root, "config.json"), "r") as f:
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

output = "script/run_generated.bat"

with open(output, "w") as f:
    f.write("@echo off\n")
    f.write("setlocal\n")
    f.write("call script\\build.bat\n")
    f.write("tool\\bmgen -p -i save/sample.json -o save/save.bin -c save/save.dot > save/save.txt\n")
    f.write("tool\\bmgen -p -i save/sample.json --print-only-op > save/save_op.txt\n")
    for file in src:
        if file["lang"] == "hexmap":
            continue  # skip this currently
        f.write(f"tool\\{file["name"]} -i save/save.bin > save/save.{file["suffix"]}\n")

output = "script/run_generated.sh"

with open(output, "wb") as f:
    f.write("#!/bin/bash\n".encode())
    f.write("./script/build.sh\n".encode())
    f.write(
        "tool/bmgen -p -i save/sample.json -o save/save.bin -c save/save.dot > save/save.txt\n".encode()
    )
    f.write("tool/bmgen -p -i save/sample.json --print-only-op > save/save_op.txt\n".encode())
    for file in src:
        if file["lang"] == "hexmap":
            continue  # skip this currently
        f.write(
            f"tool/{file["name"]} -i save/save.bin > save/save.{file["suffix"]}\n".encode()
        )

output = "script/run_generated.ps1"

with open(output, "w") as f:
    f.write("$ErrorActionPreference = 'Stop'\n")
    f.write(".\\script\\build.ps1\n")
    f.write(
        ".\\tool\\bmgen -p -i save\\sample.json -o save\\save.bin -c save\\save.dot > save\\save.txt\n"
    )
    f.write(".\\tool\\bmgen -p -i save\\sample.json --print-only-op > save\\save_op.txt\n")
    for file in src:
        if file["lang"] == "hexmap":
            continue  # skip this currently
        f.write(
            f".\\tool\\{file["name"]} -i save\\save.bin > save\\save.{file["suffix"]}\n"
        )