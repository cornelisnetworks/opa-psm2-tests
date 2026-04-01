# Test Tool — Design Reference

```markdown
---
title: "test_tool — PSM2 Connection Test Tool"
description: "User guide and design reference for the PSM2 connection test tool in opa-psm2-tests"
---

# test_tool

## NAME

**test_tool** — PSM2 connection test tool that verifies connectivity, data integrity, and tag-matching over the OPA fabric.

## SYNOPSIS

```text
test_tool [server] [-s size] [-a] [-h]
```

## DESCRIPTION

`test_tool` is a client/server diagnostic utility that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that two nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool ships as part of the `opa-psm2-tests` repository and reuses the shared `libpsm2` and `psm2perf` infrastructure that underpins the latency and bandwidth benchmarks in the same tree. One node runs in **server** mode (no positional argument) while the other runs in **client** mode (passing the server hostname as a positional argument). A TCP socket is opened first to exchange PSM2 endpoint addresses, after which all test traffic flows over the native OPA fabric path.

Three built-in tests are provided: a **ping-pong** round-trip smoke test (always executed), a **data-integrity** verification test, and a **tag-match** isolation test. The latter two are gated behind the `-a` flag. Each test reports PASS, FAIL, or SKIP along with elapsed time and diagnostic detail. A summary line at the end aggregates the results, and the process exit code reflects overall pass/fail status — making `test_tool` suitable for scripted CI pipelines and fabric bring-up validation.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. Test selection is controlled through the `-a` option flag and the role is determined by the presence or absence of the positional `server` argument.

| Implicit Mode | Trigger | Description |
|---|---|---|
| Server | Omit the positional `server` argument | Listen for an incoming client connection and participate as the server side of each test. |
| Client | Provide a `server` hostname | Connect to the specified server node and participate as the client side of each test. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `server` | positional string | *(none — act as server)* | Hostname or IP address of the server node. When omitted, the process becomes the server. |
| `-s SIZE` | integer | `4096` | Message size in bytes for the data-integrity test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests. Without this flag only the `ping_pong` test executes. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., `--show-mqstats`). Refer to the `psm2perf` documentation for the full set of inherited flags.

### Built-in Test Matrix

| Test Name | Tag Values Used | Default Payload | Description |
|---|---|---|---|
| `ping_pong` | `0x10` / `0x11` | 64 B × 100 iterations | Round-trip latency smoke test. Always runs. |
| `data_integrity` | `0x20` / `0x30` | Configurable via `-s` | Deterministic fill-pattern send, receive, verify, and echo-back. |
| `tag_match` | `0x40` / `0x41` | 64 B × 2 streams | Concurrent sends on distinct tags; verifies each lands in the correct buffer. |

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node coordination uses a private TCP socket on port `SERVER_PORT + 1` (defined in `psm2perf.h`) solely for PSM2 endpoint address exchange.

## EXAMPLES

!!! warning "Prerequisites"
    Both nodes must have the `opa-psm2` library installed, an active OPA fabric link, and the `hfi1` kernel driver loaded. The server process must be started **before** the client.

### Example 1 — Run the default ping-pong test

Start the server on `node-a`, then the client on `node-b`:

=== "Server (node-a)"

    ```bash
    # On node-a: start in server mode (no positional argument)
    ./test_tool
    ```

=== "Client (node-b)"

    ```bash
    # On node-b: connect to the server
    ./test_tool node-a
    ```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 2 — Run all tests with the default 4 KiB data-integrity payload

```bash
# Server
./test_tool

# Client
./test_tool node-a -a
```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
  [PASS] data_integrity                      456.78 us  (4096 bytes verified)
  [PASS] tag_match                           234.56 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with a 64 KiB data-integrity payload

```bash
# Server
./test_tool

# Client — use -s to set the data-integrity message size
./test_tool node-a -a -s 65536
```

### Example 4 — Run all tests with the maximum 1 MiB payload

```bash
# Server
./test_tool

# Client — maximum supported message size
./test_tool node-a -a -s 1048576
```

### Example 5 — Display usage information

```bash
./test_tool -h
```

Output:

```text
usage: ./test_tool [server] [-s size] [-a] [-h]

PSM2 connection test tool — verifies connectivity and data
integrity over the OPA fabric using the PSM2 messaging API.

positional:
  server        hostname of the server node (omit to be server)

options:
  -s SIZE       message size for data-integrity test (default 4096)
  -a            run all tests (default: ping-pong only)
  -h            show this help
```

### Example 6 — Scripted CI gate using the exit code

```bash
#!/bin/bash
# run_fabric_check.sh — exits non-zero on any test failure

SERVER_HOST="$1"

./test_tool "$SERVER_HOST" -a -s 8192
rc=$?

if [ $rc -ne 0 ]; then
    echo "FABRIC CHECK FAILED" >&2
    exit 1
fi

echo "Fabric check passed."
```

### Example 7 — Build from source and run

```bash
# Clone the repository
git clone https://github.com/cornelisnetworks/opa-psm2-tests.git
cd opa-psm2-tests/test_tool

# Build (requires libpsm2 headers and library)
make

# Run server on local node
./test_tool &

# Run client against localhost (loopback test)
./test_tool localhost -a
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | `self,shm,hfi` | Comma-separated list of PSM2 device types to use. Useful for forcing loopback (`self,shm`) during development. |
| `HFI_UNIT` | No | `0` | Selects the HFI adapter unit number when multiple adapters are present. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal debug tracing. Increase for verbose diagnostics. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | `131072` | Rendezvous window size in bytes. May affect large-message data-integrity test performance. |
| `PSM2_MQ_RECVREQS_MAX` | No | `1048576` | Maximum number of pre-posted receive requests. |

!!! tip "Loopback testing"
    To test on a single node without a fabric peer, set `PSM2_DEVICES=self,shm` and use `localhost` as the server hostname.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header defining constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values), the `test_result` struct, and inline `fill_pattern` / `verify_pattern` helpers. |
| `test_tool/Makefile` | Build rules. Compiles `test_tool.c` and links against shared objects `libpsm2.o`, `psm2perf.o`, and the system `libpsm2` library. |
| `test_tool/README` | Brief plain-text usage notes shipped with the source. |
| `libpsm2.h` / `libpsm2.c` | Shared PSM2 initialization, shutdown, send/recv wrappers used by all tools in the repository. |
| `psm2perf.h` / `psm2perf.c` | Shared benchmark infrastructure: `init_benchmark()`, `open_socket()`, `exchange_info()`, timer macros, and statistics printing. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (`num_fail == 0`). |
| `1` | One or more tests failed, **or** a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! warning "Partial execution"
    If the tool exits with code `1` due to an initialization error (e.g., socket failure), no test results are printed. Check `stderr` for diagnostic messages in this case.

## SEE ALSO

- [`opa-psm2`](https://github.com/cornelisnetworks/opa-psm2) — The PSM2 user-space library that `test_tool` exercises.
- [`opa-psm2-tests`](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and this test tool.
- `psm2perf` — Shared benchmark infrastructure (socket exchange, timer helpers, MQ statistics).
- `libpsm2` — Shared PSM2 wrapper layer (`libpsm2_init`, `post_send`, `post_irecv`, `libpsm2_shutdown`).
- `psm2_mq_isend(3)`, `psm2_mq_irecv(3)`, `psm2_mq_wait(3)` — PSM2 Matched Queue API man pages.

---

*Copyright © 2026 Cornelis Networks. Dual licensed under BSD and GPLv2.*
```