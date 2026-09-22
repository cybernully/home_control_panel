Import("env")
from pathlib import Path
import subprocess, urllib.request
PROJECT_DIR=Path(env.subst("$PROJECT_DIR"));FW_DIR=PROJECT_DIR/"data"/"hosted";FW_PATH=FW_DIR/"esp32c6-v2.12.3.bin";FW_URL="https://espressif.github.io/arduino-esp32/hosted/esp32c6-v2.12.3.bin";MIN_SIZE=64*1024;MAX_SIZE=4*1024*1024
def valid(path):
    if not path.exists() or not(MIN_SIZE<=path.stat().st_size<=MAX_SIZE):return False
    with path.open("rb") as f:return f.read(1)==b"\xE9"
def urllib_dl():
    req=urllib.request.Request(FW_URL,headers={"User-Agent":"HomePanel-PlatformIO/1.0.0"})
    with urllib.request.urlopen(req,timeout=90) as r:FW_PATH.write_bytes(r.read())
def curl_dl():subprocess.run(["curl","-fL","--retry","3","--connect-timeout","20","-o",str(FW_PATH),FW_URL],check=True)
FW_DIR.mkdir(parents=True,exist_ok=True)
if not valid(FW_PATH):
    if FW_PATH.exists():FW_PATH.unlink()
    print("[HomePanel] Downloading ESP32-C6 ESP-Hosted firmware 2.12.3...")
    try:urllib_dl()
    except Exception as e1:
        print(f"[HomePanel] Python download failed ({e1}); trying curl...")
        try:curl_dl()
        except Exception as e2:raise RuntimeError(f"Could not download {FW_URL}. urllib={e1}; curl={e2}")
if not valid(FW_PATH):raise RuntimeError(f"{FW_PATH} is not a valid ESP image")
print(f"[HomePanel] ESP32-C6 firmware ready: {FW_PATH.name} ({FW_PATH.stat().st_size} bytes)")
