#!/usr/bin/env bash
# Builds the R6 driver for the host: an ELF executable on Linux, using the same
# sources as scripts/build-r6.ps1. GCC is used only here, never for an R6
# program.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cc=${R6_CC:-${CC:-gcc}}
out="$root/.r6-build/r6"
mkdir -p "$root/.r6-build"
"$cc" -std=c11 -O2 -Wall -Wextra -Werror \
    "$root/src/driver/r6.c" "$root/src/loader/ir.c" \
    "$root/src/codegen-x64/win64.c" "$root/src/codegen-x64/asm_win64.c" \
    "$root/src/codegen-x64/coff.c" "$root/src/codegen-x64/elf64.c" \
    "$root/src/codegen-x64/peephole.c" "$root/src/codegen-x64/regalloc.c" \
    -o "$out"
printf '%s\n' "$out"
