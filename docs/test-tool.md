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

`test_tool` is a client/server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched-Queue (MQ) messaging API over an OPA fabric link. It is designed to validate that two nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool operates in a two-node model: one node runs as the **server** (listener) and the other as the **client** (initiator). The server is started without a positional argument; the client is started with the server's hostname. Both sides use the shared `libpsm2` and `psm2perf` infrastructure from the parent repository to initialize PSM2 endpoints, exchange addressing information over a TCP socket, and then perform the selected tests entirely over the OPA fabric.

Three built-in tests are provided. The **ping_pong** test measures round-trip latency over 100 iterations of 64-byte messages. The **data_integrity** test fills a buffer with a deterministic byte pattern, sends it to the peer, and verifies the received content byte-by-byte on both sides. The **tag_match** test sends two messages with distinct PSM2 tag values concurrently and confirms that each message is delivered to the correct receive buffer. By default only the ping-pong test runs; the `-a` flag enables the full suite.

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled through the `-a` option flag. The individual tests executed are:

| Test Name        | Description                                                                                      |
|------------------|--------------------------------------------------------------------------------------------------|
| `ping_pong`      | Round-trip message exchange using 64-byte messages over 100 iterations. Always runs.             |
| `data_integrity` | Send/receive with deterministic fill-pattern verification. Runs when `-a` is specified.          |
| `tag_match`      | Concurrent sends on two different tags with isolation verification. Runs when `-a` is specified. |

## OPTIONS

### Global Options

| Flag        | Type     | Default | Description                                                                                  |
|-------------|----------|---------|----------------------------------------------------------------------------------------------|
| `server`    | positional (string) | *(none — act as server)* | Hostname of the server node. Omit this argument to start in server mode. |
| `-s SIZE`   | integer  | `4096`  | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB).  |
| `-a`        | flag     | off     | Run all tests. Without this flag only the `ping_pong` test executes.                         |
| `-h`        | flag     | off     | Print usage information and exit.                                                            |

!!! tip
    The `-s` option only affects the `data_integrity` test. It has no effect unless `-a` is also specified.

### Inherited Options (from `init_benchmark`)

Options parsed by the shared `psm2perf` / `libpsm2` initialization layer (e.g., HFI unit selection, MQ statistics display) are also accepted. Refer to the `psm2perf` documentation for the full list.

## Shannon Commands

`test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework. This section is not applicable.

| Command | Method | Description |
|---------|--------|-------------|
| *(n/a)* | —      | `test_tool` does not register Shannon commands. |

## API Endpoints

`test_tool` does not expose a REST API. It communicates between server and client over a raw TCP socket (port `SERVER_PORT + 1`) for PSM2 endpoint information exchange, then uses PSM2 MQ operations for all test traffic.

| Method | Path  | Description |
|--------|-------|-------------|
| *(n/a)* | —    | `test_tool` does not expose HTTP endpoints. |

## EXAMPLES

### Example 1 — Start the server side

On the server node, launch `test_tool` with no positional argument. It will listen for an incoming client connection:

```bash
# On server node (e.g., opa-node01)
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

On the client node, specify the server hostname. Only the `ping_pong` test runs by default:

```bash
# On client node (e.g., opa-node02)
cd test_tool
./test_tool opa-node01
```

Expected output on both sides:

```text
# PSM2 Connection Test Tool
# opa-node02 — CLIENT
#
  [PASS] ping_pong                          234.50 us  (100 iters, 2.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run the full test suite

Use the `-a` flag to execute all three tests (ping-pong, data integrity, and tag match):

```bash
# Server
./test_tool

# Client
./test_tool opa-node01 -a
```

### Example 4 — Run with a custom data-integrity message size

Specify a 64 KiB payload for the data-integrity test:

```bash
# Server
./test_tool

# Client
./test_tool opa-node01 -s 65536 -a
```

### Example 5 — Run with the maximum 1 MiB payload

Stress the data path with the largest supported message size:

```bash
# Server
./test_tool

# Client
./test_tool opa-node01 -s 1048576 -a
```

### Example 6 — Display help text

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

### Example 7 — Scripted pass/fail gate in CI

Use the exit code to gate a CI pipeline:

```bash
# On the server (backgrounded)
./test_tool &
SERVER_PID=$!

# On the client (same node for loopback, or remote via ssh)
ssh opa-node02 "cd /path/to/test_tool && ./test_tool opa-node01 -a"
RC=$?

kill $SERVER_PID 2>/dev/null
if [ $RC -ne 0 ]; then
    echo "FATAL: PSM2 test suite failed"
    exit 1
fi
echo "All PSM2 tests passed"
```

## ENVIRONMENT

| Variable              | Required | Default          | Description                                                                                          |
|-----------------------|----------|------------------|------------------------------------------------------------------------------------------------------|
| `PSM2_DEVICES`        | No       | *(auto-detect)*  | Comma-separated list of PSM2 device types to use (e.g., `self,shm,hfi`).                            |
| `HFI_UNIT`            | No       | `0`              | HFI unit number to open. Relevant on multi-HFI systems.                                             |
| `PSM2_TRACEMASK`      | No       | `0x1`            | Bitmask controlling PSM2 internal tracing verbosity.                                                 |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No  | *(library default)* | Rendezvous window size for large messages. May affect `data_integrity` test with large `-s` values. |
| `PSM2_MTU`            | No       | *(fabric default)* | Override the path MTU used by PSM2.                                                                 |

!!! warning
    Setting `PSM2_DEVICES` incorrectly (e.g., omitting `hfi`) will prevent the tool from communicating over the OPA fabric and cause connection failures.

## FILES

| Path                          | Description                                                                                     |
|-------------------------------|-------------------------------------------------------------------------------------------------|
| `test_tool/test_tool.c`       | Main source file containing test implementations and CLI entry point.                           |
| `test_tool/test_tool.h`       | Header file defining test constants, tag values, result structures, and inline helper functions. |
| `test_tool/Makefile`          | Build rules for compiling `test_tool` and linking against `libpsm2` and `psm2perf`.             |
| `test_tool/README`            | Brief plain-text usage notes.                                                                   |
| `libpsm2.h` / `libpsm2.c`    | Shared PSM2 initialization, endpoint management, and MQ operation wrappers (parent directory).   |
| `psm2perf.h` / `psm2perf.c`  | Shared benchmark infrastructure: socket helpers, info exchange, timing macros (parent directory). |

## EXIT STATUS

| Code | Meaning                                                                                          |
|------|--------------------------------------------------------------------------------------------------|
| `0`  | All executed tests passed (`num_fail == 0`).                                                     |
| `1`  | One or more tests failed, or a fatal initialization error occurred (socket open, PSM2 init, info exchange, or invalid command-line argument). |

!!! tip
    The summary line printed at the end of execution (`# Summary: N passed, N failed, N skipped`) provides a human-readable breakdown. The exit code is the machine-readable equivalent: `0` for clean, `1` for any failure.

## SEE ALSO

- **Repository:** [cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests)
- **libpsm2 / psm2perf** — Shared PSM2 wrapper library and benchmark infrastructure used by `test_tool` and the latency/bandwidth benchmarks in the same repository.
- **PSM2 Programmer's Guide** — Intel/Cornelis PSM2 API reference for `psm2_mq_isend`, `psm2_mq_irecv`, `psm2_mq_wait`, and tag-matching semantics.
- **opa-psm2** — The [Cornelis PSM2 userspace library](https://github.com/cornelisnetworks/opa-psm2) that `test_tool` links against (`-lpsm2`).
- **opainfo(1)** — OPA fabric diagnostic utility for verifying link state before running `test_tool`.

---

*Copyright © 2026 Cornelis Networks. Dual-licensed under BSD and GPLv2.*
```