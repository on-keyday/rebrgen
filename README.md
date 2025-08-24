# RE:brgen

RE:brgen (rebrgen) is a generator construction project for brgen(https://github.com/on-keyday/brgen)

while original brgen code generator is designed as AST-to-Code model,
rebrgen is as AST-to-VMCode-to-Code model

This is aimed at improving flexibility and inter-language compatibility

# How to add language

1. run `python script/build.py` to build `./tool/ebmcodegen[.exe]`
2. run `python script/ebmcodegen.py <lang name>`
3. run `python script/build.py` again
4. write `<any>.bgn` file (see https://github.com/on-keyday/brgen) and run the `src2json -i <any>.bgn > <saved>.json`
   - you can use generating `.json` file at [WebPlayground](https://on-keyday.github.io/brgen#mode=json+ast)
5. run `./tool/ebmgen -i <saved>.json -o <output>.ebm [-d <debug_output.txt>]`
6. run `./tool/ebm2<lang name> -i <output>.ebm` then you can see the generated code
7. to edit `ebm2<lang name>` code, write template code in `src/ebmcg/ebm2<lang name>/visitor`
   - you can see examples in `src/ebmcodegen/default_codegen_visitor/`

# Current Status

bmgen (old project)

- Rust version is working (it potentially contains bug)
- C++ version is basically working but some bugs are remaining yet

ebmgen/ebmcodegen (currently active project)

# LICENSE

MIT License
