from pathlib import Path
import os, subprocess, sys
root=Path(__file__).resolve().parents[1]
os.chdir(root)
zig=root/'.build-venv/Lib/site-packages/ziglang/zig.exe'
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(root/'.test-build/zig-cache')
lvgl=root/'.test-deps/lvgl-9.3.0'
flags=['-DLV_CONF_SKIP','-DLV_USE_OS=0','-DLV_MEM_SIZE=2097152','-DLV_COLOR_DEPTH=32']
flags += [f'-DLV_FONT_MONTSERRAT_{size}=1' for size in [12,14,16,18,20,24,28]]
media = '--media' in sys.argv
header = '--header' in sys.argv
fixture = 'tests/media_render_test.cpp' if media else 'tests/header_render_test.cpp' if header else 'tests/room_render_test.cpp'
executable = '.test-build/media_render_test.exe' if media else '.test-build/header_render_test.exe' if header else '.test-build/room_render_test.exe'
args=['-O0','-Itests/host','-Iinclude','-Isrc/modules',f'-I{lvgl}',*flags,
      fixture]
if media:
    jpeg=root/'.pio/libdeps/jc8012p4a1c_2635/JPEGDEC/src'
    # Third-party embedded codecs use integer operations outside UBSan's host assumptions.
    args += ['-fno-sanitize=undefined','-D__LINUX__',f'-I{jpeg}','-Ilib/stb','src/modules/media_module.cpp','src/stb_image_impl.cpp',str(jpeg/'JPEGDEC.cpp')]
else:
    args += ['src/modules/room_module.cpp']
args += ['-x','c']
args += [str(p) for p in (lvgl/'src').rglob('*.c')]
args += ['-o',executable]
# A response file avoids Windows' command length limit.
response=root/'.test-build/render-args.txt'
response.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args),encoding='utf8')
subprocess.run([str(zig),'c++','@'+str(response)],check=True)
subprocess.run([str(root/executable)],check=True)





