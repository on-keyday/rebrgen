#!/bin/bash
set -e
./script/gen_template.sh haskell
./script/gen_template.sh python
./script/gen_template.sh c
./script/gen_template.sh go
python script/collect_cmake.py
./tool/gen_template --template-docs=markdown > docs/template_parameters.md
