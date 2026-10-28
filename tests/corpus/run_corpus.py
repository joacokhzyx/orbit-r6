#!/usr/bin/env python3
"""Differential over the language-emitted IR corpus -- a measurement, not a gate.

The pin lives in `tests/corpus/manifest.json`: the 93+1 IRv1 documents the
language emits for `tests/suite` (71 programs plus `imports.support.orb`) and
`tests/std` (22 programs), each with a sha256.  This runner
regenerates that corpus with the pinned compiler, checks every hash against
the pin, and then differentially compares R6 against the language for each
program: the oracle from the `.orb` source, R6 from the emitted `.ir`, both
run in the same directory, with process exit status and stdout bytes required
to match exactly (the same verdict rule as `tests/diff/run_diff.py`).

What R6 supports of the corpus right now is MEASURED, not asserted.  Nothing
here may go red because the native subset is smaller than the corpus: the
point is that the size of the gap is a number with a shape -- by diagnostic
code, by message and by opcode -- so closing it is scheduled work instead of
a guess.  Two things do fail: a corpus that no longer matches the pin (the
pin is broken, and silence would be worse), and `--check` once the number of
members R6 builds or matches goes DOWN (a one-way ratchet over the baseline
in `tests/corpus/baseline.json`, the same contract as the language's
unknown-type ratchet).

Usage:
    python tests/corpus/run_corpus.py --compiler FP --cc GCC \\
        --r6-command .r6-build/r6.exe build '{ir}' -o '{exe}'
    python tests/corpus/run_corpus.py ... --json report.json
    python tests/corpus/run_corpus.py ... --check
    python tests/corpus/run_corpus.py ... --check --write-baseline

`--compiler` is the fixed point of the pinned language commit (it needs
`--emit-ir`); `--orbit-root` is the language tree that carries
`scripts/emit_ir_corpus.py` (default: ORBIT_ROOT or the sibling `orbit-lang`).
`--ir-dir` runs the differential over an already-emitted corpus instead of
regenerating one, and the hashes are still checked against the pin.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
PINNED_MANIFEST = HERE / "manifest.json"
BASELINE = HERE / "baseline.json"
CORPUS_DIR = REPO / ".r6-build" / "corpus-ir"
DEFAULT_ORBIT = REPO.parent / "orbit-lang"
ORACLE_CACHE = REPO / ".r6-build" / "oracle-cache.json"

sys.path.insert(0, str(HERE.parent))
from oracle_cache import OracleCache, sha256_file  # noqa: E402

# One line, the loader/codegen diagnostic grammar from AGENTS.md.
DIAGNOSTIC = re.compile(r"^r6: .*?:(\d+):(\d+): error\[([A-Z_]+)\]: (.*)$")
OPCODE = re.compile(r'"opcode":"([A-Za-z0-9_]+)"')
REGISTER_TYPES = re.compile(r'"registerTypes":\[[^\]]*\]')
SUPPORT = "support"


def load_json(path: Path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def corpus_findings(pinned, fresh):
    """Every way the emitted corpus disagrees with the pin."""
    findings = []
    want = {entry["path"]: entry for entry in pinned.get("files", [])}
    got = {entry["path"]: entry for entry in fresh.get("files", [])}
    for path in sorted(set(want) | set(got)):
        if path not in got:
            findings.append(f"missing from the emission: {path}")
        elif path not in want:
            findings.append(f"not in the pin: {path}")
        elif want[path] != got[path]:
            findings.append(
                f"hash differs for {path}: pinned {want[path]['sha256']} "
                f"-> emitted {got[path]['sha256']}")
    if pinned.get("filesSha256") != fresh.get("filesSha256"):
        findings.append(
            f"filesSha256 differs: pinned {pinned.get('filesSha256')} "
            f"-> emitted {fresh.get('filesSha256')}")
    return findings


def regenerate(compiler: str, orbit_root: Path, out_dir: Path) -> int:
    """Emit the corpus with the language's own tool. Returns the exit code."""
    script = orbit_root / "scripts" / "emit_ir_corpus.py"
    if not script.is_file():
        print(f"{orbit_root} has no scripts/emit_ir_corpus.py: the pinned "
              f"language commit does not carry the emitter. Pass --orbit-root "
              f"with the tree that does, or --ir-dir with an emitted corpus.",
              file=sys.stderr)
        return 2
    proc = subprocess.run(
        [sys.executable, str(script), "--compiler", compiler,
         "--out", str(out_dir)],
        cwd=str(orbit_root), capture_output=True, text=True, timeout=900,
    )
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")
    return proc.returncode


def parse_diagnostic(stderr: str, stdout: str = ""):
    """(code, message, line, column) of the first r6 diagnostic, or Nones."""
    for text in (stderr, stdout):
        for line in (text or "").splitlines():
            match = DIAGNOSTIC.match(line.strip())
            if match:
                return (match.group(3), match.group(4),
                        int(match.group(1)), int(match.group(2)))
    return (None, None, None, None)


def failing_opcode(text: str, column):
    """The opcode whose instruction the diagnostic points at, or None.

    The corpus documents are single-line compact JSON, so a codegen
    diagnostic's `line 1, column N` lands on the failing instruction: its
    `"opcode"` key starts at or before N (the loader records the position
    inside the instruction object, one past the `{`), and no later
    instruction can have started yet.  Module-level refusals (register types,
    globals, models, unions) carry no position, and those keep the message as
    their class.
    """
    if not column or column <= 0:
        return None
    found = None
    for match in OPCODE.finditer(text):
        if match.start() > column:
            break
        found = match.group(1)
    return found


def run(argv, cwd, env, timeout):
    """(returncode, stdout bytes, stderr text, timed_out, error)."""
    try:
        proc = subprocess.run(argv, cwd=str(cwd), env=env,
                              capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return (None, b"", "", True, None)
    except OSError as exc:
        return (None, b"", "", False, str(exc))
    return (proc.returncode, proc.stdout,
            proc.stderr.decode("utf-8", errors="replace"), False, None)


def ratchet_findings(baseline, report):
    """Ways the baseline forbids this run. Pure, so it can be exercised."""
    findings = []
    if baseline.get("corpusFilesSha256") != report["corpusFilesSha256"]:
        findings.append(
            f"the baseline was taken against a different corpus "
            f"({baseline.get('corpusFilesSha256')} vs "
            f"{report['corpusFilesSha256']}); re-pin the corpus and take a new "
            f"baseline in the same act")
    for key in ("r6BuildOk", "match"):
        was = baseline.get("summary", {}).get(key)
        now = report["summary"][key]
        if was is not None and now < was:
            findings.append(f"{key} {was} -> {now}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compiler", required=True,
                        help="fixed point of the pinned language commit (needs --emit-ir)")
    parser.add_argument("--cc",
                        help="C compiler passed as ORBIT_CC/CC, as in test_suite.py")
    parser.add_argument("--r6-command", nargs=argparse.REMAINDER, required=True,
                        help="R6 argv template (place last); literal {ir} and {exe} tokens")
    parser.add_argument("--orbit-root", type=Path,
                        default=Path(os.environ.get("ORBIT_ROOT", DEFAULT_ORBIT)),
                        help="language tree carrying scripts/emit_ir_corpus.py")
    parser.add_argument("--ir-dir", type=Path,
                        help="already-emitted corpus to measure (default: regenerate)")
    parser.add_argument("--manifest", type=Path, default=PINNED_MANIFEST)
    parser.add_argument("--json", dest="json_out", type=Path,
                        help="write the full report (every case) here")
    parser.add_argument("--check", action="store_true",
                        help="also ratchet against tests/corpus/baseline.json")
    parser.add_argument("--baseline", type=Path, default=BASELINE)
    parser.add_argument("--write-baseline", action="store_true",
                        help="replace tests/corpus/baseline.json with this run")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--verbose", action="store_true",
                        help="print one line per case, not just the summary")
    parser.add_argument("--no-oracle-cache", action="store_true",
                        help=f"rebuild and rerun every oracle instead of reading {ORACLE_CACHE}")
    args = parser.parse_args()

    orbit_root = args.orbit_root.resolve()
    compiler = args.compiler
    if os.path.sep in compiler or (os.path.altsep and os.path.altsep in compiler):
        compiler = str(Path(compiler).resolve())
    if not Path(compiler).is_file():
        parser.error(f"no compiler at {compiler}")
    if not args.r6_command:
        parser.error("--r6-command must include an executable")
    if not args.manifest.is_file():
        parser.error(f"no pinned manifest at {args.manifest}")

    sys.path.insert(0, str(orbit_root / "scripts"))
    try:
        import _common  # type: ignore[import-not-found]
    except ImportError as exc:
        parser.error(f"cannot import scripts/_common.py under {orbit_root}: {exc}")

    pinned = load_json(args.manifest)

    if args.ir_dir:
        corpus_dir = args.ir_dir.resolve()
        if not (corpus_dir / "manifest.json").is_file():
            parser.error(f"{corpus_dir} has no manifest.json: not an emitted corpus")
    else:
        corpus_dir = CORPUS_DIR
        code = regenerate(compiler, orbit_root, corpus_dir)
        if code != 0:
            print(f"the emitter exited {code}; the measurement did not run",
                  file=sys.stderr)
            return 2
    fresh = load_json(corpus_dir / "manifest.json")

    findings = corpus_findings(pinned, fresh)
    for finding in findings:
        print(f"corpus drift: {finding}", file=sys.stderr)
    print(f"corpus: {fresh.get('counts', {}).get('total')} members, "
          f"filesSha256 {fresh.get('filesSha256')}")
    print(f"emitter: {fresh.get('emitter', {}).get('version')} "
          f"{fresh.get('emitter', {}).get('languageCommit')} "
          f"{fresh.get('emitter', {}).get('sha256')}")

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
    cache = OracleCache(ORACLE_CACHE, enabled=not args.no_oracle_cache)
    # The pinned manifest names every program by hash and the fixed point
    # builds the oracle, so the pair keys a result that a rebuild reproduces.
    cache_prefix = (f"corpus:{sha256_file(args.manifest)}:"
                    f"{sha256_file(Path(compiler))}:")

    cases = {}
    by_opcode = {}
    by_failing = {}
    reports = []
    for entry in fresh.get("files", []):
        rel = entry["path"]
        text = (corpus_dir / Path(entry["ir"])).read_text(encoding="utf-8")
        opcodes = set(OPCODE.findall(text))
        for opcode in opcodes:
            bucket = by_opcode.setdefault(opcode, {"cases": 0, "match": 0})
            bucket["cases"] += 1
        reports.append((entry, opcodes, text))

    work_root = REPO / ".r6-build" / "corpus-run"
    work_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="run_", dir=work_root) as temp_name:
        work = Path(temp_name)
        for entry, opcodes, text in reports:
            rel, kind, ir_rel = entry["path"], entry["kind"], entry["ir"]
            slug = rel.replace("/", "__")
            case_dir = work / slug
            case_dir.mkdir()
            case = {"kind": kind, "opcodes": sorted(opcodes),
                    "unknownRegisterTypes": any(
                        "unknown" in match.group(0)
                        for match in REGISTER_TYPES.finditer(text))}
            cases[rel] = case
            if kind == SUPPORT:
                # A support module is part of the corpus for the hash, not a
                # program: there is nothing to run on either side.
                case["verdict"] = SUPPORT
                continue
            case_env = dict(env, TEMP=str(case_dir), TMP=str(case_dir),
                            TMPDIR=str(case_dir))
            source = orbit_root / Path(rel)
            oracle_exe = case_dir / f"oracle{suffix}"
            r6_exe = case_dir / f"r6{suffix}"

            key = cache_prefix + rel
            cached = cache.get(key)
            if cached is None:
                code, _, stderr, timed_out, error = run(
                    [compiler, "build", str(source), "-o", str(oracle_exe)],
                    orbit_root, case_env, args.timeout)
                if code != 0 or not oracle_exe.is_file():
                    case["verdict"] = "oracle-timeout" if timed_out else "oracle-fail"
                    case["detail"] = (stderr or error or f"rc={code}").strip()[-400:]
                    continue
                code, oracle_out, oracle_err, timed_out, error = run(
                    [str(oracle_exe)], case_dir, case_env, args.timeout)
                if timed_out or error or code is None:
                    case["verdict"] = "oracle-timeout" if timed_out else "oracle-run-fail"
                    case["detail"] = (error or "").strip()
                    continue
                cache.put(key, code, oracle_out, oracle_err.encode("utf-8"))
            else:
                code, oracle_out, _ = cached
            oracle_exit = code

            command = [part.replace("{ir}", str(corpus_dir / Path(ir_rel)))
                             .replace("{exe}", str(r6_exe))
                       for part in args.r6_command]
            code, r6_out, r6_err, timed_out, error = run(
                command, REPO, case_env, args.timeout)
            if timed_out:
                case["verdict"] = "r6-build-timeout"
                continue
            if error or code is None:
                case["verdict"] = "r6-build-error"
                case["detail"] = (error or "").strip()
                continue
            if code != 0 or not r6_exe.is_file():
                diag_code, message, line, column = parse_diagnostic(
                    r6_err, r6_out.decode("utf-8", errors="replace"))
                case["verdict"] = "r6-build-fail"
                case["code"] = diag_code or f"rc={code}"
                case["message"] = message
                case["line"], case["column"] = line, column
                case["failingOpcode"] = failing_opcode(text, column) or "(module-level)"
                continue

            code, r6_run_out, r6_run_err, timed_out, error = run(
                [str(r6_exe)], case_dir, case_env, args.timeout)
            if timed_out or error or code is None:
                case["verdict"] = "r6-run-timeout" if timed_out else "r6-run-fail"
                case["detail"] = (error or "").strip()
                continue
            if code != oracle_exit or r6_run_out != oracle_out:
                case["verdict"] = "r6-differs"
                case["oracle_exit"], case["r6_exit"] = oracle_exit, code
                case["oracle_stdout"] = repr(oracle_out[:200])
                case["r6_stdout"] = repr(r6_run_out[:200])
                continue
            case["verdict"] = "match"
            case["exit"] = oracle_exit
            for opcode in opcodes:
                by_opcode[opcode]["match"] += 1

    cache.save()
    print(cache.summary())

    if args.verbose:
        for rel, case in cases.items():
            note = case.get("code") or ""
            if case.get("message"):
                note += f" {case['message']}"
            elif case.get("detail"):
                note += f" {case['detail'].splitlines()[0]}"
            elif case.get("exit") is not None:
                note += f" exit={case['exit']}"
            print(f"  {case['verdict']:16} {rel}{(' ' + note) if note else ''}")

    by_verdict = {}
    by_code = {}
    by_message = {}
    for rel, case in cases.items():
        verdict = case["verdict"]
        by_verdict[verdict] = by_verdict.get(verdict, 0) + 1
        if verdict == "r6-build-fail":
            by_code[case["code"]] = by_code.get(case["code"], 0) + 1
            message = case.get("message") or ""
            by_message[message] = by_message.get(message, 0) + 1
            failing = case.get("failingOpcode") or "(module-level)"
            by_failing[failing] = by_failing.get(failing, 0) + 1

    programs = len(cases) - by_verdict.get(SUPPORT, 0)
    oracle_failed = sum(by_verdict.get(name, 0) for name in
                        ("oracle-fail", "oracle-timeout", "oracle-run-fail"))
    report = {
        "schema": "r6-corpus-report/1",
        "corpusFilesSha256": fresh.get("filesSha256"),
        "emitter": fresh.get("emitter"),
        "summary": {
            "members": len(cases),
            "programs": programs,
            "oracleOk": programs - oracle_failed,
            "r6BuildOk": programs - oracle_failed - by_verdict.get("r6-build-fail", 0)
                - by_verdict.get("r6-build-timeout", 0)
                - by_verdict.get("r6-build-error", 0),
            "match": by_verdict.get("match", 0),
            "unknownRegisterTypes": sum(
                1 for case in cases.values()
                if case.get("unknownRegisterTypes")),
        },
        "byVerdict": dict(sorted(by_verdict.items(), key=lambda kv: -kv[1])),
        "byCode": dict(sorted(by_code.items(), key=lambda kv: -kv[1])),
        "byFailingOpcode": dict(sorted(by_failing.items(), key=lambda kv: -kv[1])),
        "byMessage": dict(sorted(by_message.items(), key=lambda kv: -kv[1])),
        "byOpcode": dict(sorted(by_opcode.items())),
        "cases": cases,
    }

    print(f"\ncorpus differential: {report['summary']['match']}/{programs} programs match "
          f"({len(cases)} members, {report['summary']['r6BuildOk']} build under R6)")
    print("  by verdict: " + ", ".join(
        f"{name} {count}" for name, count in report["byVerdict"].items()))
    print(f"  unknown in registerTypes: "
          f"{report['summary']['unknownRegisterTypes']}/{len(cases)} documents "
          f"(the emitter serializes before inference, P1/D-F0-08)")
    print("  by code:    " + ", ".join(
        f"{name} {count}" for name, count in report["byCode"].items()))
    print("  first refusal, by opcode:")
    for opcode, count in list(report["byFailingOpcode"].items())[:14]:
        print(f"    {count:4d}  {opcode}")
    print("  top messages:")
    for message, count in list(report["byMessage"].items())[:12]:
        print(f"    {count:4d}  {message}")
    unsupported = {op: data for op, data in report["byOpcode"].items()
                   if data["match"] == 0}
    print(f"  opcodes never in a matching program: "
          f"{len(unsupported)}/{len(report['byOpcode'])}")
    if unsupported:
        print("    " + " ".join(sorted(unsupported)))

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(report, indent=2) + "\n")
        print(f"report written to {args.json_out}")

    if findings:
        print("FAIL: the corpus does not match the pin in tests/corpus/manifest.json",
              file=sys.stderr)
        return 1

    if args.check:
        if not args.baseline.is_file():
            print(f"no baseline at {args.baseline}; the ratchet has nothing to hold",
                  file=sys.stderr)
            return 2
        baseline = load_json(args.baseline)
        broken = ratchet_findings(baseline, report)
        if broken:
            print("FAIL: the corpus ratchet is broken: " + "; ".join(broken),
                  file=sys.stderr)
            return 1
        print(f"ratchet holds: r6BuildOk {baseline['summary']['r6BuildOk']} -> "
              f"{report['summary']['r6BuildOk']}, match "
              f"{baseline['summary']['match']} -> {report['summary']['match']}")

    if args.write_baseline:
        # After --check, so promoting a baseline is "verify, then re-take":
        # the same run can hold the old ratchet and record the new one.
        baseline = {
            "schema": "r6-corpus-baseline/1",
            "corpusFilesSha256": report["corpusFilesSha256"],
            "summary": report["summary"],
            "byCode": report["byCode"],
            "byFailingOpcode": report["byFailingOpcode"],
            "byOpcode": report["byOpcode"],
        }
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        with open(args.baseline, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(baseline, indent=2) + "\n")
        print(f"baseline written to {args.baseline}: "
              f"r6BuildOk {report['summary']['r6BuildOk']}, "
              f"match {report['summary']['match']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
