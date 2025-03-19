@echo off
setlocal

set BUILD_TYPE=Debug
set BUILD_MODE=native
set INSTALL_PREFIX=.
set FUTILS_DIR=C:/workspace/utils_backup
set EMSDK_PATH=C:\workspace\emsdk\emsdk_env.bat 

cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
cmake --build ./built/%BUILD_MODE%/%BUILD_TYPE% 
cmake --install ./built/%BUILD_MODE%/%BUILD_TYPE% --component Unspecified
cmake --install ./built/native/Debug --component gen_template
exit /b 0
set BUILD_MODE=web
call %EMSDK_PATH%
call emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX%/web -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
cmake --install ./built/%BUILD_MODE%/%BUILD_TYPE% --component Unspecified
