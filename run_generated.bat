@echo off
setlocal
call build.bat
tool\bm2c -i save/save.bin > save/save.c
tool\bm2haskell -i save/save.bin > save/save.hs
tool\bm2py -i save/save.bin > save/save.py
