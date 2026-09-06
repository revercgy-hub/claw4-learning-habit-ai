#!/usr/bin/env python3
"""Small LAN-only relay for the Claw4 device backend smoke test.

The family backend remains bound to 127.0.0.1:8000 by policy. This relay is
bound to one explicitly supplied private-LAN address and forwards only
/api/v1/* to the loopback backend. It never logs request bodies or headers.
It is a development helper, not a production proxy.
"""

from __future__ import annotations

import argparse
import http.server
import socketserver
import urllib.error
import urllib.request
from http import HTTPStatus


class RelayHandler(http.server.BaseHTTPRequestHandler):
    server_version = "Claw4DeviceRelay/1.0"

    def _relay(self) -> None:
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
        request = urllib.request.Request(
            target, data=body, headers=headers, method=self.command
        )
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                payload = response.read()
                self.send_response(response.status)
                response_type = response.headers.get("Content-Type")
                if response_type:
                    self.send_header("Content-Type", response_type)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
        except urllib.error.HTTPError as error:
            payload = error.read()
            self.send_response(error.code)
            response_type = error.headers.get("Content-Type")
            if response_type:
                self.send_header("Content-Type", response_type)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except (OSError, urllib.error.URLError):
            self.send_error(HTTPStatus.BAD_GATEWAY)

    def do_GET(self) -> None:  # noqa: N802
        self._relay()

    def do_POST(self) -> None:  # noqa: N802
        self._relay()

    def do_PATCH(self) -> None:  # noqa: N802
        self._relay()

    def do_OPTIONS(self) -> None:  # noqa: N802
        self._relay()

    def log_message(self, format: str, *args: object) -> None:
        # Do not print paths, headers, bodies or tokens during device tests.
        print("device-backend-relay:", self.command, "status", args[1])


class RelayServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, bind: str, port: int, target_port: int):
        self.target_port = target_port
        super().__init__((bind, port), RelayHandler)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", required=True, help="explicit private LAN IPv4")
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--target-port", type=int, default=8000)
    args = parser.parse_args()
    server = RelayServer(args.bind, args.port, args.target_port)
    print(
        f"relay listening on http://{args.bind}:{args.port} -> "
        f"http://127.0.0.1:{args.target_port}"
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

