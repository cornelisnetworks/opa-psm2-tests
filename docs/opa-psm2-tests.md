---
title: "Opa Psm2 Tests User Guide"
date: "2026-04-02"
status: "draft"
---

# Opa Psm2 Tests — User Guide

---

## NAME

**opa-psm2-tests** — PSM2 performance benchmarking and connectivity testing suite for Cornelis Networks Omni-Path Architecture (OPA) fabrics.

## SYNOPSIS

```text
latency [server] [-m size] [-M size] [-f --flush] [--mqstats] [-h --help]
bw-mrate [server] [-m size] [-M size] [-f --flush] [--mqstats] [-h --help]
bi-bw-mrate [server] [-m size] [-M size] [-f --flush] [--mqstats] [-h --help]
test_tool [server] [-s size] [-a] [-h]
```

## DESCRIPTION

`opa-psm2-tests` is a collection of PSM2 (Performance Scaled Messaging 2) micro-benchmarks and diagnostic tools for Cornelis Networks / Intel Omni-Path Architecture fabric adapters. The suite provides four standalone executables — a ping-pong latency benchmark, a unidirectional bandwidth and message rate benchmark, a bidirectional bandwidth and message rate benchmark, and a connection test tool — all built on a shared infrastructure layer that manages PSM2 endpoint lifecycle, TCP-based out-of-band coordination, command-line argument parsing, and high-resolution timing.

Each benchmark operates in a two-process client/server model across an OPA fabric link. One node runs in server mode (no positional argument) while the other runs in client mode (passing the server hostname as a positional argument). A TCP socket is established first to exchange PSM2 endpoint addresses and benchmark parameters, after which all performance-critical traffic flows over the native OPA fabric path using the PSM2 Matched Queue (MQ) API. Message sizes are swept in powers of two from a configurable minimum to maximum, and results are reported in tabular format suitable for scripted collection and analysis.

The `test_tool` utility complements the performance benchmarks by providing functional validation of PSM2 connectivity, data integrity, and tag-matching correctness. It is designed for fabric bring-up verification and CI pipeline integration, reporting PASS/FAIL/SKIP status for each test and returning a non-zero exit code on any failure.

## SUBCOMMANDS

`opa-psm2-tests` ships as four separate executables rather than a single binary with subcommands. Each executable is invoked directly.

| Executable | Description |
|---|---|
| `latency` | Measures ping-pong round-trip latency across message sizes, reporting one-way latency in microseconds. |
| `bw-mrate` | Measures unidirectional bandwidth (MB/s) and message rate (Mmps) using windowed asynchronous sends with ACK synchronization. |
| `bi-bw-mrate` | Measures bidirectional bandwidth (MB/s) and message rate (Mmps) with both endpoints sending and receiving simultaneously. |
| `test_tool` | Validates PSM2 connectivity, data integrity, and tag-match isolation. Reports PASS/FAIL/SKIP per test. |

## OPTIONS

### Performance Benchmark Options (latency, bw-mrate, bi-bw-mrate)

| Flag | Type | Default | Description |
|---|---|---|---|
| `server` (positional) | string | *(none — act as server)* | Hostname or IP of the server node. When omitted, the process becomes the server. When provided, the process becomes the client. |
| `-m` | integer | `1` | Starting (minimum) message size in bytes. Must be a positive non-zero value. |
| `-M` | integer | `4194304` (4 MiB) | Ending (maximum) message size in bytes. Must be ≥ the value of `-m`. |
| `-f` / `--flush` | boolean | off | Flush the L3 cache before running the benchmark. Helps produce more consistent results by eliminating cache warm-up effects. |
| `--mqstats` | boolean | off | Print PSM2 Matched Queue statistics (byte counts, message counts, eager/rendezvous splits) after the benchmark completes. |
| `-h` / `--help` | boolean | off | Print usage information and exit. |

!!! warning "Server ignores CLI arguments"
    All benchmark parameters (`-m`, `-M`, `-f`, `--mqstats`) are set on the **client** and transmitted to the server via the TCP coordination socket. Any arguments passed to the server process are ignored (with a warning printed to stdout).

### Test Tool Options (test_tool)

| Flag | Type | Default | Description |
|---|---|---|---|
| `server` (positional) | string | *(none — act as server)* | Hostname or IP of the server node. When omitted, the process becomes the server. |
| `-s` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests. Without this flag, only the `ping_pong` test executes. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip
    The `-s` option only affects the `data_integrity` test. It has no effect unless `-a` is also specified.

### Compile-Time Constants

| Constant | Value | Description |
|---|---|---|
| `SERVER_PORT` | `33087` | TCP port for out-of-band socket coordination (benchmarks). `test_tool` uses `SERVER_PORT + 1` = `33088`. |
| `WINDOW` | `64` | Number of outstanding asynchronous messages in bandwidth tests. |
| `ITERS_LARGE` | `50000` | Iteration count for small messages (≤ 64 KiB) in latency and bi-directional BW tests. |
| `ITERS_MEDIUM` | `500` | Iteration count for small messages in unidirectional BW tests. |
| `ITERS_SMALL` | `50` | Iteration count for large messages (> 64 KiB) across all benchmarks. |
| `LARGE_MSG` | `65536` | Message size threshold at which iteration count switches from large/medium to small. |
| `MAX_PSM2_RANKS` | `2` | Maximum number of PSM2 endpoints (one server + one client). |
| `PING_ITERS` | `100` | Iteration count for the `test_tool` ping-pong smoke test. |
| `TEST_MAX_MSG` | `1048576` (1 MiB) | Maximum payload size for `test_tool` data-integrity tests. |

## Shannon Commands

*Not applicable.* `opa-psm2-tests` executables are standalone CLI binaries and are not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `opa-psm2-tests` does not expose a REST API. Inter-node coordination uses private TCP sockets on ports `33087` (benchmarks) and `33088` (`test_tool`) solely for PSM2 endpoint address exchange and parameter synchronization.

## EXAMPLES

!!! warning "Prerequisites"
    Both nodes must have the `opa-psm2` library (`libpsm2`) installed, an active OPA fabric link, and the `hfi1` kernel driver loaded. The PSM2 development headers (`libpsm2-devel`) are required to build from source. The **server** process must be started **before** the client.

### Example 1 — Build from source and run a quick latency check

```bash
# Clone the repository
git clone https://github.com/cornelisnetworks/opa-psm2-tests.git
cd opa-psm2-tests

# Build all benchmarks (requires libpsm2-devel)
make

# Build the test tool
cd test_tool && make && cd ..
```

### Example 2 — Run the latency benchmark with default settings

=== "Server (node-a)"

    ```bash
    # Start the server — no positional argument means server mode
    ./latency
    ```

=== "Client (node-b)"

    ```bash
    # Connect to the server and run the latency sweep (1 B to 4 MiB)
    ./latency node-a
    ```

Expected output:

```text
# PSM2 Ping Pong Latency Test
# Message Size(B)      Latency(us)
1                         1.23
2                         1.24
4                         1.25
...
4194304                  512.34
```

### Example 3 — Run unidirectional bandwidth with a custom message size range

=== "Server (node-a)"

    ```bash
    ./bw-mrate
    ```

=== "Client (node-b)"

    ```bash
    # Sweep from 1 KiB to 1 MiB only
    ./bw-mrate node-a -m 1024 -M 1048576
    ```

Expected output:

```text
# PSM2 Uni-directional Bandwidth, Message Rate Test
# Message Size(B)  Bandwidth(MB/s)  Message Rate(Mmps)
1024                     2345.67                2.29
2048                     4567.89                2.23
...
1048576                 11234.56                0.01
```

### Example 4 — Run bidirectional bandwidth with L3 cache flush and MQ statistics

=== "Server (node-a)"

    ```bash
    ./bi-bw-mrate
    ```

=== "Client (node-b)"

    ```bash
    # Flush L3 cache before benchmark and show PSM2 MQ counters after
    ./bi-bw-mrate node-a -f --mqstats
    ```

Expected output:

```text
Flushing L3 Cache... will take a few seconds
Flushed L3 cache (29360128)
# PSM2 Bi-directional Bandwidth, Message Rate Test
# Message Size(B)  Bandwidth(MB/s)  Message Rate(Mmps)
1                       1234.56                1234.56
...
4194304                22345.67                0.01
PSM2 MQ STATS:
rx_user_bytes 123456789
rx_user_num 12345
tx_num 12345
tx_eager_num 11000
tx_eager_bytes 98765432
tx_rndv_num 1345
tx_rndv_bytes 24691358
...
```

### Example 5 — Run the test_tool with all tests and a 64 KiB data-integrity payload

=== "Server (node-a)"

    ```bash
    ./test_tool/test_tool
    ```

=== "Client (node-b)"

    ```bash
    # Run all tests with a 64 KiB data-integrity message
    ./test_tool/test_tool node-a -a -s 65536
    ```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
  [PASS] data_integrity                      456.78 us  (65536 bytes verified)
  [PASS] tag_match                           234.56 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 6 — Loopback latency test on a single node

```bash
# Force PSM2 to use shared-memory transport for single-node testing
export PSM2_DEVICES=self,shm

# Start server in background
./latency &
sleep 1

# Run client against localhost
./latency localhost

# Wait for background server to finish
wait
```

### Example 7 — Scripted CI fabric validation using test_tool

```bash
#!/bin/bash
# fabric_check.sh — run on the client node
# Usage: ./fabric_check.sh <server_hostname>

set -euo pipefail
SERVER_HOST="$1"

cd /opt/opa-psm2-tests/test_tool

./test_tool "$SERVER_HOST" -a -s 8192
rc=$?

if [ $rc -ne 0 ]; then
    echo "FABRIC CHECK FAILED — see test output above" >&2
    exit 1
fi

echo "All fabric connectivity tests passed."
```

### Example 8 — Compare unidirectional vs. bidirectional bandwidth

```bash
# === On the server node (node-a) ===
# Run unidirectional server first
./bw-mrate
# After client finishes, run bidirectional server
./bi-bw-mrate

# === On the client node (node-b) ===
# Capture unidirectional results
./bw-mrate node-a -m 4096 -M 4194304 | tee uni-bw.txt

# Capture bidirectional results
./bi-bw-mrate node-a -m 4096 -M 4194304 | tee bi-bw.txt

# Compare results side by side
paste uni-bw.txt bi-bw.txt
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | `self,shm,hfi` | Comma-separated list of PSM2 device types to use. Set to `self,shm` for loopback testing without a fabric peer. |
| `HFI_UNIT` | No | `0` | Selects the HFI adapter unit number when multiple adapters are present. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal debug tracing. Increase for verbose diagnostics. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | `131072` | Rendezvous window size in bytes. May affect large-message bandwidth performance. |
| `PSM2_MQ_RECVREQS_MAX` | No | `1048576` | Maximum number of pre-posted receive requests in the PSM2 matched queue. |
| `PSM2_MTU` | No | *(driver default)* | Override the maximum transfer unit size used by PSM2. |

!!! tip "Loopback testing"
    To test on a single node without a fabric peer, set `PSM2_DEVICES=self,shm` and use `localhost` as the server hostname. This exercises the shared-memory transport path rather than the HFI hardware path.

## FILES

| Path | Description |
|---|---|
| `Makefile` | Top-level build rules for all benchmark executables (`latency`, `bw-mrate`, `bi-bw-mrate`). |
| `latency.c` | Ping-pong latency benchmark executable source. |
| `bw-mrate.c` | Unidirectional bandwidth and message rate benchmark executable source. |
| `bi-bw-mrate.c` | Bidirectional bandwidth and message rate benchmark executable source. |
| `libpsm2.c` | Shared PSM2 lifecycle management: initialization, endpoint open/connect, shutdown, MQ statistics. |
| `libpsm2.h` | PSM2 wrapper API header: inline helpers for `post_send`, `post_isend`, `post_irecv`, `psm2_waitall`, `cancel`, `test`. |
| `psm2perf.c` | Shared benchmark infrastructure: `init_benchmark()`, `open_socket()`, `exchange_info()`, `get_cpu_rate()`. |
| `psm2perf.h` | Constants, macros (`SEND`, `RECV`, `TIMER`), data structures (`benchmark_info`), global buffers (`sbuff`, `rbuff`), and inline utilities (`flush_l3cache`, `ts_diff`). |
| `test_tool/test_tool.c` | Connection test tool: ping-pong, data-integrity, and tag-match test implementations plus CLI entry point. |
| `test_tool/test_tool.h` | Test tool header: constants (`TAG_*`, `TEST_MAX_MSG`, `PING_ITERS`), `test_result` struct, inline `fill_pattern`/`verify_pattern` helpers. |
| `test_tool/Makefile` | Build rules for the test tool. Links against `libpsm2.o`, `psm2perf.o`, and the system `libpsm2` library. |
| `/proc/cpuinfo` | Read at startup to detect CPU frequency (stored in `benchmark_info.cpu_freq`). |

## EXIT STATUS

### Performance Benchmarks (latency, bw-mrate, bi-bw-mrate)

| Code | Meaning |
|---|---|
| `0` | Benchmark completed successfully. |
| `-1` | Fatal error during initialization (argument parsing failure, socket open failure, PSM2 initialization failure) or during benchmark execution (TCP send/recv failure). |

### Test Tool (test_tool)

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (`num_fail == 0`). |
| `1` | One or more tests failed, **or** a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! warning "Partial execution"
    If a benchmark or test tool exits with a non-zero code due to an initialization error (e.g., socket failure, PSM2 endpoint open failure), no results are printed. Check `stderr` for diagnostic messages from `perror()` or the `PSM2_ERR` macro in this case.

## BREAKING CHANGES

No breaking changes have been introduced in the current release. The following items are noted for awareness:

- The `README` file has been replaced with a short pointer to the `docs/` directory. Users who previously relied on the `README` for usage instructions should now consult this user guide and the companion [Test Tool Design Reference](test-tool.md).
- The `test_tool` executable uses TCP port `33088` (`SERVER_PORT + 1`), which differs from the benchmark port `33087`. Firewall rules that previously only opened port `33087` must be updated to also allow `33088` when running the test tool.

## SEE ALSO

- [`opa-psm2`](https://github.com/cornelisnetworks/opa-psm2) — The PSM2 user-space library that these benchmarks and tests exercise.
- [`test_tool`](test-tool.md) — Detailed design reference for the PSM2 connection test tool.
- `psm2_mq_isend(3)`, `psm2_mq_irecv(3)`, `psm2_mq_wait(3)`, `psm2_mq_send(3)` — PSM2 Matched Queue API man pages.
- `psm2_ep_open(3)`, `psm2_ep_connect(3)`, `psm2_ep_close(3)` — PSM2 endpoint management API man pages.