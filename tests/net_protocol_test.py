#!/usr/bin/env python3
"""Loopback-only TCP integration tests for framing and partial I/O."""

from __future__ import annotations

import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def reserve_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def connect(port: int) -> socket.socket:
    sock = socket.create_connection(("127.0.0.1", port), timeout=2.0)
    sock.settimeout(2.0)
    return sock


def wait_until_ready(process: subprocess.Popen[str], port: int) -> None:
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if process.poll() is not None:
            output = process.stdout.read() if process.stdout else ""
            raise AssertionError(f"server exited early ({process.returncode}): {output}")
        try:
            with connect(port):
                return
        except OSError:
            time.sleep(0.02)
    raise AssertionError("server did not become ready")


def exchange(port: int, request: bytes, *, bytewise: bool = False) -> str:
    with connect(port) as sock:
        if bytewise:
            for byte in request:
                sock.sendall(bytes((byte,)))
        else:
            sock.sendall(request)

        chunks: list[bytes] = []
        while True:
            chunk = sock.recv(3 if bytewise else 4096)
            if not chunk:
                break
            chunks.append(chunk)
        return b"".join(chunks).decode("utf-8", errors="replace")


def main() -> int:
    port = reserve_port()
    process = subprocess.Popen(
        [str(ROOT / "imagedb-server"), str(port)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        wait_until_ready(process, port)

        output = exchange(
            port,
            b"LIST\r\nINFO 1\r\nSEARCH 1 2\r\nQUIT\r\n",
            bytewise=True,
        )
        assert "sample1" in output, output
        assert "Width:" in output, output
        assert "Metric: intersection" in output, output
        assert "BYE" in output, output
        print("  tcp(partial read + fragmented receive + CRLF): PASS")

        output = exchange(
            port,
            b"LIST extra\n"
            b"INFO 999999999999999999999999\n"
            b"SEARCH 1 999999999999999999999999\n"
            b"SEARCH 1 -1\n"
            b"SEARCH 1 1001\n"
            b"UNKNOWN\n"
            b"QUIT\n",
        )
        assert "ERROR: Usage: LIST" in output, output
        assert "ERROR: Usage: INFO <id>" in output, output
        assert "ERROR: Usage: SEARCH <id> <k>" in output, output
        assert "ERROR: top_k must be positive" in output, output
        assert "ERROR: top_k too large" in output, output
        assert "ERROR: Unknown command" in output, output
        print("  tcp(invalid requests): PASS")

        output = exchange(
            port,
            (b"X" * 5000) + b"\nLIST\nQUIT\n",
        )
        assert output.count("ERROR: Line too long") == 1, output
        assert "ERROR: Unknown command" not in output, output
        assert "sample1" in output, output
        print("  tcp(overlong request recovery): PASS")

        output = exchange(port, b"LIST\x00ignored\nLIST\nQUIT\n")
        assert "ERROR: NUL byte is not allowed" in output, output
        assert "sample1" in output and "BYE" in output, output
        print("  tcp(NUL request recovery): PASS")

        with connect(port) as truncated:
            truncated.sendall(b"INFO 1")
        time.sleep(0.05)
        assert process.poll() is None, "server died after truncated request"

        sock = connect(port)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        sock.sendall(b"LIST\n")
        sock.close()
        time.sleep(0.1)
        assert process.poll() is None, "server died after client reset"
        output = exchange(port, b"LIST\nQUIT\n")
        assert "sample1" in output and "BYE" in output, output
        print("  tcp(truncated/reset client + reconnect): PASS")
    finally:
        process.terminate()
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2.0)

    print("network protocol integration tests: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
