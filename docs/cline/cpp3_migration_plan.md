# C++ Generator Migration Plan (cpp -> cpp3)

**Goal:** Replace the handwritten C++ generator (`src/bm2cpp/`) with a new generator (`src/bm2cpp3/`) created using the generator-generator (`src/bm2/gen_template/`).

**Steps:**

1.  **Add `cpp3` Language:**
    - [x] Added `cpp3` to `LANG_LIST` in `script/generate.py`.
    - [x] Fixed bug in `script/gen_template.py` related to missing `env` argument.
    - [x] Ran `script/generate.py` to create initial `src/bm2cpp3/` files.
2.  **Configure `cpp3`:**
    - [x] Read `src/bm2cpp/config.json`.
    - [x] Updated `src/bm2cpp3/config.json` with `suffix` and `worker_request_name`.
3.  **Transfer Custom Logic via Hooks:**
    - [x] Check existing hooks in `src/bm2cpp/hook/`. (Found `cmptest_build.txt`)
    - [x] Copy relevant hooks from `src/bm2cpp/hook/` to `src/bm2cpp3/hook/`. (Skipped, logic added to `sections.txt`)
    - [ ] Analyze differences between handwritten `src/bm2cpp/bm2cpp.cpp`/`.hpp` and generated `src/bm2cpp3/bm2cpp3.cpp`/`.hpp` (if necessary, after copying hooks).
    - [ ] Identify required custom logic not covered by copied hooks.
    - [ ] Implement missing logic using appropriate hooks in `src/bm2cpp3/hook/` (potentially using `sections.txt`). Refer to `tool/gen_template --print-hooks` and `docs/template_parameters.md`.
4.  **Testing:**
    - [x] Run `script/generate.py` to regenerate `bm2cpp3` with new hooks.
    - [x] Run `script/run_generated.py` to build and run the `bm2cpp3` generator.
    - [x] Add/update C++ specific test configurations in `testkit/inputs.json` if needed.
    - [x] Implement `cmptest_build.txt` and `cmptest_run.txt` in `src/bm2cpp3/hook/` based on `src/bm2cpp/hook/` or C++ build/run requirements. (`cmptest_build.txt` added to `sections.txt`)
    - [ ] Run `script/run_cmptest.py` and ensure tests pass.
5.  **Cleanup (Optional/Future):**
    - [ ] Remove the old `src/bm2cpp/` directory.
    - [ ] Update references from `cpp` or `cpp2` to `cpp3` elsewhere if necessary.

**Current Step:** Run `script/run_cmptest.py` and ensure tests pass.
