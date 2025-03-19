$ErrorActionPreference = 'Stop'
.\script\build.ps1
.\tool\bmgen -p -i save\sample.json -o save\save.bin -c save\save.dot > save\save.txt
.\tool\bmgen -p -i save\sample.json --print-only-op > save\save_op.txt
mkdir -Force save\c
.\tool\bm2c -i save\save.bin > save\c\save.
mkdir -Force save\cpp
.\tool\bm2cpp -i save\save.bin > save\cpp\save.cpp
mkdir -Force save\go
.\tool\bm2go -i save\save.bin > save\go\save.go
mkdir -Force save\haskell
.\tool\bm2haskell -i save\save.bin > save\haskell\save.hs
mkdir -Force save\python
.\tool\bm2python -i save\save.bin > save\python\save.py
mkdir -Force save\rust
.\tool\bm2rust -i save\save.bin > save\rust\save.rs
