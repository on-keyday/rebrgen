# run src/ebm/ebm.py to update the C++ files
import subprocess as sp
import sys

sp.check_call(["python", "src/ebm/ebm.py"], stdout=sys.stdout, stderr=sys.stderr)

# then run script/build.py to build the tools
sp.check_call(["python", "script/build.py"], stdout=sys.stdout, stderr=sys.stderr)

something_changed = False


def rewrite_if_needed(file_path: str, new_content: str):
    try:
        with open(file_path, "r") as f:
            existing_content = f.read()
        if existing_content == new_content:
            print(f"No changes needed for: {file_path}")
            return False
    except FileNotFoundError:
        pass  # File does not exist, will create it

    with open(file_path, "w") as f:
        f.write(new_content)
    print(f"Updated: {file_path}")
    return True


# then run tool/ebmcodegen --mode subset > src/ebmcodegen/body_subset.cpp
subset = sp.check_output(["tool/ebmcodegen", "--mode", "subset"], stderr=sys.stderr)

if rewrite_if_needed("src/ebmcodegen/body_subset.cpp", subset.decode("utf-8")):
    something_changed = True

# run tool/ebmcodegen --mode json-conv-header > src/ebmgen/json_conv.hpp
json_header = sp.check_output(
    ["tool/ebmcodegen", "--mode", "json-conv-header"], stderr=sys.stderr
)
if rewrite_if_needed("src/ebmgen/json_conv.hpp", json_header.decode("utf-8")):
    something_changed = True

# run tool/ebmcodegen --mode json-conv-source > src/ebmgen/json_conv.cpp
json_source = sp.check_output(
    ["tool/ebmcodegen", "--mode", "json-conv-source"], stderr=sys.stderr
)
if rewrite_if_needed("src/ebmgen/json_conv.cpp", json_source.decode("utf-8")):
    something_changed = True

if something_changed:
    # rerun script/build.py to build the tools again
    sp.check_call(["python", "script/build.py"], stdout=sys.stdout, stderr=sys.stderr)

hex_test_data = sp.check_output(
    [
        "tool/ebmgen",
        "-i",
        "./src/ebm/extended_binary_module.bgn",
        "-o",
        "-",
        "--output-format",
        "hex",
    ],
    stderr=sys.stderr,
)


# split per 32 characters
lines = []
hex_str = hex_test_data.decode("utf-8").strip()
for i in range(0, len(hex_str), 32):
    lines.append(hex_str[i : i + 32])
# insert space every 2 characters
formatted_lines = []
for line in lines:
    formatted_lines.append(" ".join(line[i : i + 2] for i in range(0, len(line), 2)))
final_hex_content = "\n".join(formatted_lines) + "\n"
rewrite_if_needed("test/binary_data/extended_binary_module.dat", final_hex_content)
