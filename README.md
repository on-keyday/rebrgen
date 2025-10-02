# RE:brgen

RE:brgen (rebrgen) is a generator construction project for brgen(https://github.com/on-keyday/brgen)

while original brgen code generator is designed as AST-to-Code model,
rebrgen is as AST-to-IR-to-Code model

This is aimed at improving flexibility and inter-language compatibility

# How to add language

1. you may copy `build_config.template.json` to `build_config.json`
2. run `python script/build.py` to build `./tool/ebmcodegen[.exe]`
   - at first build, also run `./brgen/script/clone_utils.[bat|sh]` to fetch and build dependency
3. run `python script/ebmcodegen.py <lang name>`
4. run `python script/build.py` again
5. write `<any>.bgn` file (see https://github.com/on-keyday/brgen) and run the `src2json <any>.bgn > <saved>.json`
   - you can use [WebPlayground](https://on-keyday.github.io/brgen/#code=Zm9ybWF0IERhdGE6DQogICAgbGVuIDp1OA0KICAgIGRhdGEgOltsZW5ddTgNCg&lang=json+ast) to generate `.json` file
6. run `./tool/ebmgen -i <saved>.json -o <output>.ebm`
7. run `./tool/ebm2<lang name> -i <output>.ebm` then you can see the generated code
8. to edit `ebm2<lang name>` code, run `python script/ebmtemplate.py interactive` for more info.

# Current Status

bmgen (old project)

- Rust version is working (it potentially contains bug)
- C++ version is basically working but some bugs are remaining yet

ebmgen/ebmcodegen (currently active project)

# LICENSE

MIT License
