import sys
import subprocess as sp

CMPTEST_PATH = (
    "C:/workspace/shbrgen/brgen/tool/cmptest" if len(sys.argv) < 2 else sys.argv[1]
)

sp.run(
    [
        CMPTEST_PATH,
        "-f",
        "./testkit/test_info.json",
        "-c",
        "./testkit/cmptest.json",
        "--clean-tmp",
        "--save-tmp-dir",
    ],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
