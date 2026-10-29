#!/usr/bin/env python3
"""End-to-end check of a runtime module (D-M1-105, D-M1-106).

1. r6 build --object compiles probe-rt.json, a runtime module, to an object.
2. nm must list the two exports as global text symbols and the object must
   define no main, orbit_main or orbit_start.
3. Without the object in the archive, probe-main.json fails to link.
4. The object is added to a temporary copy of libruntime_net.a with ar, as a
   member of the runtime archive, and probe-main.json links and runs.

Usage: python tests/runtime-module/run_runtime_module.py [--r6 .r6-build/r6.exe]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
HERE = Path(__file__).resolve().parent

EXPECTED_STDOUT = (b"HELLO, ORBIT 42!\r\n\r\n468\r\nhello, orbit 42!\r\ncopy me\r\nOrbit\r\nx\r\n"
                   b"2\r\nnegative\r\n1\r\n-2.5\r\n4609434218613702656\r\n-9223372036854775808\r\n")
EXPORTS = ["rtmod_probe_bits", "rtmod_probe_cell", "rtmod_probe_float", "rtmod_probe_guard",
           "rtmod_probe_lower", "rtmod_probe_slice", "rtmod_probe_sum", "rtmod_probe_upper"]


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True,
                          errors="replace", cwd=str(REPO), **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--r6", default=str(REPO / ".r6-build" / "r6.exe"))
    ap.add_argument("--runtime-dir", default=str(REPO / ".r6-build" / "runtime-win64"))
    args = ap.parse_args()

    mingw = Path(os.environ.get("R6_MINGW_ROOT", "C:/w64devkit")) / "bin"
    nm, ar = mingw / "nm.exe", mingw / "ar.exe"
    out = REPO / ".r6-build" / "runtime-module"
    if out.exists():
        shutil.rmtree(out)
    runtime = out / "runtime"
    runtime.mkdir(parents=True)
    failures = []

    def check(name, ok, detail=""):
        print(("PASS " if ok else "FAIL ") + name + (" " + detail if detail else ""))
        if not ok:
            failures.append(name)

    obj = out / "probe-rt.o"
    p = run([args.r6, "build", "--object", HERE / "probe-rt.json", "-o", obj])
    check("object builds", p.returncode == 0 and obj.exists(), p.stderr.strip())
    if failures:
        return 1

    p = run([nm, obj])
    symbols = {}
    for line in p.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = parts[1]
    check("exports are global text symbols",
          all(symbols.get(n) == "T" for n in EXPORTS), str(sorted(symbols.items())))
    undefined = {parts[1] for parts in (ln.split() for ln in p.stdout.splitlines())
                 if len(parts) == 2 and parts[0] == "U"}
    check("allocates through orbit_alloc and the global-arena fallback",
          {"orbit_alloc", "orbit_arena_get_global"} <= undefined, str(sorted(undefined)))
    # D-M1-112, D-M1-117: results are built in place, with no call into types.c.
    check("builds results without orbit_result_ok or orbit_result_err",
          not any(n.startswith("orbit_result_") for n in undefined), str(sorted(undefined)))
    check("no entry symbol",
          not any(n in symbols for n in ("main", "orbit_main", "orbit_start")))

    # D-M1-118: an ordinary fixture that builds ok, err and a float unwrap,
    # recompiled as a runtime module, must not reference any orbit_result_*.
    doc = json.loads((REPO / "docs" / "proposed-ir" / "m1" / "22-result-ops.json").read_text())
    doc["module"]["runtimeModule"] = True
    for fn in doc["module"]["functions"]:
        if fn["name"] == "main":
            fn["name"] = "rtmod_result_ops"
        if not fn["isExtern"]:
            fn["export"] = True
    ops_json = out / "result-ops-rt.json"
    ops_json.write_bytes((json.dumps(doc) + "\n").encode())
    ops_obj = out / "result-ops-rt.o"
    p = run([args.r6, "build", "--object", ops_json, "-o", ops_obj])
    check("result-ops fixture builds as a runtime module", p.returncode == 0, p.stderr.strip())
    if p.returncode == 0:
        p = run([nm, ops_obj])
        ops_undefined = {ln.split()[1] for ln in p.stdout.splitlines()
                         if len(ln.split()) == 2 and ln.split()[0] == "U"}
        check("ok, err and a float unwrap reference no orbit_result_*",
              not any(n.startswith("orbit_result_") for n in ops_undefined),
              str(sorted(ops_undefined)))

    # The IR the language emits for tests/runtime_module in orbit-lang
    # (REBUILD.txt): parameters travel through load_var, as for any program.
    emitted = {}
    for stem in ("arena", "upper"):
        eo = out / ("emitted-" + stem + ".o")
        p = run([args.r6, "build", "--object", HERE / "emitted" / (stem + ".json"), "-o", eo])
        check("emitted " + stem + ".json builds", p.returncode == 0, p.stderr.strip())
        emitted[stem] = eo
    if failures:
        return 1
    obj_arena = emitted["arena"]

    for name in ("libruntime_net.a",):
        shutil.copy(Path(args.runtime_dir) / name, runtime / name)

    exe = out / "probe-main.exe"
    p = run([args.r6, "build", HERE / "probe-main.json", "-o", exe,
             "--runtime-dir", runtime])
    check("link without the member is refused",
          p.returncode != 0 and "error[LINK]" in p.stderr, p.stderr.strip()[:120])

    p = run([ar, "r", runtime / "libruntime_net.a", obj, obj_arena])
    check("object added to the archive", p.returncode == 0, p.stderr.strip())
    p = run([args.r6, "build", HERE / "probe-main.json", "-o", exe,
             "--runtime-dir", runtime])
    check("program links with the member", p.returncode == 0, p.stderr.strip())
    if p.returncode == 0:
        r = subprocess.run([str(exe)], capture_output=True)
        check("program output", r.returncode == 0 and r.stdout == EXPECTED_STDOUT,
              f"exit={r.returncode} stdout={r.stdout!r}")

    print(f"Runtime module: {'FAILED' if failures else 'all checks passed'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
