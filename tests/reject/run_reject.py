#!/usr/bin/env python3
"""Second gate: loader contract acceptance/rejection suite (fase B).

Consumes tests/reject/manifest.json (rendered together with cases/ by
make_cases.py). Every rejection case must fail with exit 1, exactly one
single-line diagnostic `r6: <file>:<line>:<col>: error[CODE]: <msg>` and the
expected diagnostic CODE. Acceptance cases must build; when the manifest
declares run_exit (and optionally run_stdout) the binary is also executed and
compared byte-for-byte.

The driver runs with the repository root as CWD because R6 resolves its
temporary files and the runtime archive relative to the current directory.

Usage (from anywhere):
    python tests/reject/run_reject.py [--r6 .r6-build/r6.exe] [--case STEM]...
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
HERE = Path(__file__).resolve().parent

DIAG = re.compile(r"^r6: .+:\d+:\d+: error\[([A-Z][A-Z0-9_]*)\]: .+$")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--r6", default=str(REPO / ".r6-build" / "r6.exe"),
                    help="driver binary (default: .r6-build/r6.exe)")
    ap.add_argument("--case", action="append", default=[],
                    help="run only cases whose stem contains this text (repeatable)")
    args = ap.parse_args()

    r6 = Path(args.r6)
    if not r6.exists():
        print(f"driver not found: {r6} (build it with scripts/build-r6.ps1)",
              file=sys.stderr)
        return 2
    manifest = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
    outdir = REPO / ".r6-build" / "reject"
    outdir.mkdir(parents=True, exist_ok=True)

    def selected(file: str) -> bool:
        return not args.case or any(k in Path(file).stem for k in args.case)

    def build(path: Path, exe: Path):
        return subprocess.run([str(r6), "build", str(path), "-o", str(exe)],
                              capture_output=True, text=True, errors="replace",
                              cwd=str(REPO))

    passed = failed = 0
    for case in manifest["reject"]:
        if not selected(case["file"]):
            continue
        path = HERE / case["file"]
        exe = outdir / (path.stem + ".exe")
        proc = build(path, exe)
        lines = [ln for ln in proc.stderr.splitlines() if ln.strip()]
        diag = DIAG.match(lines[0]) if len(lines) == 1 else None
        ok = (proc.returncode == case["exit"] and diag is not None
              and diag.group(1) == case["code"])
        if ok:
            passed += 1
            print(f"PASS reject {case['rule']:>12} {path.stem} "
                  f"exit={proc.returncode} code={case['code']}")
        else:
            failed += 1
            got = diag.group(1) if diag else f"<{len(lines)} stderr line(s)>"
            print(f"FAIL reject {case['rule']:>12} {path.stem} "
                  f"exit={proc.returncode} want={case['exit']} "
                  f"code={got} want={case['code']}")
            if proc.stderr.strip():
                print(f"     stderr: {proc.stderr.strip()}")

    for case in manifest["accept"]:
        if not selected(case["file"]):
            continue
        path = (HERE / case["file"]).resolve()
        exe = outdir / (path.stem + ".exe")
        proc = build(path, exe)
        ok = proc.returncode == 0
        detail = "build"
        if ok and "run_exit" in case:
            run = subprocess.run([str(exe)], capture_output=True)
            want_stdout = case.get("run_stdout", "").encode("utf-8")
            ok = run.returncode == case["run_exit"] and run.stdout == want_stdout
            detail = f"run exit={run.returncode} stdout={run.stdout!r}"
        if ok:
            passed += 1
            print(f"PASS accept {case['rule']:>12} {path.stem} {detail}")
        else:
            failed += 1
            print(f"FAIL accept {case['rule']:>12} {path.stem} "
                  f"build exit={proc.returncode}")
            if proc.stderr.strip():
                print(f"     stderr: {proc.stderr.strip()}")

    total = passed + failed
    print(f"Loader suite: {passed}/{total} cases passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
