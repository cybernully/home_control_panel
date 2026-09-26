from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
os.chdir(root)
zig=root/'.build-venv/Lib/site-packages/ziglang/zig.exe'
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(root/'.test-build/zig-cache')
lvgl=root/'.test-deps/lvgl-9.3.0'
flags=['-DLV_CONF_SKIP','-DLV_USE_OS=0','-DLV_MEM_SIZE=2097152','-DLV_COLOR_DEPTH=32']
flags += [f'-DLV_FONT_MONTSERRAT_{size}=1' for size in [12,14,16,18,20,24,28]]
args=['-O0','-Itests/host','-Iinclude','-Isrc/modules',f'-I{lvgl}',*flags,
      'tests/room_render_test.cpp','src/modules/room_module.cpp','-x','c']
args += [str(p) for p in (lvgl/'src').rglob('*.c')]
args += ['-o','.test-build/room_render_test.exe']
# A response file avoids Windows' command length limit.
response=root/'.test-build/render-args.txt'
response.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args),encoding='utf8')
subprocess.run([str(zig),'c++','@'+str(response)],check=True)
subprocess.run([str(root/'.test-build/room_render_test.exe')],check=True)

