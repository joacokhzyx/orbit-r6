# AGENTS.md — R6

R6 is a native x86-64 code generator for the Orbit language IR, pinned to
`orbit-lang` `main` commit `01e31e882610cea21fbfbf4fe02fee00051fab2e` (D-F0-21).
[`docs/PIN.md`](docs/PIN.md) lists every pinned input and its hash;
[`CONTRIBUTING.md`](CONTRIBUTING.md) owns the commands. This file holds the
rules that a command list cannot express.

`C:\Users\Usuario\Downloads\orbit-lang` is read-only to R6, and *its commit* is
the pin. The working tree there can be ahead of the pin and dirty, so pinned
bytes come from `git cat-file blob 01e31e8:<path>`, never from the working
tree. Do not write there.

## Working rules

- **Fail closed.** An opcode, version, type or platform outside the supported
  set gets one diagnostic and a nonzero exit. No stubs, invented semantics or
  fallback.
- **The target is the host.** `src/codegen-x64/target.h` derives
  `R6_TARGET_WIN64` from `_WIN32`. One binary builds one target; there is no
  `--target` flag and no cross build (D-M1-119).
- **No C and no assembler in the program path.** `win64.c` emits a private
  GAS-syntax text, `peephole.c` and `regalloc.c` rewrite it, `asm_win64.c`
  encodes it, `coff.c` or `elf64.c` writes the object. GNU `ld` only links.
- **Do not generalize.** The measured subset is
  [`docs/CONTRACT.md`](docs/CONTRACT.md#measured-subset). Overflow, NaN
  payloads, other list kinds, macOS and threading are not evidence.
- **Never commit a build artifact.** The runtime archives are rebuilt from the
  pin and are not byte-reproducible (D-M1-119).
- **One writer of the pin at a time.** `docs/PIN.md`, `docs/DECISIONS.md`,
  `tests/corpus/manifest.json` and `tests/corpus/baseline.json` move together.
  Check who moved the pin last:

  ```bash
  git log -1 --format='%h %ad %s' --date=short -- docs/PIN.md
  ```

## Commands

[`CONTRIBUTING.md`](CONTRIBUTING.md) carries the build, the gates and the
runtime setup. The two facts agents get wrong:

- Run the driver from the repository root unless `--runtime-dir` or
  `R6_RUNTIME_DIR` names the archive directory; the default is relative to the
  current directory (D-M1-98).
- `--r6-command` is the last argument of the harnesses, and only `{ir}` and
  `{exe}` are quoted in a POSIX shell.

A diagnostic is `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`. Exit 2 is CLI
misuse and exit 1 is every other failure.

## Layout

| Path | Content |
|---|---|
| `src/loader/ir.{h,c}` | Strict IRv1 parser and validator; owns `R6Module` |
| `src/codegen-x64/` | `target.h`, `win64.c`, `peephole.c`, `regalloc.c`, `asm_win64.c`, `coff.c`, `elf64.c`, `text.{h,c}` |
| `src/driver/r6.c` | CLI, temporaries, direct `ld` link |
| `tests/diff/` | Differential harness; `fixtures/` holds the 100 pairs and their catalogue |
| `tests/reject/` | Loader suite; `manifest.json` is the truth, `cases/` is rendered |
| `tests/corpus/` | Language-emitted corpus, manifest and one-way ratchet |
| `tests/compiler-ir/` | Census of the compiler's own IR |
| `tests/perf/` | Compile-time measurement and per-platform baseline |
| `scripts/` | `build-r6.ps1`, `build-r6.sh`, `build-runtime-linux.sh`, `selfhost.ps1`, `selfhost.sh`, `ci.ps1`, `bisect.ps1` |
| `docs/` | `README.md` index, `PIN.md`, `CONTRACT.md`, `DECISIONS.md`, `BRANCHES.md` |

## Adding IR support

1. Add the fixture pair (`.json` plus `.orb`) to `tests/diff/fixtures/` and its
   row to `tests/diff/fixtures/README.md`. Add the reject cases to
   `tests/reject/manifest.json`; a new admission needs a positive and a
   negative control.
2. Record the decision in `docs/DECISIONS.md` with the evidence that supports
   it.
3. Widen the loader's admitted range, lower the opcode, and update
   `docs/CONTRACT.md`.
4. Run the differential, the loader suite and the corpus on both hosts.

## Working with other agents

| Zone | Parallel | Reason |
|---|---|---|
| `src/**` and promoting | no | one codegen pipeline, one writer |
| `tests/reject/manifest.json` | no | it is the suite truth; `make_cases.py` renders `cases/` |
| `tests/diff/fixtures/` for different cases | yes | additive |
| `src/loader`, `src/codegen-x64`, `src/driver`, separate files | yes, carefully | the pipeline takes one writer and `win64.c` is one file |
| `docs/*.md` | yes, different files | |
| `docs/PIN.md`, `docs/DECISIONS.md`, corpus manifest and baseline | no | they move together as one pin |

Do not compare two corpus measurements taken against different `--orbit-root`
trees. Materialize the pin and use it.

## The read-only reviewer

A reviewer edits nothing. It reads the source diff, the fixture or reject case,
the decision and the commit body with its Evidence block. A count in the body
that differs from what the gate prints is the finding.

- It runs `tests/diff/run_diff.py` and `tests/reject/run_reject.py` with the
  pinned inputs. It does not move the pin and does not `--write-baseline`.
- Three questions:
  1. Does the change do what the fixture claims? A new admission needs a
     positive and a negative control in the same commit.
  2. Is it silent on the control? Admitting one program must not admit the one
     next to it.
  3. Does it survive both gates and the corpus without a regression? The
     ratchet only goes up.
- A NO means redo. A defect in another file is separate work. A reviewer that
  cannot run the gates says so and does not sign.

## When to stop and hand back

Stop and ask rather than push on, when:

- **A first refusal disappears without a change you made.** The corpus moved
  under you, usually because the checkout advanced.
- **A gate fails only on a case you did not touch.** `docs/PIN.md` and the
  corpus manifest are the authority.
- **`docs/DECISIONS.md` or `docs/PIN.md` contradicts your change.** They are the
  contract. Update them in the same commit, or ask first.
- **A change needs a second first refusal to pass.** One decision is one
  commit.
- **You are about to generalize beyond the measured subset.** That is a new
  decision with its own evidence.

## Writing

Both repositories write English. Write for the person who takes the pin
tomorrow: they have the repository and the gates, not this session.

- Documentation is impersonal. A commit body may say "I".
- One idea per sentence. Average under 22 words, none over 40. A semicolon
  usually means two sentences.
- Every claim carries a command, a number or a line. "Faster", "cleaner" and
  "more robust" carry nothing.
- Name the thing: "the 39/93 corpus", not "the coverage"; "the first refusal at
  `list_create`", not "the gap".
- User-facing prose names the fact, not its decision. `README.md`,
  `CONTRIBUTING.md` and `docs/README.md` do not cite `D-M1-xx` or `D-F0-xx`.
  A decision ID belongs in `docs/DECISIONS.md`, `docs/CONTRACT.md`,
  `docs/PIN.md` and commit bodies, where it points at the evidence. Write the
  measurement and link the document.
- No hedges. "Should", "likely" and "probably" become a measurement or a
  question.
- A table is for more than two columns. Two facts are a sentence.
- Do not explain the sentence you are about to write, or the one you just wrote.
- Banned in new text: shape, surface, lane, load-bearing, battery, premise,
  clause, witness, residual, honest, and their inflections.
- Defined once, used freely: **pin** (the Orbit commit and the inputs in
  `docs/PIN.md`), **corpus** (the executable documents in `tests/corpus/`),
  **ratchet** (the one-way baseline), **decision** (a dated entry in
  `docs/DECISIONS.md`).
- New entries in `docs/DECISIONS.md` are English. Entries written before
  2026-10-31 stay as they are.

## Commits

One decision, one commit. [`CONTRIBUTING.md`](CONTRIBUTING.md#commits) carries
the message format and the Evidence block. Two rules the format does not say:

- A pin move is one commit: `docs/PIN.md`, `docs/DECISIONS.md`, the regenerated
  corpus manifest and the re-taken baseline together.
- Never rewrite a pushed commit. Fix forward.
