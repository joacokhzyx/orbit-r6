# ELF64 writer probe

`src/codegen-x64/elf64.c` writes the object model as an ELF64 ET_REL for
x86-64. `probe.c` emits `.text` with `movl $42, %eax; ret` and a global
`main` at offset 0.

```bash
mkdir -p .r6-build
gcc -std=c11 -Wall -Wextra -Werror tests/elf/probe.c \
  src/codegen-x64/elf64.c src/codegen-x64/coff.c -o .r6-build/elf-probe
.r6-build/elf-probe .r6-build/elf-probe.o
readelf -h .r6-build/elf-probe.o    # ET_REL, EM_X86_64 (62)
readelf -S -W .r6-build/elf-probe.o # .text AX align 16, .note.GNU-stack
readelf -s -W .r6-build/elf-probe.o # main FUNC GLOBAL, section 1
```

Link with `ld` and the glibc start files, then run:

```bash
ld -o .r6-build/elf-probe.exe --dynamic-linker /lib64/ld-linux-x86-64.so.2 \
  /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o \
  .r6-build/elf-probe.o -lc /usr/lib/x86_64-linux-gnu/crtn.o
./.r6-build/elf-probe.exe; echo $?   # 42
```

`ld` only links; the object comes from the writer, not from GAS. Paths are the
Ubuntu layout under WSL. COFF relocation type 4 becomes `R_X86_64_PC32` for a
defined symbol and `R_X86_64_PLT32` for an undefined one, both with addend -4;
type 3 becomes `R_X86_64_32S` with addend 0.
