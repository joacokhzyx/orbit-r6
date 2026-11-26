"""Check that the optional per-function cache preserves linked bytes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import subprocess
import tempfile


PROGRAM = """fn edited(x: int) -> int {
    return x + 2
}

fn unchanged(x: int) -> int {
    return x * 3
}

fn main() -> int {
    print(unchanged(edited(4)))
    return 0
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("compiler", "orbit-root", "r6", "runtime-dir"):
        parser.add_argument("--" + flag, type=Path, required=True)
    args = parser.parse_args()
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="r6-cache-gate-") as temp:
        work = Path(temp)
        cache = work / "cache"
        disabled = dict(os.environ, R6_FUNCTION_CACHE="", R6_TIMINGS="1")
        enabled = dict(disabled, R6_FUNCTION_CACHE=str(cache))

        def emit(name, program):
            source, ir = work / f"{name}.orb", work / f"{name}.ir"
            source.write_text(program, encoding="utf-8")
            subprocess.run([str(args.compiler.resolve()), "build", str(source), "--backend=c",
                            "--emit-ir", str(ir), "-o", str(work / (name + suffix))],
                           cwd=args.orbit_root, check=True, timeout=90)
            return ir

        def build(ir, label, env, expected_stats=None):
            out = work / f"{label}.out{suffix}"
            compiled = subprocess.run([str(args.r6.resolve()), "build", str(ir), "-o", str(out),
                                       "--runtime-dir", str(args.runtime_dir.resolve())],
                                      env=env, cwd=work, capture_output=True, text=True, timeout=30)
            if compiled.returncode:
                raise RuntimeError(compiled.stderr)
            stats = re.search(r"function cache (\d+) hits (\d+) misses", compiled.stderr)
            if expected_stats and (not stats or tuple(map(int, stats.groups())) != expected_stats):
                raise RuntimeError(f"{label}: unexpected cache counts: {compiled.stderr}")
            return out.read_bytes()

        base = emit("base", PROGRAM)
        edit = emit("edit", PROGRAM.replace("x + 2", "x + 5"))
        expected_base = build(base, "base", disabled)
        expected_edit = build(edit, "edit", disabled)
        if expected_base == expected_edit:
            raise RuntimeError("the edit should change executable bytes")
        assert expected_base == build(base, "cold", enabled, (0, 3))
        assert expected_base == build(base, "warm", enabled, (3, 0))
        assert expected_edit == build(edit, "edited", enabled, (2, 1))
        cache.write_bytes(b"truncated cache")
        assert expected_edit == build(edit, "repair", enabled, (0, 3))
        with ThreadPoolExecutor(max_workers=2) as workers:
            built_base = workers.submit(build, base, "parallel-base", enabled)
            built_edit = workers.submit(build, edit, "parallel-edit", enabled)
            assert expected_base == built_base.result()
            assert expected_edit == built_edit.result()
        assert expected_edit == build(edit, "after-concurrency", enabled)
        print("PASS function cache: byte parity, one-function edit, corruption and concurrent writers")


if __name__ == "__main__":
    main()
