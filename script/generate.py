import subprocess as sp
import sys

LANG_LIST = ["c", "python", "haskell", "go"]

for lang in LANG_LIST:
    sp.run(
        ["python", "script/gen_template.py", lang],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

sp.run(
    ["python", "script/collect_cmake.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)

DOC = sp.check_output(["tool/gen_template", "--mode", "docs-markdown"])

with open("docs/template_parameters.md", "wb") as f:
    f.write(DOC)
