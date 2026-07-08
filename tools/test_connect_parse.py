#!/usr/bin/env python3
"""Run CONNECT parse unit test."""
import subprocess, sys, os
base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
inc_server = os.path.join(base, 'include', 'server')
inc_common = os.path.join(base, 'include', 'common')
inc = os.path.join(base, 'include')
srcs = [os.path.join(base, 'src', 'server', s) for s in
        ('mqtt_broker.c', 'mqtt_parser.c', 'mqtt_codec.c',
         'mqtt_topic.c', 'mqtt_connection.c', 'connection.c')]
srcs += [os.path.join(base, 'src', 'common', 'log.c')]
test = os.path.join(base, 'tools', 'test_connect_parse.c')
out = os.path.join(base, 'build', 'test_connect_parse')
cmd = (['gcc', '-Wall', '-Wextra', '-Werror', '-std=c11', '-g',
        f'-I{inc}', f'-I{inc_server}', f'-I{inc_common}',
        test] + srcs + ['-o', out, '-lpthread'])
rc = subprocess.run(cmd, capture_output=True, text=True)
if rc.returncode != 0:
    print("compile failed:", rc.stderr, file=sys.stderr)
    sys.exit(1)
rc = subprocess.run([out], capture_output=True, text=True)
print(rc.stdout, end='')
sys.exit(rc.returncode)
