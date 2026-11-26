"""Measure HTTP/1.1 services on one Linux VM using only loopback."""

import argparse
import asyncio
import json
import math
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import time


PAYLOAD = b"x" * 4096
SCENARIOS = {
    "fixed": (b"GET /fixed", b"", b"ok"),
    "param": (b"GET /item/probe", b"", b"probe"),
    "post": (b"POST /body", PAYLOAD, b"ok"),
}


def pss_kib(pid):
    with open(f"/proc/{pid}/smaps_rollup", encoding="ascii") as rollup:
        for line in rollup:
            if line.startswith("Pss:"):
                return int(line.split()[1])
    raise RuntimeError(f"no Pss for PID {pid}")


def percentile(values, fraction):
    if not values:
        return None
    return sorted(values)[math.ceil(len(values) * fraction) - 1]


def ready(proc, log):
    deadline = time.monotonic() + 10
    while proc.poll() is None and time.monotonic() < deadline:
        banner = re.search(rb"localhost:(\d+)", log.read_bytes())
        if banner:
            return int(banner[1])
        time.sleep(0.02)
    raise RuntimeError(f"server did not start: {log.read_text(errors='replace')}")


async def exchange(reader, writer, request, want):
    writer.write(request)
    await writer.drain()
    head = await reader.readuntil(b"\r\n\r\n")
    lines = head[:-4].split(b"\r\n")
    if int(lines[0].split()[1]) != 200:
        raise ValueError(lines[0])
    headers = dict(line.lower().split(b":", 1) for line in lines[1:])
    body = await reader.readexactly(int(headers[b"content-length"]))
    if body != want:
        raise ValueError(f"body mismatch: {body!r}")


async def load(proc, port, scenario, count, rate, duration, timeout):
    loop = asyncio.get_running_loop()
    clients = []
    attempted, completed, errors = 0, 0, 0
    latencies = []
    samples = []
    base = pss_kib(proc.pid)
    method, body, want = SCENARIOS[scenario]
    request = (method + b" HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n"
               + (f"Content-Length: {len(body)}\r\n".encode() if body else b"")
               + b"\r\n" + body)

    async def drive(index, reader, writer, start):
        nonlocal attempted, completed, errors
        next_send = start + (index / rate if rate else 0)
        end = start + duration
        while next_send < end and loop.time() < end:
            if next_send > loop.time():
                await asyncio.sleep(next_send - loop.time())
            now = loop.time()
            if now >= end:
                break
            attempted += 1
            last = next_send if rate else now
            try:
                await asyncio.wait_for(exchange(reader, writer, request, want), timeout)
            except (OSError, asyncio.TimeoutError, asyncio.IncompleteReadError, ValueError, KeyError):
                errors += 1
                break
            finish = loop.time()
            if finish < end:
                completed += 1
                latencies.append((finish - last) * 1000)
            next_send = next_send + count / rate if rate else finish

    async def sample(start):
        await asyncio.sleep(max(0, start - loop.time()))
        end = start + duration
        while loop.time() < end:
            samples.append(pss_kib(proc.pid))
            await asyncio.sleep(0.2)

    try:
        for _ in range(count):
            clients.append(await asyncio.wait_for(asyncio.open_connection("127.0.0.1", port), timeout))
        # Quiescent measurement with the same live sockets used for the workload.
        await asyncio.sleep(0.4)
        idle = pss_kib(proc.pid)
        for reader, writer in clients:
            await asyncio.wait_for(exchange(reader, writer, request, want), timeout)
        start = loop.time() + 0.1
        await asyncio.gather(sample(start), *(drive(i, *client, start)
                                             for i, client in enumerate(clients)))
        return {
            "pss_start_kib": base,
            "pss_idle_kib": idle,
            "idle_increment_bytes_per_connection": (idle - base) * 1024 / count,
            "pss_loaded_kib": percentile(samples, 0.5),
            "pss_loaded_samples_kib": samples,
            "loaded_increment_bytes_per_connection": (percentile(samples, 0.5) - base) * 1024 / count,
            "attempted": attempted,
            "completed": completed,
            "errors": errors,
            "rps": completed / duration,
            "p99_ms": percentile(latencies, 0.99),
        }
    finally:
        for _, writer in clients:
            writer.close()
        await asyncio.gather(*(writer.wait_closed() for _, writer in clients),
                             return_exceptions=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", nargs=2, action="append", metavar=("NAME", "EXECUTABLE"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--connections", type=int, default=128)
    parser.add_argument("--seconds", type=float, default=2.5)
    parser.add_argument("--rate", type=int, default=2048, help="offered requests/s, or 0 for saturation")
    parser.add_argument("--reps", type=int, default=5)
    args = parser.parse_args()
    cores = sorted(os.sched_getaffinity(0))
    if platform.machine() != "x86_64" or len(cores) < 4:
        parser.error("require Linux x64 with four available logical CPUs")
    if (args.rate < 0 or args.connections < 1 or args.connections > 256
            or args.seconds < 1 or args.reps < 1):
        parser.error("require 1..256 connections, >=1 second and >=1 repetition")
    server_cores, client_cores = cores[:2], cores[2:]
    os.sched_setaffinity(0, client_cores)
    environment = dict(os.environ, ORBIT_WORKERS="2", ORBIT_ADMISSION="0",
                       ORBIT_LOGS="0", ORBIT_HTTP_MAX_BODY="12000",
                       ORBIT_HTTP_MAX_HEADER_BYTES="12000", GOMAXPROCS="2")
    settings = {
        "kernel": platform.release(),
        "machine": platform.machine(),
        "server_cpus": server_cores,
        "client_cpus": client_cores,
        "connections": args.connections,
        "offered_rps": args.rate,
        "seconds": args.seconds,
        "reps": args.reps,
    }
    args.metadata.write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
    cwd = Path.cwd()
    with args.output.open("w", encoding="utf-8") as results:
        for index in range(args.reps):
            # Rotate variants so every implementation occupies each position.
            servers = args.server[index % len(args.server):] + args.server[:index % len(args.server)]
            for name, path in servers:
                for scenario in SCENARIOS:
                    with tempfile.TemporaryDirectory(prefix="r6-http-perf-") as directory:
                        root = Path(directory)
                        (root / ".orbit").mkdir()
                        log = root / "service.log"
                        command = ["taskset", "-c", ",".join(map(str, server_cores)),
                                   str(cwd / path), "0"]
                        with log.open("wb", buffering=0) as output:
                            proc = subprocess.Popen(command, cwd=root, env=environment,
                                                    stdout=output, stderr=subprocess.STDOUT)
                            try:
                                port = ready(proc, log)
                                time.sleep(0.15)
                                record = asyncio.run(load(proc, port, scenario, args.connections,
                                                          args.rate, args.seconds, timeout=1))
                            finally:
                                if proc.poll() is None:
                                    proc.terminate()
                                    try:
                                        proc.wait(5)
                                    except subprocess.TimeoutExpired:
                                        proc.kill()
                                        proc.wait()
                    record.update(variant=name, scenario=scenario, repetition=index + 1)
                    line = json.dumps(record, sort_keys=True)
                    print(line, flush=True)
                    results.write(line + "\n")
                    results.flush()


if __name__ == "__main__":
    main()
