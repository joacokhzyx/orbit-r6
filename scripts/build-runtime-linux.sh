#!/usr/bin/env bash
# Builds the Linux x64 runtime archives from a pinned Orbit checkout, the
# counterpart of the Windows recipe in docs/PIN.md. The archives are not
# committed.
# usage: build-runtime-linux.sh <pinned-orbit-checkout> [output-dir]
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
orbit_root=${1:?usage: build-runtime-linux.sh <pinned-orbit-checkout> [output-dir]}
out=${2:-$root/.r6-build/runtime-linux-x64}
cc=${CC:-gcc}
mkdir -p "$out"
python3 "$orbit_root/scripts/mklib_runtime.py" --cc "$cc" --out "$out/libruntime_net.a"
python3 "$orbit_root/scripts/mklib_runtime.py" --cc "$cc" --db --out "$out/libruntime_net_db.a"
