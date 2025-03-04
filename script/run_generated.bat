@echo off
setlocal
call script\build.bat
tool\bmgen -p -i save/sample.json -o save/save.bin -c save/save.dot > save/save.txt
tool\bmgen -p -i save/sample.json --print-only-op > save/save_op.txt
tool\bm2c -i save/save.bin > save/save.c
tool\bm2cpp -i save/save.bin > save/save.cpp
tool\bm2go -i save/save.bin > save/save.go
tool\bm2haskell -i save/save.bin > save/save.hs
tool\bm2py -i save/save.bin > save/save.py
tool\bm2rust -i save/save.bin > save/save.rs
