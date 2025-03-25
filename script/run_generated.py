import os
import subprocess as sp
import sys

sp.run(
    ["script/build"],
    check=True,
    shell=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "-o", "save/save.bin", "-c", "save/save.dot"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/save.txt", "wb") as f:
    f.write(save)
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "--print-only-op"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/save_op.txt", "wb") as f:
    f.write(save)

src = sp.check_output(
    ["tool/bm2c", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/c/save.c", "wb") as f:
    f.write(src)
src = sp.check_output(
    ["tool/bm2cpp", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/cpp/save.cpp", "wb") as f:
    f.write(src)
src = sp.check_output(
    ["tool/bm2go", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/go/save.go", "wb") as f:
    f.write(src)
src = sp.check_output(
    ["tool/bm2haskell", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/haskell/save.hs", "wb") as f:
    f.write(src)
src = sp.check_output(
    ["tool/bm2python", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/python/save.py", "wb") as f:
    f.write(src)
src = sp.check_output(
    ["tool/bm2rust", "-i", "save/save.bin"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
with open("save/rust/save.rs", "wb") as f:
    f.write(src)
