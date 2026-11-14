import hashlib
import io
import json
import tempfile
import unittest
import uuid
from pathlib import Path
from unittest.mock import patch

from learning_pairing_usb import PairingError, build_frame, main, send_on_stream, verify_manifest


DEVICE = "123e4567-e89b-42d3-a456-426614174000"
SECRET = "a" * 64
PAYLOAD = {"base_url": "https://192.168.3.100:8744", "device_id": DEVICE,
           "child_id": "child_1", "device_secret": SECRET}
ORIGIN = "https://192.168.3.100:8744"


class FakeSerial:
    def __init__(self, lines):
        self.lines = iter(lines)
        self.written = []
        self.flushed = False

    def readline(self):
        return next(self.lines, b"")

    def write(self, data):
        self.written.append(data)

    def flush(self):
        self.flushed = True


class LearningPairingUsbTests(unittest.TestCase):
    def test_frame_digest_is_over_exact_ascii_frame(self):
        frame, digest = build_frame(PAYLOAD, ORIGIN)
        self.assertEqual(frame, f"C4PAIR1 {DEVICE} child_1 {SECRET}\n".encode())
        self.assertEqual(digest, hashlib.sha256(frame).hexdigest())

    def test_rejects_invalid_fields_and_nonmatching_origin(self):
        for key, value in (("child_id", "bad child"), ("device_secret", "A" * 64),
                           ("device_id", str(uuid.uuid4()).upper()),
                           ("base_url", "https://192.168.3.101:8744")):
            bad = dict(PAYLOAD, **{key: value})
            with self.subTest(key=key), self.assertRaises(PairingError):
                build_frame(bad, ORIGIN)

    def test_manifest_requires_both_digest_fields_and_elf(self):
        _, digest = build_frame(PAYLOAD, ORIGIN)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "candidate.json"
            path.write_text(json.dumps({"learning_usb_pairing": {
                "frame_sha256": digest, "include_sha256": digest},
                "elf": {"sha256": "b" * 64}}), encoding="utf-8")
            self.assertEqual(verify_manifest(path, digest), "b" * 64)
            path.write_text("{}", encoding="utf-8")
            with self.assertRaises(PairingError):
                verify_manifest(path, digest)

    def test_send_waits_for_matching_boot_and_ready_and_saved(self):
        frame, digest = build_frame(PAYLOAD, ORIGIN)
        elf = "b" * 64
        stream = FakeSerial([
            f"I (1) app_init: ELF file SHA256:  {'c' * 64}\n".encode(),
            f"I (2) app_init: ELF file SHA256:  {elf}\n".encode(),
            f"I (3) Pair: LEARNING_PAIRING_READY {'0' * 64}\n".encode(),
            f"I (4) Pair: LEARNING_PAIRING_READY {digest}\n".encode(),
            f"I (5) Pair: LEARNING_PAIRING_SAVED {digest}\n".encode(),
        ])
        send_on_stream(stream, frame, digest, elf, timeout=1)
        self.assertEqual(stream.written, [frame])
        self.assertTrue(stream.flushed)

    def test_does_not_send_without_exact_ready_digest(self):
        frame, digest = build_frame(PAYLOAD, ORIGIN)
        elf = "b" * 64
        stream = FakeSerial([f"ELF file SHA256:  {elf}\n".encode(),
                             f"LEARNING_PAIRING_READY {'0' * 64}\n".encode()])
        with self.assertRaises(PairingError):
            send_on_stream(stream, frame, digest, elf, timeout=0.001)
        self.assertEqual(stream.written, [])

    def test_stdin_mode_emits_only_digest_metadata(self):
        out = io.StringIO()
        code = main(["--digest-only"], io.StringIO(json.dumps(PAYLOAD)), out)
        result = json.loads(out.getvalue())
        self.assertEqual(code, 0)
        self.assertEqual(result["status"], "digest_ready")
        self.assertNotIn(SECRET, out.getvalue())
        self.assertNotIn(DEVICE, out.getvalue())


if __name__ == "__main__":
    unittest.main()
