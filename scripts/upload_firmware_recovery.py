"""Pace a firmware upload to the legacy v2.0.0/2.0.1 multipart OTA endpoint.

Uses only Python's standard library. Never retries the upload automatically.
Credentials are requested interactively and are not logged or stored.
"""
import argparse
import base64
import getpass
import http.client
import json
from pathlib import Path
import time
from urllib.parse import urlsplit

MAX_IMAGE_BYTES = 0x600000
BOUNDARY = "HomeControlPanelRecovery202"


def connection_for(host, timeout=120):
    parsed = urlsplit(host if "://" in host else "http://" + host)
    if parsed.scheme not in {"http", "https"} or not parsed.hostname or parsed.username or parsed.path not in {"", "/"}:
        raise ValueError("Use the panel hostname/IP or its HTTP(S) root URL.")
    cls = http.client.HTTPSConnection if parsed.scheme == "https" else http.client.HTTPConnection
    return cls(parsed.hostname, parsed.port, timeout=timeout)


def get_status(host, authorization):
    connection = connection_for(host, timeout=5)
    try:
        connection.request("GET", "/api/status", headers={"Authorization": authorization, "Cache-Control": "no-store"})
        response = connection.getresponse()
        result = json.loads(response.read().decode())
        if response.status != 200:
            raise RuntimeError(result.get("error", f"HTTP {response.status}"))
        return result
    finally:
        connection.close()


def upload(connection, firmware, authorization, rate=65536, sleep=time.sleep, progress=None):
    """Stream bounded blocks at the requested rate; testable without a network."""
    size = firmware.stat().st_size
    with firmware.open("rb") as image:
        if not 0 < size <= MAX_IMAGE_BYTES or image.read(1) != b"\xe9":
            raise ValueError("Choose a non-empty ESP application .bin image no larger than 6 MB.")
        image.seek(0)
        prefix = (f"--{BOUNDARY}\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n"
                  "Content-Type: application/octet-stream\r\n\r\n").encode()
        suffix = f"\r\n--{BOUNDARY}--\r\n".encode()
        connection.putrequest("POST", "/api/firmware")
        connection.putheader("Authorization", authorization)
        connection.putheader("Content-Type", f"multipart/form-data; boundary={BOUNDARY}")
        connection.putheader("Content-Length", str(len(prefix) + size + len(suffix)))
        connection.endheaders()
        connection.send(prefix)
        sent = 0
        while block := image.read(1024):
            connection.send(block)
            sent += len(block)
            sleep(len(block) / rate) # Force gaps so the old parser yields while waiting for data.
            if progress:
                progress(sent, size)
        connection.send(suffix)
        response = connection.getresponse()
        payload = response.read().decode(errors="replace")
        try:
            result = json.loads(payload)
        except ValueError:
            raise RuntimeError(f"The panel returned HTTP {response.status} without a firmware result.") from None
        if response.status != 200 or not result.get("ok"):
            raise RuntimeError(result.get("error", f"Firmware rejected: HTTP {response.status}"))
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Panel IP, hostname, or HTTP(S) root URL")
    parser.add_argument("--firmware", required=True, type=Path, help="Correct 2624 or 2635 firmware.bin")
    parser.add_argument("--username", default="admin")
    args = parser.parse_args()
    if args.firmware.suffix.lower() != ".bin":
        parser.error("The firmware file must use the .bin extension.")
    # Check the file before requesting credentials or making any network request.
    with args.firmware.open("rb") as image:
        if not 0 < args.firmware.stat().st_size <= MAX_IMAGE_BYTES or image.read(1) != b"\xe9":
            parser.error("Choose a valid ESP application .bin image no larger than 6 MB.")
    password = getpass.getpass("Web Admin password: ")
    authorization = "Basic " + base64.b64encode(f"{args.username}:{password}".encode()).decode()
    before = get_status(args.host, authorization)
    print(f"Panel {before.get('device_id', '')}: v{before.get('version', 'unknown')}")
    print("Uploading at 64 KB/s. Keep the panel powered on; this takes about one minute.")
    connection = connection_for(args.host)
    last_percent = -1

    def progress(sent, size):
        nonlocal last_percent
        percent = sent * 100 // size
        if percent != last_percent and (percent % 10 == 0 or percent == 100):
            print(f"Sent {percent}% (server verification pending)", flush=True)
            last_percent = percent

    try:
        result = upload(connection, args.firmware, authorization, progress=progress)
        print(result.get("message", "Firmware accepted. Waiting for reboot."))
    except (OSError, http.client.HTTPException, RuntimeError) as error:
        print(f"Upload did not return a confirmed result: {error}")
        print("Checking whether the panel activated v2.0.2; the upload will not be repeated.")
    finally:
        connection.close()
    time.sleep(5)
    for _ in range(24):
        try:
            after = get_status(args.host, authorization)
            rebooted = (after.get("boot_id") and after.get("boot_id") != before.get("boot_id")) or after.get("uptime_ms", 2**32) < before.get("uptime_ms", 0)
            if after.get("version") == "2.0.2" and rebooted:
                print("Confirmed: the panel rebooted and is running v2.0.2.")
                return 0
        except (OSError, http.client.HTTPException, RuntimeError, ValueError):
            pass
        time.sleep(2.5)
    print("Activation was not confirmed. Check the panel version before retrying.")
    print("If the old OTA handler still fails, install v2.0.2 once by USB without flashing the filesystem.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
