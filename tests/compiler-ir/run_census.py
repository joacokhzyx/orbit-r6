#!/usr/bin/env python3
"""R6 against the IR the language emits for the compiler itself -- a measurement.

`tests/corpus` measures R6 on the 97 programs the language emits for its own
test suite.  It says nothing about the program the native backend exists for:
the compiler.  This runner emits the IR of every `compiler/**/*.orb` module
with the pinned compiler and records R6's first refusal for each, grouped by
diagnostic, so the size of what remains is a number with a shape instead of a
guess, and every round can report its effect on both targets.

The documents are a function of the language commit, so `--tree` must be a
checkout at the pinned commit, and the baseline carries that commit plus the
hash of the documents.  The emitter may be the patched fixed point: the
emission fix (orb-lang `33abf7a`) does not change a byte of the documents, and
`docs/M1_REPORT.md` records the equality.

Nothing here fails because R6 builds fewer modules than the compiler has: the
gap is the measurement.  Two things do fail: an emission that cannot run, and
`--check` once `r6BuildOk` goes down against `baseline.json` (one-way ratchet,
the same contract as `tests/corpus`).

Usage:
    python tests/compiler-ir/run_census.py --compiler FP \\
        --tree C:/Temp/orbit-pin --r6-command .r6-build/r6.exe build '{ir}' -o '{exe}'
    python tests/compiler-ir/run_census.py ... --json report.json
    python tests/compiler-ir/run_census.py ... --check
    python tests/compiler-ir/run_census.py --ir-dir DIR ... --check
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
BASELINE = HERE / "baseline.json"
DEFAULT_TREE = REPO.parent / "orbit-lang"
IR_DIR = REPO / ".r6-build" / "compiler-ir"

DIAGNOSTIC = re.compile(r"^r6: .*?:(\d+):(\d+): error\[([A-Z_]+)\]: (.*)$")
SCHEMA = 1


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canon(obj):
    """Canonical bytes for hashing: sorted keys, compact, ASCII-safe."""
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True).encode("ascii")


def files_hash(files):
    """The one hash that stands for the emitted set. Order is part of it."""
    return hashlib.sha256(canon(files)).hexdigest()


def discover(tree: Path):
    """Every compiler module, repo-relative and slash-separated, sorted."""
    root = tree / "compiler"
    if not root.is_dir():
        return []
    return sorted(path.relative_to(tree).as_posix()
                  for path in root.rglob("*.orb"))


def ir_name(rel: str) -> str:
    """compiler/front/lexer.orb -> compiler/front/lexer.ir."""
    return rel[:-len(".orb")] + ".ir"


def language_commit(tree: Path):
    try:
        proc = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(tree),
                              capture_output=True, text=True, timeout=60)
        return proc.stdout.strip() or None
    except Exception:
        return None


def emitter_fingerprint(compiler: str, tree: Path):
    try:
        version = subprocess.run([compiler, "--version"], capture_output=True,
                                 text=True, timeout=60).stdout.strip()
    except Exception:
        version = None
    return {
        "path": compiler,
        "sha256": sha256_file(Path(compiler)),
        "bytes": os.path.getsize(compiler),
        "version": version or None,
        "languageCommit": language_commit(tree),
    }


def emit(compiler: str, tree: Path, rel: str, out_dir: Path):
    """Emit one module; returns the .ir path or None on failure."""
    ir = out_dir / ir_name(rel)
    ir.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["ORBIT_SKIP_CC"] = "1"
    try:
        proc = subprocess.run([compiler, "build", rel, "--emit-ir", str(ir)],
                              cwd=str(tree), env=env, capture_output=True,
                              text=True, timeout=900)
    except subprocess.TimeoutExpired:
        sys.stderr.write(f"emission timed out: {rel}\n")
        return None
    if proc.returncode != 0 or not ir.is_file():
        sys.stderr.write(proc.stdout or "")
        sys.stderr.write(proc.stderr or "")
        return None
    return ir


def run_r6(template, ir: Path, exe: Path, timeout: int):
    """(exit, stderr+stdout text, error) for one r6 invocation."""
    argv = [part.replace("{ir}", str(ir)).replace("{exe}", str(exe))
            for part in template]
    try:
        proc = subprocess.run(argv, cwd=str(REPO), capture_output=True,
                              timeout=timeout)
    except subprocess.TimeoutExpired:
        return (None, "", "timed out")
    except OSError as exc:
        return (None, "", str(exc))
    text = (proc.stderr + b"\n" + proc.stdout).decode("utf-8", errors="replace")
    return (proc.returncode, text, None)


def first_diagnostic(text: str):
    """(code, message, line, column) of the first r6 diagnostic, or Nones."""
    for line in (text or "").splitlines():
        match = DIAGNOSTIC.match(line.strip())
        if match:
            return (match.group(3), match.group(4),
                    int(match.group(1)), int(match.group(2)))
    return (None, None, None, None)


def refusal_of(exit_code, text):
    """The first refusal as a stable group key."""
    code, message, _, _ = first_diagnostic(text)
    if code:
        return f"error[{code}]: {message}"
    if exit_code is None:
        return "r6 did not run"
    if exit_code == 0:
        return "builds"
    head = (text or "").strip().splitlines()
    return f"exit {exit_code}: {head[0] if head else 'no output'}"


def measure(compiler: str, tree: Path, out_dir: Path, template, timeout: int):
    """Emit every module and ask R6 about it; returns (files, cases, failures)."""
    files, cases, failures = [], [], []
    for rel in discover(tree):
        ir = emit(compiler, tree, rel, out_dir)
        if ir is None:
            failures.append(rel)
            continue
        files.append({
            "path": rel,
            "kind": "module",
            "ir": ir_name(rel),
            "bytes": os.path.getsize(ir),
            "sha256": sha256_file(ir),
        })
        exe = out_dir / "exe" / (ir_name(rel)[:-len(".ir")] + ".exe")
        exe.parent.mkdir(parents=True, exist_ok=True)
        exit_code, text, error = run_r6(template, ir, exe, timeout)
        code, message, line, column = first_diagnostic(text)
        cases.append({
            "module": rel,
            "bytes": os.path.getsize(ir),
            "exit": exit_code,
            "error": error,
            "code": code,
            "message": message,
            "line": line,
            "column": column,
            "refusal": refusal_of(exit_code, text),
            "built": exit_code == 0,
        })
    return files, cases, failures


def groups(cases):
    """{refusal: [modules]} over the cases that did not build."""
    out = {}
    for case in cases:
        if case["built"]:
            continue
        out.setdefault(case["refusal"], []).append(case["module"])
    return dict(sorted(out.items(), key=lambda item: (-len(item[1]), item[0])))


def report_for(files, cases, failures, fingerprint, commit, total_modules):
    return {
        "schema": SCHEMA,
        "emitter": fingerprint,
        "languageCommit": commit,
        "counts": {
            "modules": total_modules,
            "emitted": len(files),
            "r6BuildOk": sum(1 for case in cases if case["built"]),
            "emitFailed": len(failures),
        },
        "totalBytes": sum(entry["bytes"] for entry in files),
        "irFilesSha256": files_hash(files),
        "files": files,
        "failures": failures,
        "cases": cases,
        "refusals": groups(cases),
    }


def baseline_for(report):
    return {
        "schema": SCHEMA,
        "languageCommit": report["languageCommit"],
        "irFilesSha256": report["irFilesSha256"],
        "summary": {
            "modules": report["counts"]["modules"],
            "emitted": report["counts"]["emitted"],
            "r6BuildOk": report["counts"]["r6BuildOk"],
        },
        "refusals": {key: len(value)
                     for key, value in report["refusals"].items()},
    }


def ratchet_findings(baseline, report):
    """Ways the baseline forbids this run. Pure, so it can be exercised."""
    findings = []
    if baseline.get("irFilesSha256") != report["irFilesSha256"]:
        findings.append(
            f"the baseline was taken against different documents "
            f"({baseline.get('irFilesSha256')} vs {report['irFilesSha256']}); "
            f"re-pin the language and take a new baseline in the same act")
    was = baseline.get("summary", {}).get("r6BuildOk")
    now = report["counts"]["r6BuildOk"]
    if was is not None and now < was:
        findings.append(f"r6BuildOk {was} -> {now}")
    return findings


def write_json(path: Path, doc):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(doc, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compiler", required=True,
                        help="fixed point of the pinned commit (needs --emit-ir)")
    parser.add_argument("--tree", type=Path,
                        default=Path(os.environ.get("ORBIT_ROOT", DEFAULT_TREE)),
                        help="language checkout at the pinned commit (cwd of the emission)")
    parser.add_argument("--r6-command", nargs=argparse.REMAINDER, required=True,
                        help="R6 argv template (place last); {ir} and {exe} tokens")
    parser.add_argument("--ir-dir", type=Path, default=IR_DIR,
                        help="where the emitted documents live (reused with --measure-only)")
    parser.add_argument("--measure-only", action="store_true",
                        help="do not emit; measure the documents already in --ir-dir")
    parser.add_argument("--json", dest="json_out", type=Path,
                        help="write the full report here")
    parser.add_argument("--check", action="store_true",
                        help="ratchet against tests/compiler-ir/baseline.json")
    parser.add_argument("--baseline", type=Path, default=BASELINE)
    parser.add_argument("--write-baseline", action="store_true",
                        help="replace the baseline with this run")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--verbose", action="store_true",
                        help="print one line per module, not just the groups")
    args = parser.parse_args()

    tree = args.tree.resolve()
    compiler = str(Path(args.compiler).resolve())
    if not Path(compiler).is_file():
        parser.error(f"no compiler at {compiler}")
    if not (tree / "compiler").is_dir():
        parser.error(f"{tree} has no compiler/ directory")
    if not args.r6_command:
        parser.error("--r6-command must include an executable")

    modules = discover(tree)
    if not modules:
        parser.error(f"{tree} has no compiler/**/*.orb modules")

    out_dir = args.ir_dir.resolve()
    if args.measure_only:
        if not out_dir.is_dir():
            parser.error(f"{out_dir} does not exist; drop --measure-only")
        files = []
        for rel in modules:
            ir = out_dir / ir_name(rel)
            if not ir.is_file():
                parser.error(f"{out_dir} is missing {ir_name(rel)}")
            files.append({"path": rel, "kind": "module", "ir": ir_name(rel),
                          "bytes": os.path.getsize(ir),
                          "sha256": sha256_file(ir)})
        cases, failures = [], []
        for rel in modules:
            ir = out_dir / ir_name(rel)
            exe = out_dir / "exe" / (ir_name(rel)[:-len(".ir")] + ".exe")
            exe.parent.mkdir(parents=True, exist_ok=True)
            exit_code, text, error = run_r6(args.r6_command, ir, exe, args.timeout)
            code, message, line, column = first_diagnostic(text)
            cases.append({"module": rel, "bytes": os.path.getsize(ir),
                          "exit": exit_code, "error": error, "code": code,
                          "message": message, "line": line, "column": column,
                          "refusal": refusal_of(exit_code, text),
                          "built": exit_code == 0})
    else:
        files, cases, failures = measure(compiler, tree, out_dir,
                                         args.r6_command, args.timeout)

    report = report_for(files, cases, failures, emitter_fingerprint(compiler, tree),
                        language_commit(tree), len(modules))

    if args.verbose:
        for case in cases:
            print(f"  {case['module']:<34} {case['bytes']:>9}  {case['refusal']}")

    print(f"modules {report['counts']['modules']}, "
          f"emitted {report['counts']['emitted']}, "
          f"r6 builds {report['counts']['r6BuildOk']}, "
          f"emission failed {report['counts']['emitFailed']}")
    print(f"documents {report['totalBytes']} bytes, "
          f"irFilesSha256 {report['irFilesSha256']}")
    for refusal, members in report["refusals"].items():
        print(f"  {len(members):>2}  {refusal}")
        print(f"      {', '.join(members)}")
    for rel in report["failures"]:
        print(f"  emission failed: {rel}")

    if args.json_out:
        write_json(args.json_out, report)
        print(f"report {args.json_out}")

    if args.check:
        if not args.baseline.is_file():
            print(f"no baseline at {args.baseline}; run with --write-baseline",
                  file=sys.stderr)
            return 1
        with open(args.baseline, "r", encoding="utf-8") as handle:
            baseline = json.load(handle)
        findings = ratchet_findings(baseline, report)
        if findings:
            for finding in findings:
                print(f"RATCHET {finding}", file=sys.stderr)
            return 1
        if args.write_baseline:
            write_json(args.baseline, baseline_for(report))
            print(f"baseline {args.baseline} re-taken")
        print("ratchet holds")
        return 0

    if args.write_baseline:
        write_json(args.baseline, baseline_for(report))
        print(f"baseline {args.baseline} written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
