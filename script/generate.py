import subprocess as sp

LANG_LIST = ["c", "python", "haskell", "go"]

for lang in LANG_LIST:
    sp.check_call(["python", "script/gen_template.py", lang])

sp.check_call(["python", "script/collect_cmake.py"])

DOC = sp.check_output(["tool/gen_template", "--mode", "docs-markdown"])

with open("docs/template_parameters.md", "wb") as f:
    f.write(DOC)
