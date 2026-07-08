#!/usr/bin/env python3
"""Run C topic matching unit tests and report results."""
import subprocess, sys, os
base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
inc = os.path.join(base, 'include', 'server')
src = os.path.join(base, 'src', 'server', 'mqtt_topic.c')
test = os.path.join(base, 'tools', 'test_topic.c')
out = os.path.join(base, 'build', 'test_topic')
rc = subprocess.run(
    ['gcc', '-Wall', '-Wextra', '-Werror', '-std=c11',
     f'-I{inc}', test, src, '-o', out],
    capture_output=True, text=True)
if rc.returncode != 0:
    print("compile failed:", rc.stderr, file=sys.stderr)
    sys.exit(1)
rc = subprocess.run([out], capture_output=True, text=True)
print(rc.stdout, end='')
sys.exit(rc.returncode)
