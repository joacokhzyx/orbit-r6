"""Measure a clean self-host build and the build after a one-function edit."""
import argparse
import json
import os
from pathlib import Path
import re
from statistics import median
import subprocess
import tempfile
from time import perf_counter


LITERAL = re.compile(
    rb'"opcode"\s*:\s*"load_const",\s*"dest"\s*:\s*[0-9]+,\s*'
    rb'"operand1"\s*:\s*\{\s*"kind"\s*:\s*"Int",\s*"value"\s*:\s*([1-9])\s*\}'
)


def compile_one(driver, ir, output, runtime_dir, cache=None):
    env = dict(os.environ, R6_TIMINGS="1", R6_FUNCTION_CACHE=str(cache) if cache else "")
    start = perf_counter()
    proc = subprocess.run(
        [str(driver), "build", str(ir), "-o", str(output), "--runtime-dir", str(runtime_dir)],
        env=env, capture_output=True, text=True, timeout=120,
    )
    wall = (perf_counter() - start) * 1000
    if proc.returncode:
        raise RuntimeError(proc.stderr)
    return {"wall_ms": wall, "timing": proc.stderr}, output.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("r6", "reference-r6", "ir", "runtime-dir", "json"):
        parser.add_argument("--" + flag, type=Path, required=True)
    parser.add_argument("--reps", type=int, default=5)
    args = parser.parse_args()
    if args.reps < 1:
        parser.error("repetitions must be positive")
    base = args.ir.read_bytes()
    match = LITERAL.search(base)
    if not match:
        parser.error("no one-digit Int load_const in the compiler IR")
    point = match.start(1)
    value = b"2" if match[1] == b"1" else b"1"
    modes = ("reference_clean", "prototype_clean", "prototype_cache_cold", "prototype_edit_hit")
    records = {mode: [] for mode in modes}
    with tempfile.TemporaryDirectory(prefix="r6-pass-cache-") as scratch:
        root = Path(scratch)
        edit = root / "edited.ir"
        edit.write_bytes(base[:point] + value + base[point + 1:])
        cache = root / "module.cache"
        output = root / ("compiler.exe" if os.name == "nt" else "compiler")
        for repetition in range(args.reps):
            cache.unlink(missing_ok=True)
            reference, _ = compile_one(args.reference_r6, edit, output, args.runtime_dir)
            clean, expected = compile_one(args.r6, edit, output, args.runtime_dir)
            cold, _ = compile_one(args.r6, args.ir, output, args.runtime_dir, cache)
            warm, recovered = compile_one(args.r6, edit, output, args.runtime_dir, cache)
            if recovered != expected:
                raise RuntimeError("cached edit changed the linked executable")
            if not re.search(r"\b[1-9][0-9]* hits 1 misses", warm["timing"]):
                raise RuntimeError(f"the edit did not invalidate exactly one function: {warm}")
            for mode, record in zip(modes, (reference, clean, cold, warm)):
                records[mode].append(record)
            print(repetition + 1, {mode: round(items[-1]["wall_ms"], 1)
                                   for mode, items in records.items()}, flush=True)
    args.json.write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print({mode: round(median(item["wall_ms"] for item in items), 1)
           for mode, items in records.items()})


if __name__ == "__main__":
    main()
