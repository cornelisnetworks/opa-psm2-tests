# Test Tool — Design Reference

```markdown
---
title: "test_tool — PSM2 Connection Test Tool"
description: "User guide and design reference for the PSM2 connection test tool in opa-psm2-tests"
---

# test_tool

## NAME

**test_tool** — PSM2 connection test tool that verifies connectivity, data integrity, and tag-matching isolation over the OPA fabric.

## SYNOPSIS

```text
test_tool [server] [-s size] [-a] [-h]
```

## DESCRIPTION

`test_tool` is a client/server diagnostic utility that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that a pair of nodes can successfully exchange messages, that payload data arrives without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams onto distinct receive buffers.

The tool is built on the same `libpsm2` and `psm2perf` shared infrastructure used by the latency and bandwidth benchmarks in the `opa-psm2-tests` repository. It reuses the `init_benchmark()`, `open_socket()`, `exchange_info()`, and `libpsm2_init()` helpers for endpoint setup and out-of-band coordination, so the operational model — one side runs as a server, the other as a client that names the server — is identical to the existing performance tools.

Three tests are provided. The **ping_pong** test is always executed and serves as a basic connectivity smoke test. The **data_integrity** and **tag_match** tests are enabled with the `-a` flag and perform deeper verification. All results are printed in a structured `[PASS]`/`[FAIL]`/`[SKIP]` format with per-test timing and a final summary line, making the output suitable for both interactive use and automated CI pipelines.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. Test selection is controlled through the `-a` option flag. The individual tests executed are:

| Test | Description |
|------|-------------|
| `ping_pong` | Round-trip message exchange using 64-byte messages over 100 iterations. Measures per-iteration latency. Always runs. |
| `data_integrity` | Sends a deterministic fill-pattern payload from server to client, client verifies byte-by-byte, echoes back, and server re-verifies. Runs with `-a`. |
| `tag_match` | Sends two concurrent messages with distinct PSM2 tags (`TAG_MULTI_A`, `TAG_MULTI_B`), verifies each lands in the correct receive buffer via tag-selective matching. Runs with `-a`. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `server` | positional string | *(none — act as server)* | Hostname or IP address of the server node. Omit this argument to run as the server side. |
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean flag | off | Run all tests. Without this flag only the `ping_pong` test executes. |
| `-h` | boolean flag | off | Print usage information and exit. |

!!! tip
    The `-s` flag only affects the `data_integrity` test. The `ping_pong` and `tag_match` tests always use 64-byte messages.

!!! warning
    The positional `server` argument is parsed by `init_benchmark()` before `getopt` processes the option flags. Place the hostname **before** any dash-prefixed options to avoid ambiguity.

### Inherited Options

Options inherited from the shared `psm2perf` / `init_benchmark()` infrastructure (e.g., `--show-mqstats`) are also accepted. When `info->show_mqstats` is set, PSM2 internal MQ statistics are printed after the test summary via `print_psm2_stats()`.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node coordination uses a raw TCP socket on port `SERVER_PORT + 1` for out-of-band endpoint information exchange, followed by PSM2 MQ operations for the actual test traffic.

## EXAMPLES

### Example 1 — Start the server side

Run `test_tool` with no positional argument to act as the server. The process will listen for an incoming client connection:

```bash
# On node "opa-server-01":
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

Connect to the server and execute only the `ping_pong` connectivity smoke test (the default):

```bash
# On node "opa-client-01":
./test_tool opa-server-01
```

Expected output:

```text
# PSM2 Connection Test Tool
# opa-client-01 — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with the default 4 KiB data-integrity payload

Enable the full test suite with `-a`:

```bash
# Server side:
./test_tool

# Client side:
./test_tool opa-server-01 -a
```

### Example 4 — Run all tests with a 64 KiB data-integrity payload

Use `-s` to increase the data-integrity message size to 65536 bytes:

```bash
# Server side:
./test_tool

# Client side:
./test_tool opa-server-01 -s 65536 -a
```

### Example 5 — Run all tests with the maximum 1 MiB payload

Stress the data path with the largest supported message:

```bash
# Server side:
./test_tool

# Client side:
./test_tool opa-server-01 -s 1048576 -a
```

### Example 6 — Build from source

Compile `test_tool` and its shared dependencies from the repository root:

```bash
cd opa-psm2-tests/test_tool
make clean
make
```

### Example 7 — Scripted pass/fail check in CI

Use the exit code to gate a CI pipeline:

```bash
#!/bin/bash
set -e

# Start server in background on the local node
./test_tool &
SERVER_PID=$!
sleep 2

# Run client against localhost with all tests
./test_tool localhost -s 8192 -a
RC=$?

wait $SERVER_PID
exit $RC
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `PSM2_DEVICES` | No | *(auto)* | Colon-separated list of PSM2 device names to use. Inherited by `libpsm2_init()`. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask controlling PSM2 internal debug tracing. Useful for diagnosing connection failures. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple OPA adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Controls the rendezvous window size for large messages. May affect `data_integrity` test performance at 1 MiB. |
| `PSM2_MTU` | No | *(fabric default)* | Override the maximum transfer unit. Must not exceed the fabric-configured MTU. |

!!! tip
    Set `PSM2_TRACEMASK=0xffff` to enable verbose PSM2 debug output when troubleshooting connection establishment failures.

## FILES

| Path | Description |
|------|-------------|
| `test_tool/test_tool.c` | Main source file containing test implementations (`run_ping_pong`, `run_data_integrity`, `run_tag_match`) and the CLI entry point. |
| `test_tool/test_tool.h` | Header defining test constants (`TEST_MAX_MSG`, `PING_ITERS`, `FILL_SEED`), tag values, the `test_result` struct, and inline `fill_pattern()` / `verify_pattern()` helpers. |
| `test_tool/Makefile` | Build rules for `test_tool`. Links against `libpsm2.o`, `psm2perf.o`, and `-lpsm2`. |
| `test_tool/README` | Plain-text quick-start reference. |
| `libpsm2.h` / `libpsm2.o` | Shared PSM2 wrapper library providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, and `print_psm2_stats()`. |
| `psm2perf.h` / `psm2perf.o` | Shared benchmark infrastructure providing `init_benchmark()`, `open_socket()`, `exchange_info()`, `TIMER()`, and `ts_diff()`. |

## EXIT STATUS

| Code | Meaning |
|------|---------|
| `0` | All executed tests passed (`num_fail == 0`). |
| `1` | One or more tests failed, **or** a fatal initialization error occurred (benchmark init failure, socket open failure, info exchange failure, or PSM2 init failure). |

!!! warning
    An exit code of `1` is used for both test failures and infrastructure errors. Inspect the standard output for the `# Summary:` line to distinguish between the two cases. If no summary line is printed, the failure occurred during setup before any tests ran.

## SEE ALSO

- **opa-psm2-tests repository** — [github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests)
- **libpsm2** (`libpsm2.h`) — Shared PSM2 endpoint management and MQ operation wrappers used by all tools in the repository.
- **psm2perf** (`psm2perf.h`) — Shared benchmark scaffolding: socket coordination, timing macros, and `benchmark_info` lifecycle.
- **PSM2 Programmer's Guide** — Cornelis Networks documentation for the PSM2 Matched Queue API (`psm2_mq_isend`, `psm2_mq_irecv`, `psm2_mq_wait`).
- **opa-psm2** — [github.com/cornelisnetworks/opa-psm2](https://github.com/cornelisnetworks/opa-psm2) — The PSM2 userspace library that `test_tool` links against (`-lpsm2`).

---

*Copyright © 2026 Cornelis Networks. Dual-licensed under BSD and GPLv2.*
```