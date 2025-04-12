#!/usr/bin/env python3
# Code generated by gen_template, DO NOT EDIT.
import sys
import os
import subprocess
import shutil

def run_command(args):
    print("run command: ", args)
    return subprocess.check_call(args,stdout = sys.stdout, stderr = sys.stderr)

def capture_command(args):
    print("run command with capture: ", args)
    return subprocess.check_output(args,stderr = sys.stderr)

def copy_file(src, dst):
    print("copy file: {} -> {}".format(src, dst))
    shutil.copyfile(src, dst)
def copy_dir(src, dst):
    print("copy dir: {} -> {}".format(src, dst))
    shutil.copytree(src, dst, dirs_exist_ok=True)
def move_file(src, dst):
    print("move file: {} -> {}".format(src, dst))
    shutil.move(src, dst)
def move_dir(src, dst):
    print("move dir: {} -> {}".format(src, dst))
    shutil.move(src, dst)
def write_file(filename, content):
    print("write file: {}".format(filename))
    with open(filename, "w") as f:
        f.write(content)
def read_file(filename):
    print("read file: {}".format(filename))
    with open(filename, "r") as f:
        return f.read()
if __name__ == "__main__":
    MODE = sys.argv[1]
    if MODE == "build":
        MAIN = sys.argv[2]
        EXEC = sys.argv[3]
        GENERATED = sys.argv[4]
        TMPDIR = sys.argv[5]
        DEBUG = sys.argv[6]
        CONFIG = sys.argv[7]
        CONFIG_DIR = os.path.dirname(CONFIG)
        run_command(["g++", MAIN, GENERATED, "-o", EXEC])
    elif MODE == "run":
        EXEC = sys.argv[2]
        INPUT = sys.argv[3]
        OUTPUT = sys.argv[4]
        exit(run_command([EXEC,INPUT,OUTPUT]))
    else:
        print("invalid mode")
        exit(1)
    
