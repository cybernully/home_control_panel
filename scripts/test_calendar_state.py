from pathlib import Path
import os, subprocess

root=Path(__file__).resolve().parents[1]
os.chdir(root)
zig=root/'.build-venv/Lib/site-packages/ziglang/zig.exe'
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(root/'.test-build/zig-cache')
output=root/'.test-build/calendar_state_model_test.exe'
subprocess.run([str(zig),'c++','-std=c++17','-Itests/host','-Iinclude',
                'tests/calendar_state_model_test.cpp','src/calendar_state_model.cpp',
                '-o',str(output)],check=True)
subprocess.run([str(output)],check=True)
