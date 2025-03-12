@echo off
setlocal

set LANG_NAME=%1
if "%LANG_NAME%" == "" (
    echo Usage: %0 LANG_NAME
    exit /b 1
)
set FUTILS_DIR=C:/workspace/utils_backup

cmake --build ./built/native/Debug --target gen_template
cmake --install ./built/native/Debug --component gen_template
echo Generating template for %LANG_NAME%
set DEBUG_OPTION=
.\tool\gen_template --config --config-file src\bm2%LANG_NAME%\config.json --hook-dir src\bm2%LANG_NAME%\hook  %DEBUG_OPTION% > src\bm2%LANG_NAME%\config.json
.\tool\gen_template --lang %LANG_NAME% --header --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\bm2%LANG_NAME%.hpp
.\tool\gen_template --config-file src\bm2%LANG_NAME%\config.json --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\bm2%LANG_NAME%.cpp
.\tool\gen_template --lang %LANG_NAME% --main --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION%> src\bm2%LANG_NAME%\main.cpp
.\tool\gen_template --lang %LANG_NAME% --cmake --hook-dir src\bm2%LANG_NAME%\hook %DEBUG_OPTION% > src\bm2%LANG_NAME%\CMakeLists.txt
