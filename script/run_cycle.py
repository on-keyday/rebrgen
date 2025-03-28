import subprocess as sp
import sys


sp.check_call(["python", "script/generate.py"], stdout=sys.stdout, stderr=sys.stderr)
sp.check_call(
    ["python", "script/run_generated.py"], stdout=sys.stdout, stderr=sys.stderr
)
sp.check_call(["python", "script/run_cmptest.py"], stdout=sys.stdout, stderr=sys.stderr)
