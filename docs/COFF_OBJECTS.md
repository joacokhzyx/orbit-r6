# AMD64 COFF emission foundation

`src/codegen-x64/coff.c` serializes caller-provided bytes and metadata as a
relocatable AMD64 COFF object. It emits section headers, raw section data,
symbol/string tables, and `IMAGE_REL_AMD64_REL32`-compatible relocation records
as supplied by the caller. It currently rejects auxiliary symbols and
relocation overflow. The instruction encoder and R6 driver do not use this
writer yet; the program path still uses GNU `as`.

## Verification

On 2026-10-14, `tests/coff/probe.c` emitted a COFF object containing a direct
`call rel32` relocation to a long-name function symbol (`orbit_probe`) and two
`.pdata` entries with `IMAGE_REL_AMD64_ADDR32NB` relocations to function and
unwind symbols. GNU `objdump -r -t -h` read the section, symbol, and relocation
tables. MinGW `ld.exe` resolved the relocations, linked the object with the
CRT, emitted a PE exception directory backed by `.pdata`, and produced an
executable that exited 0. The fixture's call displacement field was zero
before linking, matching the observed GNU assembler COFF output. Earlier
testing with an encoded `-4` addend crashed the linked probe; that encoding was
corrected based on the object/link/run evidence.

The probe establishes that the serializer's header, string table, symbol
indices, and REL32 relocation are accepted by this MinGW linker. It does not
establish R6 IR lowering through this writer, unwind metadata correctness for
nontrivial prologues, or runtime ABI conformance. The next implementation piece
is a byte encoder that feeds the writer, then driver integration and the
existing differential gate.

Alternativa descartada: depender de GAS para fabricar el objeto que consume
MinGW `ld`; se está construyendo un serializador COFF propio para enlazar bytes
emitidos por R6.
