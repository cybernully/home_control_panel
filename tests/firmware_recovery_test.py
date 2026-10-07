"""Verify the actual recovery uploader's multipart bytes and pacing."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("recovery", root / "scripts/upload_firmware_recovery.py")
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


class Connection:
    def __init__(self, ok=True):
        self.headers = {}
        self.blocks = []
        self.status = 200 if ok else 400
    def putrequest(self, method, path): self.request = (method, path)
    def putheader(self, key, value): self.headers[key] = value
    def endheaders(self): pass
    def send(self, block): self.blocks.append(block)
    def getresponse(self): return self
    def read(self): return json.dumps({"ok": self.status == 200, "error": "invalid image"}).encode()


class RecoveryTest(unittest.TestCase):
    def test_stream_and_pacing(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            payload = b"\xe9" + bytes(range(256)) * 14000
            firmware.write_bytes(payload)
            connection, delays = Connection(), []
            self.assertTrue(recovery.upload(connection, firmware, "Basic test", sleep=delays.append)["ok"])
            body = b"".join(connection.blocks)
            self.assertEqual(int(connection.headers["Content-Length"]), len(body))
            self.assertEqual(connection.request, ("POST", "/api/firmware"))
            self.assertEqual(b"".join(connection.blocks[1:-1]), payload)
            self.assertTrue(all(0 < len(block) <= 1024 for block in connection.blocks[1:-1]))
            self.assertAlmostEqual(sum(delays), len(payload) / 65536)
            with self.assertRaisesRegex(RuntimeError, "invalid image"):
                recovery.upload(Connection(False), firmware, "Basic test", sleep=lambda _: None)

    def test_invalid_image_never_posts(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"invalid")
            connection = Connection()
            with self.assertRaises(ValueError): recovery.upload(connection, firmware, "Basic test")
            self.assertEqual(connection.blocks, [])


if __name__ == "__main__": unittest.main()
