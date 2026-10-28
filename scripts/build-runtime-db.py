#!/usr/bin/env python3
"""Build the database variant of the pinned runtime archive (D-M1-87, D-M1-103).

The language links a program that calls the database against the runtime
modules plus `database` and `auth`, and against
<runtime>/vendor/win-x64/sqlite3.lib (compiler/host/pipeline.orb). R6 does the
same with two files beside the plain archive:

    .r6-build/runtime-win64/libruntime_net_db.a
    .r6-build/runtime-win64/sqlite3.lib

The archive is the one `scripts/mklib_runtime.py --db` of the pin writes: one
member per module, 21 members. The driver picks it when a module declares an
orbit_db_* extern. Nothing here is committed; both files are build output.

    python scripts/build-runtime-db.py --orbit-root C:\\Temp\\orbit-pin-cc6b167-git \\
        --cc C:\\w64devkit\\bin\\gcc.exe
"""

import argparse
import pathlib
import shutil
import subprocess
import sys

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
    recipe = root / "scripts" / "mklib_runtime.py"
    sqlite = root / "runtime" / "vendor" / "win-x64" / "sqlite3.lib"
    for need in (recipe, sqlite):
        if not need.is_file():
            print(f"build-runtime-db: missing {need}", file=sys.stderr)
            return 1

    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    archive = out_dir / "libruntime_net_db.a"
    proc = subprocess.run(
        [sys.executable, str(recipe), "--db", "--quiet", "--cc", args.cc,
         "--out", str(archive)],
        cwd=str(root), capture_output=True, text=True, errors="replace")
    if proc.returncode != 0 or not archive.is_file():
        sys.stderr.write(proc.stdout + proc.stderr)
        print("build-runtime-db: mklib_runtime.py --db failed", file=sys.stderr)
        return 1
    shutil.copyfile(sqlite, out_dir / "sqlite3.lib")
    print(f"build-runtime-db: wrote {archive} and {out_dir / 'sqlite3.lib'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
