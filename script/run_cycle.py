import subprocess as sp
import sys


def check_call(*args, **kwargs):
    ret = sp.call(*args, **kwargs)
    if ret != 0:
        print(f"Command failed with exit code {ret}")
        sys.exit(ret)


check_call(["python", "script/generate.py"], stdout=sys.stdout, stderr=sys.stderr)
check_call(["python", "script/run_generated.py"], stdout=sys.stdout, stderr=sys.stderr)
check_call(["python", "script/run_cmptest.py"], stdout=sys.stdout, stderr=sys.stderr)
