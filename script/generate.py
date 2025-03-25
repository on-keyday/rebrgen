import subprocess as sp
import sys

INPUT = "src/test/test_cases.bgn" if len(sys.argv) < 2 else sys.argv[1]
SRC2JSON = (
    "C:/workspace/shbrgen/brgen/tool/src2json" if len(sys.argv) < 3 else sys.argv[2]
)

LANG_LIST = ["c", "python", "haskell", "go"]

for lang in LANG_LIST:
    sp.run(
        ["python", "script/gen_template.py", lang],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

sp.run(
    ["python", "script/collect_cmake.py", INPUT, SRC2JSON],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)

sp.run(
    ["python", "script/generate_test_glue.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)

DOC = sp.check_output(["tool/gen_template", "--mode", "docs-markdown"])

with open("docs/template_parameters.md", "wb") as f:
    f.write(DOC)
