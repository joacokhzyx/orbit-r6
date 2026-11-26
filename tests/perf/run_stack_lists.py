"""Compare local stack lists with owning lists and check their code paths."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


FIXTURE = Path(__file__).with_name("local_lists.orb").resolve()


def calls_by_function(exe):
    text = subprocess.check_output(["objdump", "-d", str(exe)], text=True, timeout=30)
    calls = {}
    function = None
    for line in text.splitlines():
        label = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
        if label and not label[1].startswith("."):
            function = label[1]
        elif re.search(r"\bcallq?\s+[^\n]*<orbit_list_create_typed>", line):
            calls[function] = calls.get(function, 0) + 1
    return calls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--orbit-root", type=Path, required=True)
    parser.add_argument("--r6", type=Path, required=True)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    args = parser.parse_args()
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="r6-stack-lists-") as temp:
        scratch = Path(temp)
        ir = scratch / "fixture.ir"
        oracle = scratch / f"oracle{suffix}"
        subprocess.run([str(args.compiler.resolve()), "build", str(FIXTURE), "--backend=c",
                        "--emit-ir", str(ir), "-o", str(oracle)],
                       cwd=args.orbit_root, check=True, timeout=90)
        expected = subprocess.check_output([str(oracle)], cwd=scratch, text=True, timeout=10)
        if expected.splitlines() != ["4504500", "111", "222", "333", "444"]:
            raise RuntimeError(f"unexpected C output: {expected!r}")
        for enabled in (0, 1):
            exe = scratch / f"native-{enabled}{suffix}"
            env = dict(os.environ, R6_STACK_LISTS=str(enabled))
            subprocess.run([str(args.r6.resolve()), "build", str(ir), "-o", str(exe),
                            "--runtime-dir", str(args.runtime_dir.resolve())],
                           cwd=scratch, env=env, check=True, timeout=30)
            actual = subprocess.check_output([str(exe)], cwd=scratch, text=True, timeout=10)
            if actual != expected:
                raise RuntimeError(f"stack lists setting {enabled} returned {actual!r}")
            calls = calls_by_function(exe)
            if calls.get("orbit_main", 0) != 1 - enabled:
                raise RuntimeError(f"loop allocation count at setting {enabled}: {calls}")
            for control in ("returnList", "callWithList", "growingList"):
                if calls.get("o_" + control, 0) != 1:
                    raise RuntimeError(f"owning control {control} changed: {calls}")
        print("PASS local lists: 3000 iterations; zero loop allocations with rewrite; three owning controls")


if __name__ == "__main__":
    main()
