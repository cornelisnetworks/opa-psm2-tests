# Test Tool — Design Reference

```markdown
---
title: "Test Tool — Design Reference"
description: "PSM2 connection test tool for verifying connectivity, data integrity, and tag-matching over the OPA fabric"
---

# test_tool

## NAME

**test_tool** — PSM2 connection test tool that verifies connectivity, data integrity, and tag-match isolation over the Omni-Path Architecture (OPA) fabric.

## SYNOPSIS

```text
test_tool [server] [-s size] [-a] [-h]
```

## DESCRIPTION

`test_tool` is a lightweight verification utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched-Queue (MQ) messaging API over an OPA fabric link. It operates in a client/server model: one node runs as the **server** (listener) and the other as the **client** (initiator). The tool establishes a PSM2 endpoint pair, exchanges addressing information over a TCP socket, and then runs one or more functional tests against the live fabric path.

The tool ships three built-in tests. The **ping_pong** test performs 100 round-trip message exchanges at 64 bytes to confirm basic send/receive connectivity and measure per-iteration latency. The **data_integrity** test fills a buffer with a deterministic byte pattern (seeded with `0xCAFE`), transmits it to the peer, and verifies the received payload byte-by-byte on both sides — catching silent data corruption, truncation, or zero-fill errors. The **tag_match** test sends two messages concurrently using distinct PSM2 tag values (`0x40` and `0x41`) and confirms that each message is delivered exclusively to the receive buffer that posted the matching tag, validating the tag-selective matching logic in the PSM2 stack.

`test_tool` is intended for fabric bring-up, driver regression testing, and post-maintenance smoke checks. It reuses the shared `libpsm2` and `psm2perf` infrastructure from the parent benchmark suite, so it can be built alongside the existing latency and bandwidth tools with no additional dependencies beyond `libpsm2`.

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled by the `-a` flag and the positional `server` argument determines the role of the process.

| Positional Argument | Description |
|---|---|
| *(none)* | Run as the **server** (listener). The process waits for an incoming client connection. |
| `server` | Run as the **client**. Connect to the specified server hostname or IP address. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests (`ping_pong`, `data_integrity`, `tag_match`). When omitted, only `ping_pong` is executed. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., `--show-mqstats`) before `test_tool` parses its own flags. Consult the `psm2perf` documentation for the full set of inherited options.

### Test Parameters (Compile-Time Constants)

The following parameters are defined in `test_tool.h` and require recompilation to change:

| Constant | Value | Description |
|---|---|---|
| `PING_ITERS` | `100` | Number of round-trip iterations in the `ping_pong` test. |
| `TEST_MAX_MSG` | `1048576` (1 MiB) | Maximum payload size for `data_integrity`. The `-s` flag is clamped to this value. |
| `FILL_SEED` | `0xCAFE` | Deterministic seed for the fill/verify pattern used in `data_integrity`. |

## Shannon Commands

`test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework. This section is not applicable.

## API Endpoints

`test_tool` does not expose a REST API. It communicates peer-to-peer over a TCP control socket (port `SERVER_PORT + 1`) and PSM2 matched-queue operations. This section is not applicable.

## EXAMPLES

!!! warning "Prerequisites"
    Both nodes must have the OPA fabric driver loaded, `libpsm2` installed, and the OPA link active. The server process must be started **before** the client.

### Example 1 — Basic connectivity check (ping-pong only)

Start the server on `node-a`, then run the client on `node-b`:

=== "Server (node-a)"

    ```bash
    # On node-a: start as server (no positional argument)
    cd test_tool
    ./test_tool
    ```

=== "Client (node-b)"

    ```bash
    # On node-b: connect to the server
    cd test_tool
    ./test_tool node-a
    ```

Expected output on each node:

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 2 — Run all tests with default message size

```bash
# Client side — runs ping_pong, data_integrity (4096 B), and tag_match
./test_tool node-a -a
```

Expected output:

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1200.00 us  (100 iters, 12.00 us/iter)
  [PASS] data_integrity                      850.33 us  (4096 bytes verified)
  [PASS] tag_match                           245.10 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 3 — Data integrity test with a large message (64 KiB)

```bash
# Server
./test_tool -s 65536 -a

# Client (on the other node)
./test_tool node-a -s 65536 -a
```

!!! tip
    Both the server and client must specify the same `-s` value so that send and receive buffer sizes match.

### Example 4 — Data integrity test at maximum payload (1 MiB)

```bash
# Client — stress-test the full 1 MiB path
./test_tool node-a -s 1048576 -a
```

This exercises the PSM2 rendezvous (RNDV) protocol path, which is typically engaged for messages larger than the eager threshold.

### Example 5 — Build from source and run

```bash
# Clone the repository and build
git clone https://github.com/cornelisnetworks/opa-psm2-tests.git
cd opa-psm2-tests/test_tool
make

# Start server on node-a
ssh node-a "cd /path/to/opa-psm2-tests/test_tool && ./test_tool" &

# Run all tests from node-b
./test_tool node-a -a
echo "Exit code: $?"
```

### Example 6 — Scripted pass/fail gate in CI

```bash
#!/bin/bash
# ci_fabric_check.sh — fail the CI pipeline if any test fails

set -e

SERVER_HOST="${1:?Usage: $0 <server-hostname>}"

./test_tool "$SERVER_HOST" -s 8192 -a
rc=$?

if [ $rc -ne 0 ]; then
    echo "FABRIC CHECK FAILED" >&2
    exit 1
fi

echo "FABRIC CHECK PASSED"
exit 0
```

### Example 7 — Display PSM2 MQ statistics after tests

If the shared `psm2perf` infrastructure supports the `--show-mqstats` flag:

```bash
# Server
./test_tool --show-mqstats

# Client
./test_tool node-a -a --show-mqstats
```

This appends PSM2 internal matched-queue counters (posted receives, unexpected messages, etc.) after the test summary.

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | (auto) | Comma-separated list of PSM2 device names to use. When unset, PSM2 auto-selects. |
| `HFI_UNIT` | No | `0` | HFI unit number to open. Relevant on multi-HFI systems. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal debug tracing. Increase for verbose diagnostics. |
| `PSM2_MQ_RNDV_HFI_THRESH` | No | (library default) | Byte threshold above which PSM2 uses the rendezvous protocol instead of eager send. Affects `data_integrity` behavior at large `-s` values. |
| `PSM2_MQ_RNDV_SHM_THRESH` | No | (library default) | Rendezvous threshold for shared-memory (intra-node) transfers. |

!!! warning
    Environment variables prefixed with `PSM2_` or `HFI_` are consumed by the `libpsm2` library, not by `test_tool` directly. Refer to the [PSM2 Programmer's Guide](https://github.com/cornelisnetworks/opa-psm2) for the complete list.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header defining test constants, result structures, `fill_pattern()`, and `verify_pattern()` inline helpers. |
| `test_tool/Makefile` | Build rules. Depends on `../libpsm2.o` and `../psm2perf.o` from the parent directory. |
| `test_tool/README` | Plain-text quick-start notes. |
| `../libpsm2.h` | Shared PSM2 wrapper API header (endpoint init, send, recv, shutdown). |
| `../psm2perf.h` | Shared benchmark infrastructure header (`init_benchmark`, `open_socket`, `exchange_info`, timing macros). |
| `../libpsm2.o` | Compiled shared object providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, `print_psm2_stats()`. |
| `../psm2perf.o` | Compiled shared object providing `init_benchmark()`, `open_socket()`, `exchange_info()`, `TIMER()`, `ts_diff()`. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (`num_fail == 0`). |
| `1` | One or more tests failed, **or** a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! tip "Interpreting results"
    The tool prints a summary line before exiting:
    ```text
    # Summary: 3 passed, 0 failed, 0 skipped
    ```
    Parse this line in automation scripts to extract granular pass/fail/skip counts beyond the binary exit code.

## SEE ALSO

- [`opa-psm2`](https://github.com/cornelisnetworks/opa-psm2) — PSM2 user-space library for Omni-Path fabrics
- [`opa-psm2-tests`](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency and bandwidth benchmarks
- `libpsm2.h` / `psm2perf.h` — Shared infrastructure API reference
- `psm2_mq_isend(3)`, `psm2_mq_irecv(3)`, `psm2_mq_wait(3)` — PSM2 matched-queue API man pages
- [Cornelis Networks Documentation](https://www.cornelisnetworks.com) — OPA hardware and software documentation portal
```