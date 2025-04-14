# C++ Generator Migration G-PDCA Cycle (cpp -> cpp3)

## Goal

Replace the handwritten C++ generator (`src/bm2cpp/`) with a new, generated C++ generator (`src/bm2cpp3/`) that leverages the generator-generator (`src/bm2/gen_template/`) for continuous improvement.

## Plan

- **Setup:**
  - `cpp3` has been added to `LANG_LIST` and initial configuration updates (e.g. `src/bm2cpp3/config.json` adjustments) are complete.
- **Analysis:**
  - Performed diff analysis between the handwritten generator (`src/bm2cpp/`) and the generated one (`src/bm2cpp3/`).
  - Identified missing custom logic components:  
    _ Advanced handling of union types and member access (e.g. formatting union variants with `std::variant`).  
    _ Detailed bit field encoding/decoding operations.  
    \* Customized property getter and setter logic.
- **Action Items:**
  - Define iterative improvements via hooks in `src/bm2cpp3/hook/` (using `sections.txt` or new hook files) addressing the identified gaps.
  - Ensure test configurations in `testkit/inputs.json` and build scripts (`cmptest_build.txt` and `cmptest_run.txt`) are in place.

## Do

- Implement missing custom logic using appropriate hooks.
- Regenerate the generator via `script/generate.py` and build/run it with `script/run_generated.py`.

## Check

- Run `script/run_cmptest.py` to verify that tests pass.
- Analyze test outcomes and capture any feedback for further changes.

## Act (Adjust)

- Refine hook implementations based on test results.
- Iterate the cycle until all tests consistently pass.
- Once stable, plan cleanup actions such as removing the old `src/bm2cpp/` directory and updating remaining references.

**Next Step:** run `script/run_cmptest.py` to validate the improvements.
