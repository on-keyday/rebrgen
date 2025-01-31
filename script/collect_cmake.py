import os
import pathlib as pl


def collect_cmake_files(root_dir):
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "CMakeLists.txt" and root != root_dir:
                cmake_files.append(os.path.join(root))
    return cmake_files


src = collect_cmake_files("src")

output = "src/CMakeLists.txt"

with open(output, "w") as f:
    for file in src:
        f.write(f'add_subdirectory("{pl.Path(file).relative_to("src").as_posix()}")\n')
