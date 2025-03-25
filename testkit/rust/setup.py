import sys
import os
import subprocess

def run_command(args):
    print("run command: ", args)
    return subprocess.check_call(args,stdout = sys.stdout, stderr = sys.stderr)

def capture_command(args):
    print("run command with capture: ", args)
    return subprocess.check_output(args,stderr = sys.stderr)

if __name__ == "__main__":
    MODE = sys.argv[1]
    if MODE == "build":
        INPUT = sys.argv[2]
        OUTPUT = sys.argv[3]
        ORIGIN = sys.argv[4]
        TMPDIR = sys.argv[5]
        DEBUG = sys.argv[6]
        CONFIG = sys.argv[7]
        CONFIG_DIR = os.path.dirname(CONFIG)
        print("You have to implement build command in hook/cmptest_build.txt. see also testkit/rust/setup.py")
        exit(1)
    elif MODE == "run":
        EXEC = sys.argv[2]
        INPUT = sys.argv[3]
        OUTPUT = sys.argv[4]
        exit(run_command([EXEC,INPUT,OUTPUT]))
    else:
        print("invalid mode")
        exit(1)
    
