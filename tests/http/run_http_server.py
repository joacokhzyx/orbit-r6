#!/usr/bin/env python3
"""Run a route-only, native Orbit service alongside the C oracle."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing, contextmanager
import http.client
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
SUFFIX = ".exe" if os.name == "nt" else ""
SERVICE = """fn decorate(value: string) -> string {
    return "[" + value + "]"
}

route GET "/items/:rest..." {
    return ok 200 "multi"
}

route GET "/items/:value" {
    return ok 200 decorate(req.param("value"))
}

route GET "/items/fixed" {
    return ok 200 "fixed"
}

route POST "/echo" {
    val key = req.query("key")
    val header = req.header("X-Probe")
    res.header("X-Reply", "seen")
    return ok 201 decorate(key + ":" + header + ":" + req.body())
}

route GET "/burst" limit 1 / s burst 1 {
    return ok 200 "admitted"
}
"""


@contextmanager
def running_service(exe, compiler, work, argv, env):
    log = work / "server.log"
    with log.open("wb", buffering=0) as output:
        service = subprocess.Popen([str(exe), *argv], cwd=work, env=env,
                                   stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while service.poll() is None and time.monotonic() < deadline:
                ready = re.search(rb"localhost:([1-9][0-9]*)", log.read_bytes())
                if ready:
                    break
                time.sleep(0.05)
            else:
                raise RuntimeError(f"server did not start: {log.read_text(errors='replace')}")
            yield int(ready[1])
            if os.name == "nt":
                subprocess.run([str(compiler), "stop"], cwd=work, env=env, check=True, timeout=15)
            else:
                service.send_signal(signal.SIGTERM)
            if service.wait(timeout=8) != 0 or b"Server stopped." not in log.read_bytes():
                raise RuntimeError("server did not drain and exit normally")
        finally:
            if service.poll() is None:
                service.kill()
                service.wait()


def request(client, method, name, body=None, headers=None):
    client.request(method, name, body, headers or {})
    response = client.getresponse()
    return response.status, response.read(), response.getheader("X-Reply"), response.getheader("Retry-After")


def exercise(port):
    with closing(http.client.HTTPConnection("127.0.0.1", port, timeout=5)) as client:
        observed = [
            request(client, "GET", "/items/fixed"),
            request(client, "GET", "/items/one"),
            request(client, "GET", "/items/one/two"),
            request(client, "POST", "/echo?key=orbit", b"payload", {"X-Probe": "header"}),
            request(client, "GET", "/r69"),
            request(client, "GET", "/missing"),
        ]
    expected = [
        (200, b"fixed", None, None), (200, b"[one]", None, None),
        (200, b"multi", None, None), (201, b"[orbit:header:payload]", "seen", None),
        (200, b"route-69", None, None), (404, b"Not Found", None, None),
    ]
    if observed != expected:
        raise RuntimeError(f"unexpected replies: {observed!r}")

    def concurrent_client(index):
        with closing(http.client.HTTPConnection("127.0.0.1", port, timeout=5)) as client:
            for number in (index, index + 8):
                response = request(client, "GET", f"/items/{number}")
                if response[:2] != (200, f"[{number}]".encode()):
                    raise RuntimeError(f"wrong concurrent response: {response!r}")

    with ThreadPoolExecutor(max_workers=8) as clients:
        list(clients.map(concurrent_client, range(8)))

    with closing(http.client.HTTPConnection("127.0.0.1", port, timeout=5)) as client:
        first = request(client, "GET", "/burst")
        blocked = request(client, "GET", "/burst")
    if first[0] != 200 or blocked[0] != 429 or not blocked[3] or int(blocked[3]) < 1:
        raise RuntimeError(f"wrong admission responses: {first!r}, {blocked!r}")
    return observed


def verify_backend(work, backend, compiler, orbit_root, env):
    work.mkdir()
    (work / ".orbit").mkdir()
    source = work / "service.orb"
    exe = work / f"service{SUFFIX}"
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        declared = reservation.getsockname()[1]
        text = f"port {declared}\n\n" + SERVICE
        text += "".join(f'\nroute GET "/r{i}" {{\n    return ok 200 "route-{i}"\n}}\n'
                        for i in range(70))
        source.write_text(text, encoding="utf-8")
        subprocess.run([str(compiler), "build", str(source), f"--backend={backend}", "-o", str(exe)],
                       cwd=orbit_root, env=env, check=True, timeout=90)
    invalid = subprocess.run([str(exe), "not-a-port"], cwd=work, env=env,
                             capture_output=True, timeout=10)
    if invalid.returncode != 2:
        raise RuntimeError(f"{backend} accepted an invalid port: {invalid.stderr!r}")
    with running_service(exe, compiler, work, [], env) as port:
        if port != declared:
            raise RuntimeError(f"{backend} bound port {port}, declared {declared}")
        with closing(http.client.HTTPConnection("127.0.0.1", port, timeout=5)) as client:
            if request(client, "GET", "/items/fixed")[:2] != (200, b"fixed"):
                raise RuntimeError(f"{backend} did not answer on the declared port")
    with running_service(exe, compiler, work, ["0"], env) as port:
        observed = exercise(port)
    print(f"PASS {backend}: 75 routes, declared/overridden port, 24 replies, two graceful stops.")
    return observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--orbit-root", required=True, type=Path)
    parser.add_argument("--r6", type=Path, default=REPO / ".r6-build" / f"r6{SUFFIX}")
    parser.add_argument("--runtime-dir", type=Path, default=REPO / ".r6-build" /
                        ("runtime-win64" if os.name == "nt" else "runtime-linux-x64"))
    args = parser.parse_args()
    env = dict(os.environ, ORBIT_R6=str(args.r6.resolve()),
               R6_RUNTIME_DIR=str(args.runtime_dir.resolve()), ORBIT_WORKERS="2",
               ORBIT_ADMISSION="1", ORBIT_LOGS="0", NO_COLOR="1")
    with tempfile.TemporaryDirectory(prefix="r6-http-server-") as scratch:
        work = Path(scratch)
        oracle = verify_backend(work / "oracle", "c", args.compiler.resolve(), args.orbit_root, env)
        native = verify_backend(work / "native", "native", args.compiler.resolve(), args.orbit_root, env)
        if oracle != native:
            raise RuntimeError("C and R6 replies differ")


if __name__ == "__main__":
    main()
