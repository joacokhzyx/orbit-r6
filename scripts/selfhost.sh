#!/usr/bin/env bash
# Self-host check on Linux, the counterpart of scripts/selfhost.ps1:
#   1. The fixed-point compiler emits the IR of compiler/host/main.orb.
#   2. R6 builds that IR into orbitc.
#   3. orbitc compiles compiler/host/main.orb again; its generated C must hash
#      to PUBLISHED_C in the pin's scripts/verify_seed.py.
# Exit 0 = SELFHOST OK, 1 = anything else. Work files go to <Repo>/.r6-build/selfhost.
set -uo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)

orbit_root=${1:?usage: selfhost.sh <orbit-root> <fixed-point> <r6>}
compiler=${2:?usage: selfhost.sh <orbit-root> <fixed-point> <r6>}
r6=${3:?usage: selfhost.sh <orbit-root> <fixed-point> <r6>}

fail() { echo "SELFHOST FAIL: $*"; exit 1; }

commit=$(git -C "$orbit_root" rev-parse HEAD) || fail "$orbit_root is not a git checkout"
seed=$(git -C "$orbit_root" cat-file blob "$commit:scripts/verify_seed.py") ||
    fail "no scripts/verify_seed.py at $commit"
expected=$(printf '%s\n' "$seed" | sed -n 's/^PUBLISHED_C = "\([0-9A-Fa-f]\{64\}\)"/\1/p' | tr a-f A-F)
[ -n "$expected" ] || fail "no PUBLISHED_C in scripts/verify_seed.py at $commit"
echo "pin $commit, PUBLISHED_C $expected"

work=$repo/.r6-build/selfhost
rm -rf "$work"
mkdir -p "$work/run"
saved_temp=${TEMP-} saved_tmp=${TMP-} saved_cache=${ORBIT_CCACHE-} saved_skip=${ORBIT_SKIP_CC-}
cleanup() {
    export TEMP=$saved_temp TMP=$saved_tmp
    export ORBIT_CCACHE=$saved_cache ORBIT_SKIP_CC=$saved_skip
}
trap cleanup EXIT

ir=$work/main.ir
orbitc=$work/orbitc
export ORBIT_CCACHE=0 ORBIT_SKIP_CC=1
export TEMP=$work TMP=$work
(cd "$orbit_root" && "$compiler" build compiler/host/main.orb -o "$work/fp" --emit-ir "$ir" >/dev/null) ||
    fail "the fixed point emitted no IR"
[ -f "$ir" ] || fail "the fixed point emitted no IR"
echo "ir: $(wc -c < "$ir") bytes"

unset ORBIT_SKIP_CC
(cd "$repo" && "$r6" build "$ir" -o "$orbitc") || fail "R6 did not build the compiler"
[ -x "$orbitc" ] || fail "R6 produced no executable"

export ORBIT_SKIP_CC=1
export TEMP=$work/run TMP=$work/run
(cd "$orbit_root" && "$orbitc" build compiler/host/main.orb -o "$work/run/x" >/dev/null) ||
    fail "orbitc built by R6 did not compile the compiler"
c=$work/run/orbit_selfhost_build.c
[ -f "$c" ] || fail "orbitc wrote no orbit_selfhost_build.c"
got=$(sha256sum "$c" | cut -d' ' -f1 | tr a-f A-F)
echo "C sha256: $got"
[ "$got" = "$expected" ] || fail "expected $expected"
echo "SELFHOST OK"
