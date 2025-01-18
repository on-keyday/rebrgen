@echo off
setlocal
call build.bat
set INPUT=full_networks.bgn
C:\workspace\shbrgen\brgen\tool\src2json src/test/%INPUT% > save/sample.json
tool\bmgen -p -i save/sample.json -o save/save.bin -c save/save.dot > save/save.txt
tool\bm2cpp -i save/save.bin > save/save.hpp
tool\bm2rust -i save/save.bin > save/save/src/save.rs
tool\bm2rust -i save/save.bin --async > save/save/src/save_async.rs
cd save/save
cargo build  2> ../cargo_build.txt
cargo fmt
cargo fix --bin "save" --allow-no-vcs 2> ../cargo_fix.txt
cd ../..
dot ./save/save.dot -Tpng -O
