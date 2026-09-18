#!/usr/bin/env python3
"""T1 diagnostic harness (LOCAL ONLY, not part of the repo's reviewed relay).

Same forwarding behaviour as tools/dev/run-device-backend-relay.py, but it
records the *response* body of /api/v1/events/batch so we can feed the exact
bytes the device received into the device's own wire decoder on the host.

Never logs request bodies or Authorization headers (device tokens stay out of
the log). Only: method, path, status, and the response body of events/batch.

Usage: t1_logging_relay.py --bind <ip> --port 18765 --target-port 8000 \
                           --out <logfile>
"""
from __future__ import annotations

import argparse
import http.server
import json
import socketserver
import threading
import urllib.error
import urllib.request
from http import HTTPStatus

LOCK = threading.Lock()
OUT = None


def record(entry: dict) -> None:
    with LOCK:
        with open(OUT, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(entry, ensure_ascii=False) + "\n")


class RelayHandler(http.server.BaseHTTPRequestHandler):
    server_version = "Claw4T1LoggingRelay/1.0"

    def _relay(self) -> None:  # noqa: N802
        if not self.path.startswith("/api/v1/"):
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else None
        target = f"http://127.0.0.1:{self.server.target_port}{self.path}"
        headers = {}
        content_type = self.headers.get("Content-Type")
        authorization = self.headers.get("Authorization")
        if content_type:
            headers["Content-Type"] = content_type
        if authorization:
            headers["Authorization"] = authorization
        request = urllib.request.Request(target, data=body, headers=headers,
                                         method=self.command)
        status = 0
        payload = b""
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                status = response.status
                payload = response.read()
                self.send_response(status)
                response_type = response.headers.get("Content-Type")
                if response_type:
                    self.send_header("Content-Type", response_type)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
        except urllib.error.HTTPError as error:
            status = error.code
            payload = error.read()
            self.send_response(status)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except (OSError, urllib.error.URLError):
            self.send_error(HTTPStatus.BAD_GATEWAY)
            status = 502
        if self.path.rstrip("/").endswith("/events/batch"):
            entry = {
                "method": self.command,
                "path": self.path,
                "status": status,
                "response_body": payload.decode("utf-8", "replace"),
            }
            # Request-side SUMMARY only: enough to see which sequences the
            # device actually sent, without ever storing the auth header or
            # any credential. Event ids/sequences/types are contract-level
            # identifiers, not secrets.
            try:
                req = json.loads(body.decode("utf-8")) if body else {}
                events = req.get("events") or []
                entry["request_summary"] = {
                    "last_acked_sequence": req.get("last_acked_sequence"),
                    "event_count": len(events),
                    "sequences": [e.get("sequence") for e in events],
                    "event_ids": [e.get("event_id") for e in events],
                    "types": [e.get("type") for e in events],
                    "child_id": req.get("child_id"),
                }
            except Exception as exc:  # noqa: BLE001
                entry["request_summary"] = {"error": repr(exc)}
            record(entry)

    def do_GET(self) -> None:  # noqa: N802
        self._relay()

    def do_POST(self) -> None:  # noqa: N802
        self._relay()

    def do_PATCH(self) -> None:  # noqa: N802
        self._relay()

    def do_OPTIONS(self) -> None:  # noqa: N802
        self._relay()

    def log_message(self, fmt: str, *args: object) -> None:
        print("t1-logging-relay:", self.command, "status", args[1])


class RelayServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, bind: str, port: int, target_port: int):
        self.target_port = target_port
        super().__init__((bind, port), RelayHandler)


def main() -> int:
    global OUT
    p = argparse.ArgumentParser()
    p.add_argument("--bind", required=True)
    p.add_argument("--port", type=int, default=18765)
    p.add_argument("--target-port", type=int, default=8000)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    OUT = args.out
    server = RelayServer(args.bind, args.port, args.target_port)
    print(f"t1-logging-relay on http://{args.bind}:{args.port} -> "
          f"127.0.0.1:{args.target_port}, response bodies -> {args.out}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
