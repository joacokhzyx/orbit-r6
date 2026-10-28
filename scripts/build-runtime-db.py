#!/usr/bin/env python3
"""Build the database variant of the pinned runtime archive (D-M1-87).

The language links a program that calls the database against the whole runtime
compiled with -DORBIT_WITH_DB, -I<runtime>/vendor and
<runtime>/vendor/win-x64/sqlite3.lib (compiler/host/pipeline.orb). R6 does the
same with two files beside the plain archive:

    .r6-build/runtime-win64/libruntime_net_db.a
    .r6-build/runtime-win64/sqlite3.lib

The archive is the translation unit of scripts/mklib_runtime.py at the pin
with one more define. The driver picks it when a module declares an orbit_db_*
extern. Nothing here is committed; both files are build output.

    python scripts/build-runtime-db.py --orbit-root C:\\Temp\\orbit-pin-bf38b38-git \\
        --cc C:\\w64devkit\\bin\\gcc.exe
"""

import argparse
import importlib.util
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--orbit-root", required=True, type=pathlib.Path,
                    help="checkout of orbit-lang at the pin")
    ap.add_argument("--cc", default="gcc")
    ap.add_argument("--out-dir", type=pathlib.Path,
                    default=REPO / ".r6-build" / "runtime-win64")
    args = ap.parse_args()

    root = args.orbit_root.resolve()
    runtime = root / "runtime"
    vendor = runtime / "vendor"
    recipe = root / "scripts" / "mklib_runtime.py"
    for need in (recipe, runtime / "runtime.h", vendor / "sqlite3.h",
                 vendor / "win-x64" / "sqlite3.lib"):
        if not need.is_file():
            print(f"build-runtime-db: missing {need}", file=sys.stderr)
            return 1

    spec = importlib.util.spec_from_file_location("pinned_mklib_runtime", recipe)
    recipe_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recipe_module)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    archive = args.out_dir / "libruntime_net_db.a"
    with tempfile.TemporaryDirectory(prefix="r6db_") as work:
        work = pathlib.Path(work)
        tu = work / "orbit_lib.c"
        obj = work / "orbit_lib.o"
        tu.write_text("#define ORBIT_WITH_DB\n" + recipe_module.LIB_TU, encoding="utf-8")
        proc = subprocess.run(
            [args.cc, "-O2", "-Wall", "-I", str(runtime), "-I", str(vendor),
             str(tu), "-c", "-o", str(obj)],
            cwd=str(root), capture_output=True, text=True, errors="replace")
        if proc.returncode != 0:
            sys.stderr.write(proc.stderr)
            print("build-runtime-db: the library TU did not compile", file=sys.stderr)
            return 1
        if archive.exists():
            archive.unlink()
        proc = subprocess.run(["ar", "rcs", str(archive), str(obj)],
                              capture_output=True, text=True, errors="replace")
        if proc.returncode != 0:
            sys.stderr.write(proc.stderr)
            print("build-runtime-db: ar failed", file=sys.stderr)
            return 1
    shutil.copyfile(vendor / "win-x64" / "sqlite3.lib", args.out_dir / "sqlite3.lib")
    print(f"build-runtime-db: wrote {archive} and {args.out_dir / 'sqlite3.lib'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
