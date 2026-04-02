---
title: "Test Tool User Guide"
date: "2026-04-02"
status: "draft"
---

# Test Tool User Guide

## NAME

**test_tool** — PSM2 connection test tool that verifies connectivity, data integrity, and tag-matching over the OPA fabric.

## SYNOPSIS

```
test_tool [server] [-s size] [-a] [-h]
```

## DESCRIPTION

`test_tool` is a client-server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched Queue (MQ) messaging API over Cornelis Omni-Path Architecture (OPA) fabrics. It provides a lightweight, deterministic test suite for validating that two nodes can exchange messages correctly, that payload data arrives without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool reuses the shared `libpsm2` and `psm2perf` infrastructure already present in the `opa-psm2-tests` benchmark suite, so it inherits the same endpoint initialization, socket-based out-of-band coordination, and PSM2 lifecycle management. One node runs as the **server** (listener) and the other as the **client** (connector). The server role is assumed when no positional hostname argument is provided.

Three tests are available: a **ping-pong** round-trip latency smoke test, a **data-integrity** verification test that fills a buffer with a deterministic byte pattern and confirms it survives a send/echo/verify cycle, and a **tag-match isolation** test that sends two messages with distinct tags concurrently and verifies each lands in the correct receive buffer. By default only the ping-pong test runs; pass '-a' to execute the full suite.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. Test selection is controlled through command-line options. The individual tests executed are:

| Test Name | Description |
|---|---|
| `ping_pong` | Performs 100 round-trip 64-byte message exchanges between server and client, measuring per-iteration latency. Always runs. |
| `data_integrity` | Sends a buffer filled with a deterministic pattern (seed `0xCAFE`), echoes it back, and verifies byte-for-byte correctness on both sides. Runs when '-a' is specified. |
| `tag_match` | Sends two 64-byte messages with different PSM2 tags (`0x40`, `0x41`) concurrently and verifies each is received into the correct buffer via tag-selective matching. Runs when '-a' is specified. |

## OPTIONS

### Positional Arguments

| Argument | Type | Default | Description |
|---|---|---|---|
| `server` | string | *(none — act as server)* | Hostname or IP address of the server node. When omitted, the process assumes the server (listener) role. When provided, the process acts as the client and connects to the specified server. |

### Test Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-s SIZE` | long | `4096` | Message size in bytes for the `data_integrity` test. Valid range is `1` to `1048576` (1 MiB). Values outside this range cause an immediate error exit. |
| `-a` | boolean | off | Run all tests. Without this flag, only the `ping_pong` test executes. With this flag, `data_integrity` and `tag_match` are also run. |
| `-h` | boolean | off | Print usage information to stderr and exit with code `0`. |

!!! tip
    The `-s` flag only affects the `data_integrity` test. The `ping_pong` and `tag_match` tests always use 64-byte messages.

!!! warning
    Options inherited from the shared `init_benchmark()` infrastructure (such as `--show-mqstats`) are parsed before `test_tool`'s own option processing. Refer to the `psm2perf` documentation for the full set of inherited options.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. It communicates between server and client using a direct TCP socket for out-of-band coordination and PSM2 MQ for data-plane messaging.

## EXAMPLES

### Example 1 — Start the server and wait for a client

On the server node, run `test_tool` with no positional argument. The process listens for an incoming client connection and then executes the default `ping_pong` test.

```bash
# On server node (e.g., opa-node01):
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

On the client node, specify the server hostname. Only the `ping_pong` test runs (100 iterations, 64-byte messages).

```bash
# On client node (e.g., opa-node02):
cd test_tool
./test_tool opa-node01
```

Expected output on both nodes:

```
# PSM2 Connection Test Tool
# opa-node02 — CLIENT
#
  [PASS] ping_pong                           532.40 us  (100 iters, 5.32 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run the full test suite with default message size

Use the '-a' flag to execute all three tests (`ping_pong`, `data_integrity`, `tag_match`) with the default 4096-byte data-integrity payload.

```bash
# Server:
./test_tool -a

# Client:
./test_tool opa-node01 -a
```

Expected output:

```
# PSM2 Connection Test Tool
# opa-node01 — SERVER
#
  [PASS] ping_pong                           548.12 us  (100 iters, 5.48 us/iter)
  [PASS] data_integrity                       18.73 us  (4096 bytes verified)
  [PASS] tag_match                            12.41 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 4 — Run data-integrity test with a large message size

Specify a 1 MiB payload to stress the data-integrity verification path across the full `TEST_MAX_MSG` range.

```bash
# Server:
./test_tool -a -s 1048576

# Client:
./test_tool opa-node01 -a -s 1048576
```

### Example 5 — Run data-integrity test with a custom 64 KiB message

Use '-s' to set a 65536-byte message for the `data_integrity` test while running the full suite.

```bash
# Server:
./test_tool -a -s 65536

# Client:
./test_tool opa-node01 -a -s 65536
```

### Example 6 — Build the tool from source

Compile `test_tool` and its shared dependencies from the repository root.

```bash
cd opa-psm2-tests/test_tool
make clean
make
```

The build produces the `test_tool` binary in the `test_tool/` directory. The shared objects `libpsm2.o` and `psm2perf.o` are built automatically in the parent directory via recursive make.

### Example 7 — Display help text

```bash
./test_tool -h
```

Output:

```
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

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | *(auto)* | Comma-separated list of PSM2 device names to use. Inherited from the PSM2 runtime. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple OPA adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(PSM2 default)* | Controls the rendezvous window size for large messages. Relevant when using '-s' with large payloads in the `data_integrity` test. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask enabling PSM2 internal tracing. Useful for debugging connectivity failures. |
| `PSM2_MQ_RECVREQS_MAX` | No | *(PSM2 default)* | Maximum number of pre-posted receive requests. May need tuning if tests hang under high concurrency. |

!!! tip
    If the tool fails during `libpsm2_init()`, set `PSM2_TRACEMASK=0xffff` to enable verbose PSM2 tracing and identify the root cause.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header file defining constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values, `FILL_SEED`), the `test_result` struct, inline `fill_pattern()`/`verify_pattern()` helpers, and function prototypes. |
| `test_tool/Makefile` | Build rules for compiling `test_tool` and linking against shared objects and `libpsm2`. |
| `test_tool/README` | Brief plain-text description of the tool, its tests, build instructions, and exit codes. |
| `libpsm2.h` | Shared header from the parent `opa-psm2-tests` repository providing PSM2 lifecycle wrappers (`libpsm2_init`, `libpsm2_shutdown`, `post_send`, `post_irecv`, etc.). |
| `psm2perf.h` | Shared header providing `benchmark_info`, `init_benchmark()`, `open_socket()`, `exchange_info()`, timing macros (`TIMER`, `ts_diff`), and `print_psm2_stats()`. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (no failures). |
| `1` | One or more tests failed, or a fatal initialization error occurred (e.g., `init_benchmark()` returned `NULL`, socket open failed, `exchange_info()` failed, `libpsm2_init()` failed, or invalid '-s' argument). |

!!! warning
    An exit code of `0` only guarantees that no test reported `TEST_FAIL`. Tests that were not selected (i.e., '-a' was not specified) are simply not run — they are not counted as skipped in the exit status logic.

## BREAKING CHANGES

This is the initial release of `test_tool`. There are no breaking changes at this time.

!!! tip
    The tool uses TCP port `SERVER_PORT + 1` for out-of-band coordination (where `SERVER_PORT` is defined in `psm2perf.h`). If the base port definition changes in the shared infrastructure, the coordination port used by `test_tool` will shift accordingly.

## SEE ALSO

- **opa-psm2-tests repository** — [github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing the shared `libpsm2` and `psm2perf` infrastructure, latency benchmarks, and bandwidth benchmarks.
- **libpsm2.h / psm2perf.h** — Shared headers documenting the PSM2 wrapper API (`post_send`, `post_irecv`, `psm2_mq_wait`, `libpsm2_init`, `libpsm2_shutdown`) and benchmark infrastructure (`init_benchmark`, `open_socket`, `exchange_info`, `TIMER`, `ts_diff`).
- **PSM2 Programmer's Guide** — Cornelis Networks documentation for the PSM2 Matched Queue API, covering endpoint management, tag matching semantics, and rendezvous protocol tuning.
- **opa-psm2** — [github.com/cornelisnetworks/opa-psm2](https://github.com/cornelisnetworks/opa-psm2) — The PSM2 userspace library that `test_tool` links against (`-lpsm2`).