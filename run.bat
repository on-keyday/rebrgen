@echo off
setlocal
C:\workspace\shbrgen\brgen\tool\src2json src/test/test_cases.bgn > save/sample.json
tool\bmgen -i save/sample.json -o save/save.bin -c save/save.dot > save/save.txt
tool\bm2cpp -i save/save.bin > save/save.hpp
tool\bm2rust -i save/save.bin > save/save.rs
