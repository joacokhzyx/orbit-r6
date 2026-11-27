# HTTP/1.1 benchmark

Dispatch `ci.yml` on `experiment/http-request-memory`. The job runs the five
binaries on one Linux x64 VM, with server and client on disjoint logical CPUs.
It builds the original HTTP C runtime, R6 with the body copy enabled and
disabled, Go net/http and Tokio/Hyper. The workflow records compiler
versions, source revisions, the resolved Cargo lock file and the CPU model.

Each trial starts a fresh service, samples PSS before connecting and with
128 idle open connections, and warms those connections with the scenario's
request. It then samples PSS under load through `smaps_rollup`. Differences
from that service's pre-connection PSS are reported as bytes per connection.
The JSONL artifacts keep every sample, reply count, error count and p99.
The fixed-rate pass includes scheduling delay in p99. The saturation pass
starts a new request as soon as the previous response arrives. Both use
HTTP/1.1 over loopback, two server workers and the same fixed response,
captured parameter or 4096-byte POST. No TLS or access logs are enabled.

Compare memory at the common offered rate only where every variant keeps
up. Compare throughput in the saturation pass. Use the five separate
observations to judge run-to-run spread, and include total PSS alongside
the per-connection difference.

## Linux x64 results, 2026-11-26

Both invocations of `python -u tests/perf/http/run_bench.py` in CI completed
75 trials each with zero response errors. The fixed-rate pass used
`--rate 2048 --reps 5`. The saturation pass used `--rate 0 --reps 5`.
The artifact contains all 150 records, toolchain versions, run settings,
and the Cargo lock file. [CI run and raw results](https://github.com/joacokhzyx/orbit-r6/actions/runs/45888120964)

The table reports medians across five trials. PSS columns use the fixed-rate
pass. The delta columns are bytes per connection, measured relative to the
same process before opening connections. Total PSS uses KiB and p99 uses ms.

| Workload | Variant | Idle delta | Loaded delta | Loaded PSS | Fixed req/s | Fixed p99 | Saturated req/s | Saturated p99 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| fixed | Orbit C | 0 | 4320 | 5598 | 2047.6 | 1.13 | 74760.8 | 2.05 |
| fixed | R6 materializes | 32 | 320 | 5062 | 2047.6 | 1.13 | 77926.4 | 1.9 |
| fixed | R6 elides | 32 | 320 | 5062 | 2048 | 1.18 | 76352.4 | 1.94 |
| fixed | Go | 12896 | 54400 | 11900 | 2047.6 | 1.18 | 75196.8 | 3.04 |
| fixed | Tokio/Hyper | 10736 | 14704 | 2972 | 2048 | 1.15 | 72215.2 | 2 |
| param | Orbit C | 0 | 4320 | 5594 | 2047.6 | 1.13 | 75814 | 2.05 |
| param | R6 materializes | 32 | 320 | 5062 | 2047.6 | 1.12 | 75425.6 | 2.06 |
| param | R6 elides | 32 | 320 | 5062 | 2047.6 | 1.13 | 76211.2 | 1.97 |
| param | Go | 12992 | 53344 | 11756 | 2048 | 1.18 | 76579.6 | 2.09 |
| param | Tokio/Hyper | 10864 | 14384 | 2883 | 2047.2 | 1.17 | 72490.8 | 1.96 |
| post | Orbit C | 0 | 8480 | 6114 | 2047.6 | 1.16 | 73981.6 | 2.04 |
| post | R6 materializes | 32 | 640 | 5098 | 2047.6 | 1.16 | 73590.4 | 2.03 |
| post | R6 elides | 32 | 640 | 5102 | 2048 | 1.21 | 71606 | 2.12 |
| post | Go | 13248 | 54176 | 11876 | 2047.6 | 1.17 | 73316.8 | 2.15 |
| post | Tokio/Hyper | 10960 | 21584 | 3791 | 2047.6 | 1.14 | 69858.8 | 2.11 |

On POST, R6 with elision used 640 incremental bytes per connection, below
half of each reference at the common rate. Tokio/Hyper used 3,791 KiB total
under load, compared with 5,102 KiB for R6. The median fixed-rate POST p99
was 1.21 ms with elision and 1.14 ms for Tokio/Hyper. The five POST p99
values ranged from 1.18 to 1.47 ms with elision and from 1.11 to 1.14 ms on
Tokio/Hyper. The complete prototype criterion is unmet. Orbit C's idle PSS
increase was below measurement resolution. Both R6 settings measured 640
incremental bytes per connection for POST at fixed rate, so that pass
does not attribute the savings to body-copy elision alone.
