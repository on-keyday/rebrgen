import subprocess
import os

# This script is a Python port of ebm.ps1.
# It generates C++ header and source files for the Extended Binary Module (EBM)
# from a .bgn definition file.

# Absolute path to the brgen tool directory, as specified in the original script.
# This makes the script dependent on a specific directory structure.
TOOL_PATH = "C:\\workspace\\shbrgen\\brgen\\tool"

# Paths to the executables.
# We assume .exe extension for Windows, which is where the original .ps1 ran.
SRC2JSON_EXE = os.path.join(TOOL_PATH, "src2json.exe")
JSON2CPP2_EXE = os.path.join(TOOL_PATH, "json2cpp2.exe")

# File paths (relative to the project root, where this script should be run from)
BGN_FILE = "src/ebm/extended_binary_module.bgn"
EBM_JSON_FILE = "save/ebm.json"
HPP_FILE = "src/ebm/extended_binary_module.hpp"
CPP_FILE = "src/ebm/extended_binary_module.cpp"

def run_command(command, output_file):
    """Runs a command and redirects its stdout to a file."""
    print(f"Running: {' '.join(command)} > {output_file}")
    try:
        with open(output_file, "w", encoding="utf-8") as f:
            result = subprocess.run(command, stdout=f, check=True, text=True, encoding="utf-8")
        if result.returncode != 0:
            print(f"Error running command. Return code: {result.returncode}")
            if result.stderr:
                print(f"Stderr: {result.stderr}")
            return False
    except FileNotFoundError:
        print(f"Error: Command not found at {command[0]}")
        print("Please ensure the TOOL_PATH is correct and the brgen tools are built.")
        return False
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while running {' '.join(command)}.")
        print(f"Return code: {e.returncode}")
        if e.stdout:
            print(f"Stdout: {e.stdout}")
        if e.stderr:
            print(f"Stderr: {e.stderr}")
        return False
    return True

def main():
    """Main function to execute the code generation steps."""
    print("Starting EBM C++ file generation...")

    # Step 1: Convert .bgn to brgen AST JSON
    # & $TOOL_PATH\src2json src/ebm/extended_binary_module.bgn | Out-File save/ebm.json -Encoding utf8
    cmd1 = [SRC2JSON_EXE, BGN_FILE]
    if not run_command(cmd1, EBM_JSON_FILE):
        return

    # Step 2: Generate C++ header file from JSON
    # & $TOOL_PATH\json2cpp2 -f save/ebm.json --mode header_file --add-visit --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.hpp -Encoding utf8
    cmd2 = [
        JSON2CPP2_EXE,
        "-f", EBM_JSON_FILE,
        "--mode", "header_file",
        "--add-visit",
        "--enum-stringer",
        "--use-error",
        "--dll-export"
    ]
    if not run_command(cmd2, HPP_FILE):
        return

    # Step 3: Generate C++ source file from JSON
    # & $TOOL_PATH\json2cpp2 -f save/ebm.json --mode source_file --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.cpp -Encoding utf8
    cmd3 = [
        JSON2CPP2_EXE,
        "-f", EBM_JSON_FILE,
        "--mode", "source_file",
        "--enum-stringer",
        "--use-error",
        "--dll-export"
    ]
    if not run_command(cmd3, CPP_FILE):
        return

    print("\nSuccessfully generated C++ files:")
    print(f"  - {HPP_FILE}")
    print(f"  - {CPP_FILE}")

if __name__ == "__main__":
    # Change directory to project root to handle relative paths correctly.
    # This script is in src/ebm, so the root is two levels up.
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    os.chdir(project_root)
    print(f"Working directory set to: {os.getcwd()}")
    main()
