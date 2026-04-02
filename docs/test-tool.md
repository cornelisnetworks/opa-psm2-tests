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

`test_tool` is a client/server diagnostic utility that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that two nodes can establish a PSM2 connection, exchange messages correctly, and that the PSM2 tag-matching logic delivers payloads to the correct receive buffers. The tool is part of the `opa-psm2-tests` repository and shares the `libpsm2` and `psm2perf` infrastructure used by the latency and bandwidth benchmarks.

The tool ships three built-in tests. The **ping_pong** test performs 100 round-trip message exchanges with 64-byte payloads and reports per-iteration latency. The **data_integrity** test fills a buffer with a deterministic byte pattern, transmits it to the peer, and verifies the received content byte-by-byte on both sides (client verifies the initial receive; server verifies the echoed copy). The **tag_match** test sends two messages concurrently using distinct PSM2 tag values and confirms that each message is delivered exclusively to the receive buffer posted with the matching tag.

Use `test_tool` as a first-line smoke test after deploying or upgrading OPA fabric firmware, PSM2 libraries, or kernel drivers. It is also useful for isolating data-corruption issues that may surface under specific message sizes or tag configurations. By default only the ping-pong test runs; pass `-a` to execute the full suite.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. The role of each process instance (server or client) is determined by the presence or absence of the positional `server` argument:

| Invocation Pattern | Role | Description |
|---|---|---|
| `test_tool` | Server | Listens for an incoming client connection on `SERVER_PORT + 1` and acts as the server side of every test. |
| `test_tool <hostname>` | Client | Connects to the specified server hostname and acts as the client side of every test. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `server` (positional) | string | *(none — run as server)* | Hostname or IP address of the server node. Omit this argument to start in server mode. |
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | flag | off | Run **all** tests. Without this flag only the `ping_pong` test executes. |
| `-h` | flag | off | Print the usage summary and exit. |

### Inherited Options (from `init_benchmark`)

The following options are parsed by the shared `psm2perf` / `libpsm2` infrastructure before `test_tool` processes its own flags:

| Flag | Type | Default | Description |
|---|---|---|---|
| `--show-mqstats` | flag | off | Print PSM2 MQ statistics at the end of the run (calls `print_psm2_stats()`). |

!!! tip
    The `-s` flag only affects the `data_integrity` test. The `ping_pong` and `tag_match` tests always use 64-byte messages.

!!! warning
    The message size is clamped to `TEST_MAX_MSG` (1 MiB). If a value larger than 1048576 is supplied, the tool prints an error and exits with code `1`.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node communication uses a TCP socket for out-of-band setup (`open_socket`, `exchange_info`) and PSM2 MQ operations for the actual test traffic.

## EXAMPLES

### Example 1 — Start the server and run the default ping-pong test

On the **server** node (e.g., `node-a`):

```bash
# Start test_tool in server mode — waits for a client connection
./test_tool
```

On the **client** node:

```bash
# Connect to the server and run the default ping-pong test
./test_tool node-a
```

### Example 2 — Run the full test suite

```bash
# Server side
./test_tool

# Client side — run ping_pong, data_integrity, and tag_match
./test_tool node-a -a
```

### Example 3 — Run all tests with a custom data-integrity message size of 64 KiB

```bash
# Server side
./test_tool

# Client side
./test_tool node-a -s 65536 -a
```

### Example 4 — Run all tests with the maximum 1 MiB payload

```bash
# Server side
./test_tool

# Client side — stress the data path with a 1 MiB integrity check
./test_tool node-a -s 1048576 -a
```

### Example 5 — Run all tests and display PSM2 MQ statistics

```bash
# Server side (with MQ stats)
./test_tool --show-mqstats

# Client side (with MQ stats)
./test_tool node-a -a --show-mqstats
```

### Example 6 — Quick connectivity check in a CI pipeline

```bash
#!/bin/bash
# ci_fabric_check.sh — exits non-zero on any test failure

SERVER_HOST="${1:?usage: $0 <server-hostname>}"

./test_tool "$SERVER_HOST" -a -s 4096
exit_code=$?

if [ "$exit_code" -ne 0 ]; then
    echo "FABRIC CHECK FAILED" >&2
fi
exit "$exit_code"
```

### Example 7 — Display the help message

```bash
./test_tool -h
```

Expected output:

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

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | *(auto)* | Comma-separated list of PSM2 device names to use. Inherited by the `libpsm2` initialization layer. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple OPA adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Controls the rendezvous window size for large messages. Relevant when testing with `-s` values above the eager/rendezvous threshold. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask enabling PSM2 internal tracing. Useful for debugging connection or tag-matching issues. |
| `PSM2_MQ_RECVREQS_MAX` | No | *(library default)* | Maximum number of pre-posted receive requests. May need tuning if tests stall under high concurrency. |

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations (`run_ping_pong`, `run_data_integrity`, `run_tag_match`) and the CLI entry point. |
| `test_tool/test_tool.h` | Header defining test constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values), the `test_result` structure, and inline helper functions (`fill_pattern`, `verify_pattern`). |
| `test_tool/Makefile` | Build rules for the `test_tool` binary. Links against `libpsm2.o`, `psm2perf.o`, and `-lpsm2`. |
| `test_tool/README` | Plain-text quick-start reference. |
| `libpsm2.h` / `libpsm2.o` | Shared PSM2 wrapper library providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, and related functions. |
| `psm2perf.h` / `psm2perf.o` | Shared benchmark infrastructure providing `init_benchmark()`, `open_socket()`, `exchange_info()`, timer macros, and `print_psm2_stats()`. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed. |
| `1` | One or more tests failed, or a fatal initialization error occurred (e.g., invalid `-s` value, socket failure, PSM2 initialization failure, `init_benchmark` returned `NULL`). |

!!! note "Interpreting results"
    The exit code reflects only pass/fail status. Skipped tests do not cause a non-zero exit. Inspect the `# Summary:` line in stdout for the full breakdown of passed, failed, and skipped counts.

## BUILT-IN TESTS REFERENCE

### ping_pong

| Property | Value |
|---|---|
| **Tag (send)** | `0x10` (`TAG_PING`) |
| **Tag (reply)** | `0x11` (`TAG_PONG`) |
| **Message size** | 64 bytes |
| **Iterations** | 100 (`PING_ITERS`) |
| **Pass criteria** | All 100 round-trips complete without PSM2 error. |

The server sends a 64-byte message tagged `TAG_PING`, then posts a receive for `TAG_PONG` and waits. The client posts a receive for `TAG_PING`, waits, then replies with `TAG_PONG`. This repeats for `PING_ITERS` iterations. The reported elapsed time covers all iterations; per-iteration latency is printed in the detail field.

### data_integrity

| Property | Value |
|---|---|
| **Tag (payload)** | `0x20` (`TAG_DATA`) |
| **Tag (echo)** | `0x30` (`TAG_VERIFY`) |
| **Message size** | Configurable via `-s` (default 4096, max 1 MiB) |
| **Fill seed** | `0xCAFE` (`FILL_SEED`) |
| **Pass criteria** | Every byte in the received buffer matches `(FILL_SEED + offset) & 0xFF` on both client and server. |

The server fills a buffer with a deterministic pattern seeded by `FILL_SEED` and sends it to the client using `TAG_DATA`. The client receives the buffer, verifies every byte, and echoes it back using `TAG_VERIFY`. The server then verifies the echoed copy. A mismatch on either side results in `TEST_FAIL` with the offset and expected/actual byte values reported.

### tag_match

| Property | Value |
|---|---|
| **Tag A** | `0x40` (`TAG_MULTI_A`) |
| **Tag B** | `0x41` (`TAG_MULTI_B`) |
| **Message size** | 64 bytes per tag (128 bytes total) |
| **Pass criteria** | Each tag's payload arrives in the correct receive buffer with no cross-contamination. |

The server sends two 64-byte messages concurrently — one tagged `TAG_MULTI_A` (filled with seed `0xAA`) and one tagged `TAG_MULTI_B` (filled with seed `0xBB`). The client posts two tag-selective receives and echoes each buffer back on its original tag. The server verifies that the echoed data for each tag matches the original fill pattern, confirming that PSM2 tag-selective matching delivered each message to the correct buffer.

## BUILD

```bash
cd test_tool
make
```

The `Makefile` compiles `test_tool.c` and links it with the shared objects `../libpsm2.o` and `../psm2perf.o`, plus the system PSM2 library (`-lpsm2`). If the shared objects have not been built yet, `make` will recurse into the parent directory to build them.

```bash
# Clean build artifacts
make clean
```

## SEE ALSO

- [`opa-psm2-tests` repository](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and shared infrastructure.
- `libpsm2.h` — PSM2 wrapper API used by all tools in the repository.
- `psm2perf.h` — Benchmark infrastructure (socket setup, timer macros, statistics).
- PSM2 Programmer's Guide — Cornelis Networks documentation for the PSM2 API (`psm2_mq_isend`, `psm2_mq_irecv`, `psm2_mq_wait`).
- `psm2_latency` / `psm2_bw` — Companion performance benchmarks in the same repository.
```