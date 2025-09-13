import subprocess
import sys
import os

# This script simplifies calling the ebmcodegen tool to generate templates.
# It can generate a single template or test generation for all available templates.


def get_tool_path():
    """Determine the ebmcodegen tool path based on the OS."""
    tool_path = "tool/ebmcodegen.exe" if sys.platform == "win32" else "tool/ebmcodegen"
    if not os.path.exists(tool_path):
        print(
            f"Error: '{tool_path}' not found. Please build the project first using 'python script/build.py native Debug'.",
            file=sys.stderr,
        )
        return None
    return tool_path


def run_single_template(tool_path, template_target):
    """Generate a single template and print it to stdout."""
    command = [tool_path, "--mode", "template", "--template-target", template_target]
    print(f"Running command: {' '.join(command)}", file=sys.stderr)
    try:
        subprocess.run(command, check=True, text=True, encoding="utf-8")
    except subprocess.CalledProcessError as e:
        print(
            f"Error: ebmcodegen failed for target '{template_target}' with exit code {e.returncode}",
            file=sys.stderr,
        )
        sys.exit(1)


def run_test_mode(tool_path):
    """Get all template targets and test generation for each one."""
    print(
        "Running in test mode. Fetching all available template targets...",
        file=sys.stderr,
    )

    # Get the list of hooks
    try:
        hooklist_cmd = [tool_path, "--mode", "hooklist"]
        result = subprocess.run(
            hooklist_cmd, check=True, capture_output=True, text=True, encoding="utf-8"
        )
        targets = [line for line in result.stdout.splitlines() if line.strip()]
        if not targets:
            print(
                "Error: Could not retrieve any template targets from 'hooklist'.",
                file=sys.stderr,
            )
            sys.exit(1)
        print(f"Found {len(targets)} targets to test.", file=sys.stderr)
    except subprocess.CalledProcessError as e:
        print(
            f"Error: Failed to get hooklist. ebmcodegen exited with code {e.returncode}",
            file=sys.stderr,
        )
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)

    # Test each target
    for i, target in enumerate(targets):
        print(f"[{i+1}/{len(targets)}] Testing target: {target}", file=sys.stderr)
        command = [tool_path, "--mode", "template", "--template-target", target]
        try:
            # We don't need to see the output, just check if it succeeds
            subprocess.run(
                command, check=True, capture_output=True, text=True, encoding="utf-8"
            )
        except subprocess.CalledProcessError as e:
            print(
                f"\n--- ERROR: Test failed for target: '{target}' ---", file=sys.stderr
            )
            print(f"ebmcodegen exited with code {e.returncode}", file=sys.stderr)
            print("\n--- STDOUT ---", file=sys.stderr)
            print(e.stdout, file=sys.stderr)
            print("\n--- STDERR ---", file=sys.stderr)
            print(e.stderr, file=sys.stderr)
            print("\n--- Test stopped. ---", file=sys.stderr)
            sys.exit(1)

    print("\nSuccess! All template targets generated successfully.", file=sys.stderr)


def main():
    # Ensure the script is run from the project root
    if not os.path.exists("tool"):
        print(
            "Error: This script must be run from the root of the rebrgen project.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Check for the required argument
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <template_target | test>", file=sys.stderr)
        print(
            "  <template_target>: The name of the template to generate (e.g., 'Statement_BLOCK').",
            file=sys.stderr,
        )
        print(
            "  test             : Test generation for all available templates.",
            file=sys.stderr,
        )
        print(
            "\nTo see available targets, run './tool/ebmcodegen --mode hooklist'",
            file=sys.stderr,
        )
        sys.exit(1)

    tool_path = get_tool_path()
    if not tool_path:
        sys.exit(1)

    target_arg = sys.argv[1]

    if target_arg == "test":
        run_test_mode(tool_path)
    else:
        run_single_template(tool_path, target_arg)


if __name__ == "__main__":
    main()
