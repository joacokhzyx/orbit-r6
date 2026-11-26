# Pinned language inputs

R6 pins one `orbit-lang` commit and the inputs that make its evidence
reproducible. The commit is on the public remote
`https://github.com/joacokhzyx/orbit-lang`. A local checkout is an observation
of one machine; the commit is the pin.

| Input | Value | Evidence |
|---|---|---|
| Orbit commit | `b1b33e06d2b26d1c1bcfebe379038228a12c9957` (`codex/http-request-memory`) | D-F0-25 pins receive buffers and native request views. |
| IRv1 | `docs/IR_SPEC.md` blob `d6ad0d475d`, emitter `compiler/emit/ir_emit.orb` blob `fe65c6c797` | `git rev-parse b1b33e0:<path>`. |
| Corpus emitter | `scripts/measure.py` blob `41008f8e3d` | `python scripts/measure.py emit-ir --compiler <fixed-point> --out <scratch>` emits 154 documents (153 executable, one support), 7211598 bytes and `filesSha256 3c0227b3395a52f016301d470bf81b623250d2731074fd26d74a1250923bc345`. |
| Corpus ratchet | `tests/corpus/manifest.json` and `tests/corpus/baseline.json` | `python tests/corpus/run_corpus.py --check` requires 153 builds and 153 matches. |
| Compiler census | `tests/compiler-ir/baseline.json` | `python tests/compiler-ir/run_census.py --write-baseline` with the self-host runtime emits 29 documents, builds two entry points and refuses 27 library modules without a main. |
| Canonical C | `compiler/selfhost/stage3.exe.c` blob `f178fb863c` | `python scripts/dev.py fp` and `scripts/selfhost.ps1` reproduce `PUBLISHED_C D088CAE40D73B76E0D79FAC4EE87A15AF7339F4FB850678CEB2A4818C8004E10`. |
| Runtime build recipe | `scripts/mklib_runtime.py` blob `d6759a3a17` | `python scripts/mklib_runtime.py --cc gcc --out <archive>` builds 28 members. `--db` builds 30 members. Compiler self-hosting uses a separate `ORBIT_WITH_EXEC` archive. |
| Runtime ABI | `runtime/orbit_api.h` blob `90cc1c8ff0`, `runtime/http.h` blob `195f48b1e0`, `runtime/http_native.h` blob `b17b3e8c08`, `runtime/http_server.h` blob `1d685dccfe` | `git rev-parse b1b33e0:<path>`. |
| C backend | `compiler/emit/c_backend.orb` blob `2862510b8a` | `git rev-parse b1b33e0:compiler/emit/c_backend.orb`. |

## Reproducibility

A build artifact embeds the absolute path of the directory it was built in, so
no SHA-256 of a local binary or archive is a stable anchor. What is pinned: the
Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the
corpus `filesSha256`, and the symbol set. Every hash of a local artifact in this
file is an observation of one build (D-F0-11).

Pinned bytes come from `git cat-file blob b1b33e0:<path>`, never from a working
tree. The sibling checkout is neither the pin nor a stable reference: it can be
ahead of the pin and dirty. Corpus and census inputs come from a tree
materialized at the pin.

## Moving the pin

A pin move is one commit. It carries a dated entry in
[`DECISIONS.md`](DECISIONS.md), this file, the workflow pin, the regenerated
`tests/corpus/manifest.json` and the re-taken `tests/corpus/baseline.json`.
Run the compiler census against the same checkout. Record any refusal and
retain its existing baseline when the census would decrease `r6BuildOk`.

1. Push the new `orbit-lang` commit first. A pin to a commit no remote has
   cannot be reproduced.
2. Materialize the commit in a scratch checkout and build its fixed-point
   compiler.
3. Rebuild the runtime archives and regenerate the corpus against that tree.
4. Re-take the corpus baseline under the new pin and run the gates on both hosts.
5. Record the decision with the new hashes, the corpus inventory and the gate
   counts.

The pin history, including every superseded commit, is in
[`DECISIONS.md`](DECISIONS.md) under D-F0.
