#!/bin/bash

BUILD_TYPE=${2:-Debug}
BUILD_MODE=${1:-native}
INSTALL_PREFIX=.
FUTILS_DIR=${FUTILS_DIR:-C:/workspace/utils_backup}
EMSDK_PATH=${EMSDK_PATH:-C:/workspace/emsdk/emsdk_env.sh}

export FUTILS_DIR
export BUILD_MODE

if [ $BUILD_MODE = "native" ];then
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
cmake --build ./built/$BUILD_MODE/$BUILD_TYPE
cmake --install ./built/$BUILD_MODE/$BUILD_TYPE  --component Unspecified
cmake --install ./built/native/Debug --component gen_template
elif [ $BUILD_MODE = "web" ]; then
source "$EMSDK_PATH"
emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX/web -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
cmake --build ./built/$BUILD_MODE/$BUILD_TYPE
cmake --install ./built/$BUILD_MODE/$BUILD_TYPE --component Unspecified
else
echo "Invalid build mode"
fi
