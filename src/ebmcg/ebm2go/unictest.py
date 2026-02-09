#!/usr/bin/env python3
# Test logic for ebm2go
import sys
import os
import subprocess
import pathlib
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .go file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The struct name (CamelCase)

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pathlib.Path("go_proj")
    os.makedirs(proj_dir, exist_ok=True)

    # Copy generated Go code into the project directory
    with open(TEST_TARGET_FILE, "r") as f_src:
        with open(proj_dir / "generated.go", "w") as f_dst:
            f_dst.write(f_src.read())

    # Create go.mod
    with open(proj_dir / "go.mod", "w") as f:
        f.write("module test_runner\n\n")
        f.write("go 1.21\n")

    # Create main.go test harness
    #
    # The generated Go code uses *[]byte for both decoder and encoder IO:
    #   func (s *Struct) Decode(input *[]byte) error
    #   func (s *Struct) Encode(output *[]byte) error
    #
    # Decode: the input slice is consumed (resliced) as data is read.
    # Encode: the output slice is consumed (resliced) as data is written
    #         via copy(). Written bytes = original capacity - len(remaining).
    with open(proj_dir / "main.go", "w") as f:
        f.write(
            f"""package main

import (
	"fmt"
	"os"
)

func main() {{
	if len(os.Args) < 3 {{
		fmt.Fprintf(os.Stderr, "Usage: %s <input_file> <output_file>\\n", os.Args[0])
		os.Exit(1)
	}}

	inputPath := os.Args[1]
	outputPath := os.Args[2]

	// Read input file
	inputData, err := os.ReadFile(inputPath)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Failed to read input file '%s': %v\\n", inputPath, err)
		os.Exit(1)
	}}

	// Decode
	var target {TEST_TARGET_FORMAT}
	inputSlice := make([]byte, len(inputData))
	copy(inputSlice, inputData)
	err = target.Decode(&inputSlice)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Decode error: %v\\n", err)
		os.Exit(10)
	}}

	// Encode
	outputCapacity := len(inputData) * 2
	if outputCapacity < 1024 {{
		outputCapacity = 1024
	}}
	outputBuf := make([]byte, outputCapacity)
	outputSlice := outputBuf
	err = target.Encode(&outputSlice)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Encode error: %v\\n", err)
		os.Exit(20)
	}}

	// Written bytes = original capacity - remaining slice length
	writtenBytes := outputCapacity - len(outputSlice)

	// Write output file
	err = os.WriteFile(outputPath, outputBuf[:writtenBytes], 0644)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Failed to write output file '%s': %v\\n", outputPath, err)
		os.Exit(1)
	}}

	os.Exit(0)
}}
"""
        )

    # Build the Go project
    print("\nBuilding Go project...")
    result = subprocess.run(
        [
            "go",
            "build",
            "-o",
            "test_runner.exe" if os.name == "nt" else "test_runner",
            ".",
        ],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Go compilation failed!")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)

    print("Compilation successful.")

    # Run the compiled test executable
    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    executable_path = (proj_dir / executable_name).resolve()

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2go unictest ({TEST_TARGET_FORMAT})",
                "program": executable_path.as_posix(),
                "args": [
                    INPUT_FILE,
                    OUTPUT_FILE,
                ],
                "stopAtEntry": True,
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path.as_posix()}")
    proc = subprocess.run(
        [
            executable_path.as_posix(),
            INPUT_FILE,
            OUTPUT_FILE,
        ],
        capture_output=True,
        text=True,
    )

    if proc.stdout:
        print("--- stdout ---")
        print(proc.stdout)
    if proc.stderr:
        print("--- stderr ---")
        print(proc.stderr)

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
