"""Compile the real OTA lifecycle against a deterministic flash/clock adapter."""
from pathlib import Path
import os
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
os.chdir(root)
build = root / ".test-build"
build.mkdir(exist_ok=True)
zig = root / ".build-venv/Lib/site-packages/ziglang/zig.exe"
compiler = [str(zig), "c++"] if zig.exists() else shlex.split(os.environ.get("CXX", "c++"))
os.environ["ZIG_GLOBAL_CACHE_DIR"] = str(build / "zig-cache")
output = build / "firmware_update_test.exe"
subprocess.run(compiler + ["-std=c++17", "-Itests/ota_host", "-Iinclude",
    "tests/firmware_update_test.cpp", "src/firmware_update.cpp", "-o", str(output)], check=True)
for case in ["size", "begin_failure", "busy", "truncated", "overflow", "magic",
             "write_failure", "verification_failure", "abort", "timeout", "success", "legacy"]:
    subprocess.run([str(output), case], check=True)

# Exercise the actual handler functions with an authenticated WebServer adapter.
# The Arduino parser/real flash remain an explicit hardware verification boundary.
source = (root / "src/web_manager.cpp").read_text()
request_struct = source[source.index("struct FirmwareRequest {"):source.index("} g_firmware_request;") + len("} g_firmware_request;")]
handlers = source[source.index("// Multipart callbacks never activate"):source.index("void handle_reboot()")]
generated = build / "firmware_routes_test.cpp"
generated.write_text('#include "web_adapter.h"\n' + request_struct + '\n' + handlers + '\n' +
    (root / "tests/firmware_routes_test.cpp").read_text())
output = build / "firmware_routes_test.exe"
subprocess.run(compiler + ["-std=c++17", "-Itests/ota_host", "-Iinclude",
    str(generated), "src/firmware_update.cpp", "-o", str(output)], check=True)
for case in ["unauthorized", "session", "offset", "size", "content_length", "truncated",
             "multiple_parts", "disconnect_after_end", "success", "legacy"]:
    subprocess.run([str(output), case], check=True)
