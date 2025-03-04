#!/bin/bash
set -e
./script/gen_template.sh haskell
./script/gen_template.sh py
./script/gen_template.sh c