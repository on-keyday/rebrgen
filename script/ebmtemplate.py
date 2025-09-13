import subprocess
import sys
import os

# This script simplifies calling the ebmcodegen tool to generate templates.
# It takes a template target name as an argument and executes:
# ./tool/ebmcodegen --mode template --template-target <template_target>

def main():
    # Ensure the script is run from the project root
    if not os.path.exists("tool") or (not os.path.exists("tool/ebmcodegen") and not os.path.exists("tool/ebmcodegen.exe")):
        print("Error: This script must be run from the root of the rebrgen project.", file=sys.stderr)
        sys.exit(1)

    # Check for the required argument
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <template_target>", file=sys.stderr)
        print("  <template_target>: The name of the template to generate (e.g., 'Statement_BLOCK').", file=sys.stderr)
        print("                     Run './tool/ebmcodegen --mode hooklist' to see available targets.", file=sys.stderr)
        sys.exit(1)

    template_target = sys.argv[1]

    # Determine the tool path based on the operating system
    tool_path = "tool/ebmcodegen.exe" if sys.platform == "win32" else "tool/ebmcodegen"

    # Construct the command
    command = [tool_path, "--mode", "template", "--template-target", template_target]

    print(f"Running command: {' '.join(command)}", file=sys.stderr)

    # Execute the command, streaming the output directly to stdout
    try:
        process = subprocess.run(command, check=True, text=True, encoding='utf-8')
    except FileNotFoundError:
        print(f"Error: '{tool_path}' not found. Please build the project first using 'python script/build.py native Debug'.", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error: ebmcodegen failed with exit code {e.returncode}", file=sys.stderr)
        # The tool's stderr is already piped, so no need to print e.stderr
        sys.exit(1)

if __name__ == "__main__":
    main()
