"""Build a SHA-committed learning pairing frame and optionally send it over USB."""
import argparse
import hashlib
import ipaddress
import json
import re
import sys
import time
import uuid
from urllib.parse import urlsplit
from pathlib import Path

MAX_STDIN = 4096
HEX64 = re.compile(r"^[0-9a-f]{64}$")
CHILD = re.compile(r"^[A-Za-z0-9_-]{1,128}$")
READY = re.compile(rb"LEARNING_PAIRING_READY ([0-9a-f]{64})")
SAVED = re.compile(rb"LEARNING_PAIRING_SAVED ([0-9a-f]{64})")
ELF = re.compile(rb"LEARNING_PAIRING_ELF ([0-9a-f]{64})(?![0-9a-f])")


class PairingError(Exception):
    pass


def build_frame(payload, origin):
    if not isinstance(payload, dict) or set(payload) != {"base_url", "device_id", "child_id", "device_secret"}:
        raise PairingError("invalid_input")
    base = payload["base_url"]
    if not isinstance(base, str) or base != origin:
        raise PairingError("invalid_input")
    try:
        parsed_origin = urlsplit(origin)
        ip = ipaddress.ip_address(parsed_origin.hostname or "")
        if (parsed_origin.scheme != "https" or ip.version != 4 or
                parsed_origin.port != 8744 or parsed_origin.path or
                parsed_origin.query or parsed_origin.fragment or
                parsed_origin.username or parsed_origin.password):
            raise ValueError()
    except ValueError:
        raise PairingError("invalid_origin") from None
    device = payload["device_id"]
    try:
        parsed = uuid.UUID(device)
        if str(parsed) != device or parsed.version != 4:
            raise ValueError()
    except (ValueError, AttributeError, TypeError):
        raise PairingError("invalid_input") from None
    child, secret = payload["child_id"], payload["device_secret"]
    if not isinstance(child, str) or not CHILD.fullmatch(child):
        raise PairingError("invalid_input")
    if not isinstance(secret, str) or not HEX64.fullmatch(secret):
        raise PairingError("invalid_input")
    frame = f"C4PAIR1 {device} {child} {secret}\n".encode("ascii")
    return frame, hashlib.sha256(frame).hexdigest()


def verify_manifest(path, digest):
    try:
        manifest = json.loads(Path(path).read_text(encoding="utf-8"))
        pairing = manifest["learning_usb_pairing"]
        elf = manifest["elf"]["sha256"]
        if not HEX64.fullmatch(digest) or pairing["frame_sha256"] != digest:
            raise ValueError()
        include = f'constexpr char kLearningPairingSha256[] = "{digest}";\n'.encode("ascii")
        if (pairing["include_sha256"] != hashlib.sha256(include).hexdigest() or
                not HEX64.fullmatch(elf)):
            raise ValueError()
        return elf
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        raise PairingError("manifest_mismatch") from None


def send_on_stream(stream, frame, digest, elf_sha256, timeout=30):
    """Send only after matching boot ELF and READY markers; await SAVED readback."""
    deadline = time.monotonic() + timeout
    got_elf = got_ready = False
    while time.monotonic() < deadline:
        line = stream.readline(512)
        if not line:
            continue
        match = ELF.search(line)
        if match:
            got_elf = match.group(1).decode("ascii") == elf_sha256
        match = READY.search(line)
        if got_elf and match and match.group(1).decode("ascii") == digest:
            got_ready = True
        if got_elf and got_ready:
            break
    if not (got_elf and got_ready):
        raise PairingError("device_not_ready")
    if stream.write(frame) != len(frame):
        raise PairingError("short_write")
    if hasattr(stream, "flush"):
        stream.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        match = SAVED.search(stream.readline(512))
        if match and match.group(1).decode("ascii") == digest:
            return
    raise PairingError("save_unconfirmed")


def main(argv=None, stdin=None, stdout=None):
    stdin, stdout = stdin or sys.stdin, stdout or sys.stdout
    parser = argparse.ArgumentParser()
    parser.add_argument("--origin", default="https://192.168.3.100:8744")
    parser.add_argument("--digest-only", action="store_true", default=True)
    parser.add_argument("--port")
    parser.add_argument("--manifest")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args(argv)
    if bool(args.port) != bool(args.manifest):
        raise PairingError("serial_requires_port_and_manifest")
    raw = stdin.read(MAX_STDIN + 1)
    if len(raw) > MAX_STDIN:
        raise PairingError("input_too_large")
    try:
        payload = json.loads(raw)
    except (json.JSONDecodeError, TypeError):
        raise PairingError("invalid_input") from None
    frame, digest = build_frame(payload, args.origin)
    result = {"schema": "learning-pairing-usb/1", "frame_sha256": digest}
    if args.port:
        elf_sha = verify_manifest(args.manifest, digest)
        try:
            import serial
        except ImportError:
            raise PairingError("serial_unavailable") from None
        try:
            stream = serial.Serial(port=None, baudrate=args.baud, timeout=0.25, write_timeout=5)
            # Set lines before opening so pyserial cannot pulse the board during open.
            stream.port = args.port
            stream.dtr = False
            stream.rts = False
            stream.open()
            try:
                send_on_stream(stream, frame, digest, elf_sha)
            finally:
                stream.close()
        except PairingError:
            raise
        except Exception:
            raise PairingError("serial_failure") from None
        result["status"] = "saved"
    else:
        result["status"] = "digest_ready"
    stdout.write(json.dumps(result, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PairingError as exc:
        # Error codes are fixed and never include input, paths, or credentials.
        sys.stdout.write(json.dumps({"schema": "learning-pairing-usb/1", "error": str(exc)}, separators=(",", ":")) + "\n")
        raise SystemExit(2)
