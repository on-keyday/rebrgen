import os
import sys
import pathlib as pl
import subprocess as sp
import binascii
import difflib

mode = sys.argv[1]
target_command = sys.argv[2]
file_ext = sys.argv[3]

env = os.environ.copy()

unictest_env_vars = {k: v for k, v in env.items() if k.startswith("UNICTEST_")}

for e in list(unictest_env_vars.keys()):
    print(f"{e}={unictest_env_vars[e]}")

runner_dir = unictest_env_vars["UNICTEST_RUNNER_DIR"]
original_workdir = unictest_env_vars["UNICTEST_ORIGINAL_WORK_DIR"]

setup_target_file = pl.Path(runner_dir) / f"test_target.{file_ext}"

if mode == "setup":

    ebmgen = (pl.Path(original_workdir) / "tool/ebmgen").as_posix()
    if os.name == "nt":
        ebmgen += ".exe"
    ebm_input_file = (pl.Path(runner_dir) / "runner_input.ebm").as_posix()

    cmd = [
        ebmgen,
        "-i",
        unictest_env_vars["UNICTEST_SOURCE_FILE"],
        "-o",
        ebm_input_file,
    ]
    print(f"\nRunning command: {' '.join(cmd)}")
    sp.check_call(cmd)

    ebm2target = (pl.Path(original_workdir) / f"tool/{target_command}").as_posix()
    if os.name == "nt":
        ebm2target += ".exe"

    cmd = [
        ebm2target,
        "-i",
        ebm_input_file,
    ]
    print(f"\nRunning command: {' '.join(cmd)}")
    output = sp.check_output(cmd)

    with open(setup_target_file, "wb") as f:
        f.write(output)
elif mode == "test":
    task_dir = unictest_env_vars["UNICTEST_WORK_DIR"]
    original_input_file = unictest_env_vars["UNICTEST_BINARY_FILE"]
    test_format_name = unictest_env_vars["UNICTEST_INPUT_FORMAT"]
    with open(original_input_file, "rb") as f:
        input_data = f.read()
    input_file = pl.Path(task_dir) / "input.bin"
    with open(input_file, "wb") as f:
        f.write(input_data)
    output_file = pl.Path(task_dir) / "output.bin"
    test_script_file = (
        pl.Path(original_workdir) / f"src/ebmcg/{target_command}/unictest.py"
    )
    print(f"\nRunning test script: {test_script_file.as_posix()}", flush=True)
    sp.check_call(
        [
            sys.executable,
            test_script_file.as_posix(),
            setup_target_file.as_posix(),
            input_file.as_posix(),
            output_file.as_posix(),
            test_format_name,
        ],
        env=unictest_env_vars,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    if not output_file.exists():
        print(f"Output file not created: {output_file.as_posix()}")
        sys.exit(1)
    with open(output_file, "rb") as f:
        output_data = f.read()
    # do compare and binary diff if not match
    if output_data != input_data:
        print("Output data does not match input data!")
        diff = difflib.unified_diff(
            binascii.hexlify(input_data).decode("utf-8").splitlines(keepends=True),
            binascii.hexlify(output_data).decode("utf-8").splitlines(keepends=True),
            fromfile="input_data",
            tofile="output_data",
        )
        for line in diff:
            print(line, end="")
        sys.exit(1)
    else:
        print("Output data matches input data.")
else:
    print(f"Unknown mode: {mode}")
    sys.exit(1)
