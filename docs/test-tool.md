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

`test_tool` is a client/server diagnostic utility that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that a pair of nodes can successfully exchange messages, that payload data arrives without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool is built on the same `libpsm2` and `psm2perf` shared infrastructure used by the latency and bandwidth benchmarks in the `opa-psm2-tests` repository. One node runs as the **server** (by omitting the positional hostname argument) and the other runs as the **client** (by specifying the server's hostname). The two sides coordinate over a TCP socket to exchange PSM2 endpoint addresses, then run the selected test suite over the native OPA fabric path.

Three tests are available: a **ping-pong** round-trip smoke test (always executed), a **data-integrity** verification test that fills buffers with a deterministic pattern and checks for byte-level corruption on both sides, and a **tag-match** isolation test that sends two concurrent messages on distinct tags and verifies each arrives in the correct receive buffer. By default only the ping-pong test runs; pass `-a` to execute the full suite.

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled through the `-a` flag and the positional `server` argument determines the role (server or client).

| Positional Argument | Description |
|---------------------|-------------|
| *(none)* | Run as the **server** node — listen for an incoming client connection. |
| `server` | Run as the **client** node — connect to the specified server hostname. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `-s SIZE` | `long` | `4096` | Message size in bytes for the data-integrity test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests (ping-pong, data-integrity, tag-match). Without this flag only the ping-pong test executes. |
| `-h` | boolean | — | Print usage information and exit. |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., `--show-mqstats`) before `test_tool` parses its own flags. Consult the `psm2perf` documentation for the full set of inherited options.

### Test Parameters (compile-time)

The following parameters are defined as compile-time constants in `test_tool.h` and cannot be changed at runtime without rebuilding:

| Constant | Value | Description |
|----------|-------|-------------|
| `PING_ITERS` | `100` | Number of round-trip iterations in the ping-pong test. |
| `TEST_MAX_MSG` | `1048576` (1 MiB) | Maximum payload size for data-integrity tests. |
| `FILL_SEED` | `0xCAFE` | Deterministic seed used to generate and verify fill patterns. |

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node coordination uses a raw TCP socket on port `SERVER_PORT + 1` (defined in `psm2perf.h`).

## EXAMPLES

### Example 1 — Run the default ping-pong test (server side)

Start the server on the first node. It will listen for an incoming client connection:

```bash
# On node-a (server):
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test (client side)

On the second node, specify the server hostname to connect and execute the ping-pong round-trip test:

```bash
# On node-b (client):
cd test_tool
./test_tool node-a
```

Expected output on both sides:

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run the full test suite with default message size

Use `-a` to execute all three tests (ping-pong, data-integrity at 4096 bytes, and tag-match):

```bash
# Server:
./test_tool -a

# Client:
./test_tool node-a -a
```

### Example 4 — Run the full test suite with a custom message size

Specify a 64 KiB payload for the data-integrity test:

```bash
# Server:
./test_tool -a -s 65536

# Client:
./test_tool node-a -a -s 65536
```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1200.00 us  (100 iters, 12.00 us/iter)
  [PASS] data_integrity                     3456.78 us  (65536 bytes verified)
  [PASS] tag_match                           234.56 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 5 — Run with the maximum 1 MiB data-integrity payload

Stress the data path with the largest supported message:

```bash
# Server:
./test_tool -a -s 1048576

# Client:
./test_tool node-a -a -s 1048576
```

### Example 6 — Build from source

Compile the test tool and its shared dependencies:

```bash
cd opa-psm2-tests/test_tool
make clean
make
```

!!! warning "Build dependency"
    The `Makefile` expects `libpsm2.o` and `psm2perf.o` to be available in the parent directory. If they have not been built yet, `make` will invoke `make -C ..` to compile them automatically. Ensure the PSM2 development headers and `libpsm2` shared library are installed on the build host.

### Example 7 — Display help

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

## ENVIRONMENT

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `PSM2_DEVICES` | No | (auto) | Comma-separated list of PSM2 device names to use. Inherited by `libpsm2_init()`. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask controlling PSM2 internal tracing output. Useful for debugging connection failures. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple OPA adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | (library default) | Controls the rendezvous window size for large messages. May affect data-integrity test performance at 1 MiB. |
| `PSM2_MQ_RECVREQS_MAX` | No | (library default) | Maximum number of pre-posted receive requests. Relevant when running the tag-match test with concurrent receives. |

!!! tip "Debugging connection issues"
    If the client cannot connect to the server, verify that both nodes can reach each other on the TCP control port (`SERVER_PORT + 1`) and that the OPA fabric is operational. Setting `PSM2_TRACEMASK=0xffff` can help diagnose PSM2-level endpoint exchange failures.

## FILES

| Path | Description |
|------|-------------|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header file defining constants, tag values, result structures, and inline pattern-fill/verify helpers. |
| `test_tool/Makefile` | Build rules for compiling `test_tool` and linking against shared objects. |
| `test_tool/README` | Brief plain-text usage notes. |
| `libpsm2.h` | Shared PSM2 wrapper API header (parent directory). |
| `psm2perf.h` | Shared benchmark infrastructure header (parent directory). |
| `libpsm2.o` | Compiled PSM2 wrapper object linked into the final binary. |
| `psm2perf.o` | Compiled benchmark infrastructure object linked into the final binary. |

## EXIT STATUS

| Code | Meaning |
|------|---------|
| `0` | All executed tests passed. |
| `1` | One or more tests failed, or a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! warning "Partial execution"
    If the tool exits with code `1` due to an initialization error (e.g., socket failure or PSM2 endpoint exchange failure), no test results will be printed. The summary line is only emitted after at least one test has been attempted.

## SEE ALSO

- **opa-psm2-tests repository**: [github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and the shared `libpsm2`/`psm2perf` infrastructure.
- **libpsm2**: PSM2 wrapper library providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, and `print_psm2_stats()`.
- **psm2perf**: Shared benchmark framework providing `init_benchmark()`, `open_socket()`, `exchange_info()`, timer macros (`TIMER`, `ts_diff`), and the `benchmark_info` structure.
- **psm2(7)**: PSM2 library man page documenting the underlying `psm2_mq_*` matched-queue API.
- **opa-fm**: Omni-Path Fabric Manager — required for fabric-level routing and subnet management.

---

## Architecture Notes

### Test Descriptions

#### ping_pong

The ping-pong test performs `PING_ITERS` (100) round-trip message exchanges using 64-byte payloads. The server sends a message tagged `TAG_PING` (`0x10`) and waits for a reply tagged `TAG_PONG` (`0x11`); the client does the inverse. This test validates basic PSM2 MQ send/receive connectivity and reports per-iteration latency.

#### data_integrity

The data-integrity test fills a send buffer with a deterministic byte pattern generated from `FILL_SEED` (`0xCAFE`): each byte at offset `i` is set to `(seed + i) & 0xFF`. The server sends the buffer to the client using `TAG_DATA` (`0x20`). The client verifies every byte, then echoes the buffer back using `TAG_VERIFY` (`0x30`). The server verifies the echoed data. Any single-byte mismatch causes a `FAIL` result with the offset and expected/actual values reported.

#### tag_match

The tag-match test sends two 64-byte messages concurrently on distinct tags (`TAG_MULTI_A` = `0x40`, `TAG_MULTI_B` = `0x41`), each filled with a different pattern seed (`0xAA` and `0xBB`). The receiver posts two tag-selective `irecv` operations and verifies that each message lands in the correct buffer. This validates that PSM2's tag-matching logic correctly demultiplexes interleaved message streams.

### Internal Tag Map

| Tag Constant | Value | Used By |
|-------------|-------|---------|
| `TAG_PING` | `0x10` | ping_pong (server → client) |
| `TAG_PONG` | `0x11` | ping_pong (client → server) |
| `TAG_DATA` | `0x20` | data_integrity (server → client payload) |
| `TAG_VERIFY` | `0x30` | data_integrity (client → server echo) |
| `TAG_MULTI_A` | `0x40` | tag_match (first concurrent stream) |
| `TAG_MULTI_B` | `0x41` | tag_match (second concurrent stream) |

!!! tip "Tag isolation from benchmarks"
    All test tags are deliberately chosen to be distinct from the default tag `0xF` used by the performance benchmarks, ensuring that `test_tool` can coexist on the same fabric without interfering with concurrent benchmark runs.
```