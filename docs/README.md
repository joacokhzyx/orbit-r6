# R6 documentation

Start with the question you are trying to answer. Every document in the
repository appears in the first table, and the second table names the
normative document for each topic.

| Question | Document | Answer in one line |
|---|---|---|
| What is R6 and how do I run it? | [`../README.md`](../README.md) | A native x86-64 code generator for Orbit IR, with a five-minute start |
| How do I build it, gate it and contribute? | [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Build, runtime setup, every gate command, review and commits |
| What exactly does R6 accept? | [`CONTRACT.md`](CONTRACT.md) | Loader, codegen, ABI, link inputs, spec alignment, measured subset |
| Which Orbit commit is the pin, and which hashes? | [`PIN.md`](PIN.md) | Every pinned input, its hash and how to move the pin |
| Why is it built this way? | [`DECISIONS.md`](DECISIONS.md) | 143 dated decisions, with an index and supersessions |
| How do branches and releases work? | [`BRANCHES.md`](BRANCHES.md) | `main`, `develop`, `feature/*`, gates and the pin between repos |
| How does the differential work, and what is each fixture? | [`../tests/diff/README.md`](../tests/diff/README.md) | Harness, verdict and fixture catalogue |
| How does the loader suite work? | [`../tests/reject/README.md`](../tests/reject/README.md) | Manifest, renderer, one diagnostic per rejection, cases by rule |
| How is the corpus measured? | [`../tests/corpus/README.md`](../tests/corpus/README.md) | Manifest, baseline, ratchet and what fails |
| How is compile time measured? | [`../tests/perf/README.md`](../tests/perf/README.md) | Three measurements, staging and per-platform baseline |
| How does the ELF64 writer work? | [`../tests/elf/README.md`](../tests/elf/README.md) | Writer probe, `readelf` and link checks |
| How do agents work in this tree? | [`../AGENTS.md`](../AGENTS.md) | Parallel zones, the reviewer, stop rules, writing and commits |

## Authority

When two documents disagree, the measured artifact and the gate win. This table
names the document that decides each topic.

| Topic | Normative document | What it decides |
|---|---|---|
| IR format and semantics | `CONTRACT.md` plus `orbit-lang/docs/IR_SPEC.md` at the pin | What the loader accepts and what codegen means |
| Pinned inputs | `PIN.md` | Commit, hashes, recipes and the symbol set |
| Gate commands and counts | `CONTRIBUTING.md` and the runner output | How a check runs and what it printed |
| Current measurement | `tests/*/baseline.json` and the runner output | The number a ratchet compares against |
| Code interfaces | `CONTRACT.md` | Loader, codegen and driver boundaries |
| Workflow | `BRANCHES.md` and `AGENTS.md` | Branches, review and stop rules |
| History | `DECISIONS.md` | Why a choice was made, and what superseded it |

A document that is not in the first table does not exist for the reader. New
content goes into an existing row before a new file is created.
