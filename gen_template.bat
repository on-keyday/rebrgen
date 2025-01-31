@echo off
setlocal

set LANG_NAME=%1

.\tool\gen_template --lang %LANG_NAME% --header > src\bm2%LANG_NAME%\bm2%LANG_NAME%.hpp
.\tool\gen_template --lang %LANG_NAME% > src\bm2%LANG_NAME%\bm2%LANG_NAME%.cpp
.\tool\gen_template --lang %LANG_NAME% --main > src\bm2%LANG_NAME%\main.cpp
.\tool\gen_template --lang %LANG_NAME% --cmake > src\bm2%LANG_NAME%\CMakeLists.txt
python script/collect_cmake.py
