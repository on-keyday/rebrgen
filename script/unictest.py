import os
import sys
import pathlib as pl

mode = sys.argv[1]
target_command = sys.argv[2]
file_ext = sys.argv[3]

env = os.environ.copy()

unictest_env_vars = {k: v for k, v in env.items() if k.startswith("UNICTEST_")}

for e in list(unictest_env_vars.keys()):
    print(f"{e}={unictest_env_vars[e]}")

runner_dir = unictest_env_vars["UNICTEST_RUNNER_DIR"]
original_workdir = unictest_env_vars["UNICTEST_ORIGINAL_WORKDIR"]
if mode == "setup":
    import subprocess as sp

    ebmgen = (pl.Path(original_workdir) / "tool/ebmgen").as_posix()
    if os.name == "nt":
        ebmgen += ".exe"
    input_file = (pl.Path(runner_dir) / "runner_input.ebm").as_posix()

    cmd = [
        ebmgen,
        "-i",
        unictest_env_vars["UNICTEST_SOURCE_FILE"],
        "-o",
        input_file,
    ]
    print(f"Running command: {' '.join(cmd)}")
    sp.check_call(cmd)

    ebm2target = (pl.Path(original_workdir) / f"tool/{target_command}").as_posix()
    if os.name == "nt":
        ebm2target += ".exe"

    cmd = [
        ebm2target,
        "-i",
        input_file,
    ]
    print(f"Running command: {' '.join(cmd)}")
    output = sp.check_output(cmd)

    output_file = pl.Path(runner_dir) / f"test_target.{file_ext}"
    with open(output_file, "wb") as f:
        f.write(output)
