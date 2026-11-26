#!/usr/bin/env python3
"""Build sys/http/client.fetch with both backends and exercise it over loopback."""

import argparse
import os
import select
import socket
import subprocess
import tempfile
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
FIXTURE = Path(__file__).resolve().with_name("client.orb")
WINDOWS = os.name == "nt"
SUFFIX = ".exe" if WINDOWS else ""
REPLIES = (
    ("content-length", b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello", "hello"),
    ("chunked", b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
     b"2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n", "hello"),
    ("close-delimited", b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nhello", "hello"),
    ("404-body", b"HTTP/1.1 404 Not Found\r\nContent-Length: 7\r\n\r\nmissing", "missing"),
    ("truncated-body", b"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort", None),
)


def run_client(exe, url, expected, listener=None, wire=None):
    kind = "err" if expected is None else "ok"
    with subprocess.Popen([str(exe), url, kind, expected or ""], cwd=exe.parent,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as client:
        try:
            if wire is not None:
                listener.settimeout(10)
                conn, _ = listener.accept()
                with conn, conn.makefile("rb") as request:
                    conn.settimeout(10)
                    if request.readline(256) != b"GET /probe HTTP/1.1\r\n":
                        raise RuntimeError("client did not send the expected GET request")
                    while True:
                        header = request.readline(8192)
                        if header == b"\r\n":
                            break
                        if not header.endswith(b"\r\n"):
                            raise RuntimeError("client did not terminate its request headers")
                    conn.sendall(wire)
            stdout, stderr = client.communicate(timeout=15)
            if client.returncode != 0:
                raise RuntimeError(f"client exit={client.returncode}, stdout={stdout!r}, stderr={stderr!r}")
            if listener is not None and wire is None and select.select([listener], [], [], 0)[0]:
                raise RuntimeError("client opened a connection for a rejected URL")
        finally:
            if client.poll() is None:
                client.kill()
                client.communicate()


def check_backend(exe):
    for name, wire, expected in REPLIES:
        with socket.create_server(("127.0.0.1", 0)) as listener:
            port = listener.getsockname()[1]
            run_client(exe, f"http://127.0.0.1:{port}/probe", expected, listener, wire)
        print(f"PASS {exe.stem} {name}", flush=True)

    with socket.socket() as closed_port:
        closed_port.bind(("127.0.0.1", 0))
        port = closed_port.getsockname()[1]
        run_client(exe, f"http://127.0.0.1:{port}/probe", None)
    print(f"PASS {exe.stem} connection-refused", flush=True)

    with socket.create_server(("127.0.0.1", 0)) as listener:
        port = listener.getsockname()[1]
        run_client(exe, f"https://127.0.0.1:{port}/probe", None, listener)
    print(f"PASS {exe.stem} https-refused", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--orbit-root", required=True, type=Path)
    parser.add_argument("--r6", type=Path, default=REPO / ".r6-build" / f"r6{SUFFIX}")
    parser.add_argument("--runtime-dir", type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="r6-http-client-") as temp:
        work = Path(temp)
        ir = work / "client.ir"
        native = work / f"native{SUFFIX}"
        oracle = work / f"oracle{SUFFIX}"
        subprocess.run([str(args.compiler.resolve()), "build", str(FIXTURE), "--backend=c",
                        "--emit-ir", str(ir), "-o", str(oracle)],
                       cwd=args.orbit_root, check=True, timeout=90)
        command = [str(args.r6.resolve()), "build", str(ir), "-o", str(native)]
        if args.runtime_dir is not None:
            command += ["--runtime-dir", str(args.runtime_dir.resolve())]
        subprocess.run(command, cwd=REPO, check=True, timeout=90)
        check_backend(oracle)
        check_backend(native)


if __name__ == "__main__":
    main()
