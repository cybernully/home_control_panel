"""Run native room config/UTF-8 tests with CXX or the local Zig toolchain."""
from pathlib import Path
import os, shlex, subprocess
root = Path(__file__).resolve().parents[1]
os.chdir(root)
(root / '.test-build').mkdir(exist_ok=True)
zig = root / '.build-venv/Lib/site-packages/ziglang/zig.exe'
compiler = [str(zig), 'c++'] if zig.exists() else shlex.split(os.environ.get('CXX', 'c++'))
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(root / '.test-build/zig-cache')
output = root / '.test-build/room_config_test.exe'
subprocess.run(compiler + ['-std=c++17', '-Itests/host', '-Iinclude', '-I.test-deps',
    'tests/room_config_test.cpp', 'src/room_config.cpp', '-o', str(output)], check=True)
subprocess.run([str(output)], check=True)
