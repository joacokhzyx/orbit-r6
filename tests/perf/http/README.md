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
