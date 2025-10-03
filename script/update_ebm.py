# run src/ebm/ebm.py to update the C++ files
import subprocess as sp
import sys

sp.check_call(["python", "src/ebm/ebm.py"], stdout=sys.stdout, stderr=sys.stderr)

# then run script/build.py to build the tools
sp.check_call(["python", "script/build.py"], stdout=sys.stdout, stderr=sys.stderr)

# then run tool/ebmcodegen --mode subset > src/ebmcodegen/body_subset.cpp
subset = sp.check_output(["tool/ebmcodegen", "--mode", "subset"], stderr=sys.stderr)

with open("src/ebmcodegen/body_subset.cpp", "w") as f:
    f.write(subset.decode("utf-8"))
print("Updated: src/ebmcodegen/body_subset.cpp")
