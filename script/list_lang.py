import os
import pathlib as pl
import json


def collect_langs(root_dir):
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        for d in dirs:
            if d != "bm2" and str(d).startswith("bm2"):
                if d == "bm2hexmap":
                    continue
                cmake_files.append(d[3:])
    return cmake_files


print("\n".join(collect_langs("src")))
