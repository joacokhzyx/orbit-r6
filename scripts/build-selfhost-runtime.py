#!/usr/bin/env python3
"""Build the separate runtime archive used to link the Orbit compiler."""

import argparse
import os
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("orbit_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    sys.path.insert(0, str(args.orbit_root.resolve() / "scripts"))
    from mklib_runtime import build_lib

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("R6_CC") or os.environ.get("CC") or "gcc"
    with TemporaryDirectory() as work:
        return build_lib(cc, output, Path(work), defines=("ORBIT_WITH_EXEC",))


if __name__ == "__main__":
    sys.exit(main())
