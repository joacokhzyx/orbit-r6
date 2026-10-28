#!/usr/bin/env python3
"""Differentially compare Orbit's gcc oracle with an R6 driver.

Build convention mirrors the pinned scripts/test_suite.py: ``orbit build
SOURCE -o EXE``. R6's driver command is configurable and receives {ir} and
{exe} substitutions. Both generated programs run in the same working
directory; process exit status and stdout bytes must match exactly.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
FIXTURES = REPO / "docs" / "proposed-ir" / "m1"


def show_bytes(data: bytes) -> str:
    return repr(data)


def first_difference(left: bytes, right: bytes) -> str:
    limit = min(len(left), len(right))
    for index in range(limit):
        if left[index] != right[index]:
            return f"first differing byte at offset {index}: gcc=0x{left[index]:02x}, r6=0x{right[index]:02x}"
    return f"common prefix is {limit} bytes; lengths gcc={len(left)}, r6={len(right)}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True,
                        help="pinned Orbit compiler executable (same role as test_suite.py --compiler)")
    parser.add_argument("--cc", help="C compiler passed as ORBIT_CC/CC, as in test_suite.py")
    parser.add_argument("--r6-command", nargs=argparse.REMAINDER, required=True,
                        help="R6 argv template (place last); use literal {ir} and {exe} tokens")
    parser.add_argument("--orbit-root", type=Path,
                        default=Path(os.environ.get("ORBIT_ROOT", REPO.parent / "orbit-lang")),
                        help="pinned Orbit checkout (default: ORBIT_ROOT or sibling orbit-lang)")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--case", action="append", dest="cases",
                        help="fixture basename without extension; repeatable, default: all five")
    args = parser.parse_args()

    orbit_root = args.orbit_root.resolve()
    compiler = str(Path(args.compiler).resolve()) if os.path.sep in args.compiler else args.compiler
    suite_scripts = orbit_root / "scripts"
    sys.path.insert(0, str(suite_scripts))
    try:
        import _common  # type: ignore[import-not-found]
    except ImportError as exc:
        parser.error(f"cannot import pinned scripts/_common.py under {orbit_root}: {exc}")

    names = args.cases or sorted(path.stem for path in FIXTURES.glob("*.json"))
    if not names:
        parser.error(f"no JSON fixtures found in {FIXTURES}")
    if not args.r6_command:
        parser.error("--r6-command must include an executable")
    for name in names:
        if Path(name).name != name or not (FIXTURES / f"{name}.json").is_file() or not (FIXTURES / f"{name}.orb").is_file():
            parser.error(f"expected matching fixture pair {name}.json and {name}.orb in {FIXTURES}")

    env = dict(os.environ)
    if args.cc:
        env.update({"ORBIT_CC": args.cc, "CC": args.cc, "R6_CC": args.cc})
    else:
        noop = "cmd /c exit 0" if os.name == "nt" else "true"
        env.update({"ORBIT_CC": noop, "CC": noop})
    env = _common.disposable_test_env(env, cc=args.cc)
    vendor = orbit_root / "runtime" / "vendor" / "win-x64"
    if vendor.is_dir():
        env["PATH"] = str(vendor) + os.pathsep + env.get("PATH", "")
    suffix = ".exe" if os.name == "nt" else ""
    failures = 0

    build_root = REPO / ".r6-build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="orbit_r6_diff_", dir=build_root) as temp_name:
        work = Path(temp_name)
        for name in names:
            case_dir = work / name
            case_dir.mkdir()
            oracle_exe = case_dir / f"oracle{suffix}"
            r6_exe = case_dir / f"r6{suffix}"
            source = FIXTURES / f"{name}.orb"
            ir = FIXTURES / f"{name}.json"
            case_env = dict(env, TEMP=str(case_dir), TMP=str(case_dir), TMPDIR=str(case_dir))

            try:
                build = subprocess.run(
                    [compiler, "build", str(source), "-o", str(oracle_exe)],
                    cwd=orbit_root, env=case_env, capture_output=True,
                    timeout=args.timeout,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                print(f"FAIL {name}: gcc oracle build failed: {exc}", file=sys.stderr)
                failures += 1
                continue
            if build.returncode != 0:
                detail = (build.stderr or build.stdout).decode("utf-8", errors="replace")
                print(f"FAIL {name}: gcc oracle did not build (rc={build.returncode})\n{detail}", file=sys.stderr)
                failures += 1
                continue

            command = [part.replace("{ir}", str(ir)).replace("{exe}", str(r6_exe))
                       for part in args.r6_command]
            try:
                build_r6 = subprocess.run(command, cwd=REPO, env=case_env,
                                          capture_output=True, timeout=args.timeout)
            except (OSError, subprocess.TimeoutExpired) as exc:
                print(f"FAIL {name}: R6 build failed: {exc}\ncommand={command!r}", file=sys.stderr)
                failures += 1
                continue
            if build_r6.returncode != 0 or not r6_exe.is_file():
                detail = (build_r6.stderr or build_r6.stdout).decode("utf-8", errors="replace")
                print(f"FAIL {name}: R6 did not produce executable (rc={build_r6.returncode})\n{detail}", file=sys.stderr)
                failures += 1
                continue

            try:
                oracle = subprocess.run([str(oracle_exe)], cwd=case_dir, env=case_env,
                                        capture_output=True, timeout=args.timeout)
                native = subprocess.run([str(r6_exe)], cwd=case_dir, env=case_env,
                                        capture_output=True, timeout=args.timeout)
            except (OSError, subprocess.TimeoutExpired) as exc:
                print(f"FAIL {name}: execution failed or timed out: {exc}", file=sys.stderr)
                failures += 1
                continue

            if oracle.returncode != native.returncode or oracle.stdout != native.stdout:
                print(f"FAIL {name}: gcc exit={oracle.returncode}, r6 exit={native.returncode}", file=sys.stderr)
                if oracle.stdout != native.stdout:
                    print(f"  stdout gcc={show_bytes(oracle.stdout)}", file=sys.stderr)
                    print(f"  stdout r6 ={show_bytes(native.stdout)}", file=sys.stderr)
                    print(f"  {first_difference(oracle.stdout, native.stdout)}", file=sys.stderr)
                if oracle.stderr != native.stderr:
                    print(f"  stderr gcc={show_bytes(oracle.stderr)}", file=sys.stderr)
                    print(f"  stderr r6 ={show_bytes(native.stderr)}", file=sys.stderr)
                failures += 1
                continue
            print(f"PASS {name}: exit={oracle.returncode}, stdout={show_bytes(oracle.stdout)}")

    print(f"Differential: {len(names) - failures}/{len(names)} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
