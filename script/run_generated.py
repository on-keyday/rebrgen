import os
import subprocess as sp
import sys

sp.run(
    ["python","script/build.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
save = sp.check_output(
    ["C:/workspace/shbrgen/brgen/tool/src2json", "src/test/test_cases.bgn"],
    stderr=sys.stderr,
)
with open("save/sample.json", "wb") as f:
    f.write(save)
print("Generated: save/sample.json")
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "-o", "save/save.bin", "-c", "save/save.dot"],
    stderr=sys.stderr,
)
print("Generated: save/save.bin")
print("Generated: save/save.dot")
with open("save/save.txt", "wb") as f:
    f.write(save)
print("Generated: save/save.txt")
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "--print-only-op"],
    stderr=sys.stderr,
)
with open("save/save_op.txt", "wb") as f:
    f.write(save)
print("Generated: save/save_op.txt")

src = sp.check_output(
    ["tool/bm2c", "-i", "save/save.bin","--test-info","save/c/save.c.json"],
    stderr=sys.stderr,
)
with open("save/c/save.c", "wb") as f:
    f.write(src)
print(f"Generated: save/c/save.c")
src = sp.check_output(
    ["tool/bm2cpp", "-i", "save/save.bin","--test-info","save/cpp/save.cpp.json"],
    stderr=sys.stderr,
)
with open("save/cpp/save.cpp", "wb") as f:
    f.write(src)
print(f"Generated: save/cpp/save.cpp")
src = sp.check_output(
    ["tool/bm2go", "-i", "save/save.bin","--test-info","save/go/save.go.json"],
    stderr=sys.stderr,
)
with open("save/go/save.go", "wb") as f:
    f.write(src)
print(f"Generated: save/go/save.go")
src = sp.check_output(
    ["tool/bm2haskell", "-i", "save/save.bin","--test-info","save/haskell/save.hs.json"],
    stderr=sys.stderr,
)
with open("save/haskell/save.hs", "wb") as f:
    f.write(src)
print(f"Generated: save/haskell/save.hs")
src = sp.check_output(
    ["tool/bm2python", "-i", "save/save.bin","--test-info","save/python/save.py.json"],
    stderr=sys.stderr,
)
with open("save/python/save.py", "wb") as f:
    f.write(src)
print(f"Generated: save/python/save.py")
src = sp.check_output(
    ["tool/bm2rust", "-i", "save/save.bin","--test-info","save/rust/save.rs.json"],
    stderr=sys.stderr,
)
with open("save/rust/save.rs", "wb") as f:
    f.write(src)
print(f"Generated: save/rust/save.rs")
sp.run(
    ["python","script/run_cmptest.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
