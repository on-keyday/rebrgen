# RE:brgen

RE:brgen (rebrgen) is a generator construction project for brgen(https://github.com/on-keyday/brgen)

while original brgen code generator is designed as AST-to-Code model,
rebrgen is as AST-to-VMCode-to-Code model

This is aimed at improving flexibility and inter-language compatibility

# How to add language

To add a new language, follow these steps:

1.  Edit `LANG_LIST` in `script/generate.py` to include the new language.
2.  Run `script/generate` to initialize code template
3.  Edit `src/bm2/gen_template.cpp` for language-independent logic.
4.  Create a directory `src/bm2{lang_name}/` for the new language.
5.  Modify `src/bm2{lang_name}/config.json` for language-specific placeholders. Base templates are generated by `tool\\gen_template --mode config`.
6.  Modify or add files in `src/bm2{lang_name}/hook/` for language-specific custom logic. Basic hook definitions are in `src/bm2/hook_list.bgn` and can be obtained by executing `tool\\gen_template --print-hooks`.
7.  Run `script/run_generated` to run the generated code generators and save the result in `save/save.{language_specific_suffix}`.

these cycle are called `Development Cycle`.
using `script/run_cycle`, you can run `script/generate && script/run_generated && script/run_cmptest`

# Current Status

- Rust version is working (it potentially contains bug)
- C++ version is basically working but some bugs are remaining yet

- Other languages are aimed to support in the future...

# LICENSE

MIT License
