#!/bin/bash
set -e

LANG_NAME=$1
if [ -z "$LANG_NAME" ]; then
    echo "Usage: $0 <lang_name>"
    exit 1
fi
export FUTILS_DIR=${FUTILS_DIR:-C:/workspace/utils_backup}
cmake --build ./built/native/Debug --target gen_template
cmake --install ./built/native/Debug --component gen_template
echo "Generating template for $LANG_NAME"
DEBUG_OPTION=
./tool/gen_template --lang $LANG_NAME --header --hook-dir src/bm2$LANG_NAME/hook "$DEBUG_OPTION" > src/bm2$LANG_NAME/bm2$LANG_NAME.hpp
./tool/gen_template --lang $LANG_NAME --hook-dir src/bm2$LANG_NAME/hook "$DEBUG_OPTION" > src/bm2$LANG_NAME/bm2$LANG_NAME.cpp
./tool/gen_template --lang $LANG_NAME --main --hook-dir src/bm2$LANG_NAME/hook "$DEBUG_OPTION" > src/bm2$LANG_NAME/main.cpp
./tool/gen_template --lang $LANG_NAME --cmake --hook-dir src/bm2$LANG_NAME/hook "$DEBUG_OPTION" > src/bm2$LANG_NAME/CMakeLists.txt
python script/collect_cmake.py
