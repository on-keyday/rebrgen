#!/bin/bash
set -e

LANG_NAME=$1
FUTILS_DIR=${FUTILS_DIR:-C:/workspace/utils_backup}
cmake --build ./built/native/Debug --target gen_template
cmake --install ./built/native/Debug --component gen_template
echo "Generating template for $LANG_NAME"
./tool/gen_template --lang $LANG_NAME --header > src/bm2$LANG_NAME/bm2$LANG_NAME.hpp
./tool/gen_template --lang $LANG_NAME > src/bm2$LANG_NAME/bm2$LANG_NAME.cpp
./tool/gen_template --lang $LANG_NAME --main > src/bm2$LANG_NAME/main.cpp
./tool/gen_template --lang $LANG_NAME --cmake > src/bm2$LANG_NAME/CMakeLists.txt
python script/collect_cmake.py
