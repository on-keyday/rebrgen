import subprocess
import sys
import os
import re

# This script simplifies calling the ebmcodegen tool to generate templates.
# It can generate a single template, test generation for all available templates,
# or update existing hook files with the latest templates.

START_MARKER = "/*DO NOT EDIT BELOW SECTION MANUALLY*/\n"
END_MARKER = "/*DO NOT EDIT ABOVE SECTION MANUALLY*/\n"


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


def run_save_template(tool_path, template_target, lang):
    """Generate a template and save it to the specified language's visitor directory."""
    if lang == "default_codegen":
        visitor_dir = "src/ebmcodegen/default_codegen_visitor"
    else:
        lang_dir = f"src/ebmcg/ebm2{lang}"
        if not os.path.isdir(lang_dir):
            print(f"Error: Directory '{lang_dir}' does not exist.", file=sys.stderr)
            sys.exit(1)
        visitor_dir = os.path.join(lang_dir, "visitor")
    os.makedirs(visitor_dir, exist_ok=True)  # Ensure visitor directory exists

    output_path = os.path.join(visitor_dir, f"{template_target}.hpp")
    if os.path.exists(output_path):
        print(f"Error: File '{output_path}' already exists.", file=sys.stderr)
        sys.exit(1)

    command = [tool_path, "--mode", "template", "--template-target", template_target]
    print(f"Running command: {' '.join(command)}", file=sys.stderr)
    try:
        result = subprocess.run(
            command, check=True, capture_output=True, text=True, encoding="utf-8"
        )
        # Add markers to the template content
        template_content = (
            f"{START_MARKER}{result.stdout}{END_MARKER}\n/*here to write the hook*/\n"
        )
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(template_content)
        print(f"Success! Template '{template_target}' saved to '{output_path}'")

    except subprocess.CalledProcessError as e:
        print(
            f"Error: ebmcodegen failed for target '{template_target}' with exit code {e.returncode}",
            file=sys.stderr,
        )
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)


def run_update_hooks(tool_path, lang):
    """Update all existing hook files in a language directory."""
    if lang == "default_codegen":
        visitor_dir = "src/ebmcodegen/default_codegen_visitor"
    else:
        visitor_dir = os.path.join("src", "ebmcg", f"ebm2{lang}", "visitor")
    if not os.path.isdir(visitor_dir):
        print(f"Error: Visitor directory '{visitor_dir}' not found.", file=sys.stderr)
        sys.exit(1)

    print(f"Checking for updates in '{visitor_dir}'...")
    hpp_files = [f for f in os.listdir(visitor_dir) if f.endswith(".hpp")]

    if not hpp_files:
        print("No .hpp files found to update.", file=sys.stderr)
        return

    for filename in hpp_files:
        template_target = os.path.splitext(filename)[0]
        file_path = os.path.join(visitor_dir, filename)
        print(f"-- Processing '{file_path}'...", file=sys.stderr)

        try:
            # Get the canonical template content
            command = [
                tool_path,
                "--mode",
                "template",
                "--template-target",
                template_target,
            ]
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, encoding="utf-8"
            )
            new_body = result.stdout
            new_block = f"{START_MARKER}{new_body}{END_MARKER}"

            with open(file_path, "r+", encoding="utf-8") as f:
                existing_content = f.read()
                # Use regex to find the existing block
                pattern = re.compile(
                    f"^{re.escape(START_MARKER)}(.*?)^{re.escape(END_MARKER)}",
                    re.DOTALL | re.MULTILINE,
                )
                match = pattern.search(existing_content)

                if not match:
                    # Case 1: Marker not found, add to the top
                    print(
                        "   -> No template block found. Prepending...", file=sys.stderr
                    )
                    new_content = f"{new_block}{existing_content}"
                    f.seek(0)
                    f.write(new_content)
                    f.truncate()
                else:
                    # Case 2: Marker found, check for differences
                    existing_body = match.group(1)
                    if existing_body != new_body:
                        print("   -> Template differs. Updating...", file=sys.stderr)
                        new_content = pattern.sub(new_block, existing_content)
                        f.seek(0)
                        f.write(new_content)
                        f.truncate()
                    else:
                        print("   -> Already up-to-date.", file=sys.stderr)

        except subprocess.CalledProcessError as e:
            print(
                f"   -> Error processing target '{template_target}': {e.stderr}",
                file=sys.stderr,
            )
        except Exception as e:
            print(f"   -> An unexpected error occurred: {e}", file=sys.stderr)

    print("\nUpdate check complete.")


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
        print(
            f"Usage: python {sys.argv[0]} <template_target | test | update> [lang]",
            file=sys.stderr,
        )
        print(
            "  <template_target>: Generate a template to stdout (e.g., 'Statement_BLOCK').",
            file=sys.stderr,
        )
        print(
            "  test             : Test generation for all available templates.",
            file=sys.stderr,
        )
        print(
            "  update           : Update all hook files in the specified [lang] directory.",
            file=sys.stderr,
        )
        print(
            "  [lang]           : Optional/Required. For new templates, saves to 'src/ebmcg/ebm2<lang>/visitor/'. Required for 'update'.",
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
    elif target_arg == "update":
        if len(sys.argv) < 3:
            print(
                "Error: 'update' command requires a [lang] argument.", file=sys.stderr
            )
            sys.exit(1)
        lang_arg = sys.argv[2]
        run_update_hooks(tool_path, lang_arg)
    elif len(sys.argv) == 3:
        lang_arg = sys.argv[2]
        run_save_template(tool_path, target_arg, lang_arg)
    else:
        run_single_template(tool_path, target_arg)


if __name__ == "__main__":
    main()
