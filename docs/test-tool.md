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
test_tool [server] [-s SIZE] [-a] [-h]
```

## DESCRIPTION

`test_tool` is a client/server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched-Queue (MQ) messaging API over an OPA fabric link. It is designed to validate that two nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams onto their intended receive buffers.

The tool operates in a two-node model: one node runs as the **server** (listener) and the other as the **client** (initiator). Role selection is implicit — omitting the positional `server` hostname argument causes the process to assume the server role. The tool reuses the shared `libpsm2` and `psm2perf` infrastructure that underpins the latency and bandwidth benchmarks in the same repository, so it inherits their socket-based out-of-band connection setup and PSM2 endpoint initialization.

Three built-in tests are provided. The **ping_pong** test is always executed and serves as a basic smoke test for round-trip messaging. When the `-a` flag is supplied, two additional tests run: **data_integrity** sends a deterministic fill-pattern payload and verifies it byte-by-byte on both sides, and **tag_match** sends two messages with distinct tags concurrently to confirm that tag-selective receives land in the correct buffers. A summary line reports pass/fail/skip counts, and the process exit code reflects overall success or failure.

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled through the `-a` option flag. The individual tests executed internally are:

| Test Name        | Description                                                                                         |
|------------------|-----------------------------------------------------------------------------------------------------|
| `ping_pong`      | Round-trip message exchange using 64-byte messages over 100 iterations. Always runs.                |
| `data_integrity` | Sends a deterministic fill-pattern buffer and verifies byte-level correctness on both endpoints.    |
| `tag_match`      | Sends two concurrent messages with different PSM2 tags and verifies each arrives in the correct buffer. |

## OPTIONS

### Positional Arguments

| Argument | Type   | Default       | Description                                                                 |
|----------|--------|---------------|-----------------------------------------------------------------------------|
| `server` | string | *(none)*      | Hostname of the server node. Omit this argument to run as the server.       |

### Optional Flags

| Flag     | Type   | Default | Description                                                                                  |
|----------|--------|---------|----------------------------------------------------------------------------------------------|
| `-s SIZE`| long   | `4096`  | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB).  |
| `-a`     | bool   | off     | Run all tests. Without this flag, only `ping_pong` executes.                                 |
| `-h`     | bool   | off     | Print usage information and exit.                                                            |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., HFI unit selection, MQ statistics display). Consult the `psm2perf` documentation for the full set of inherited options.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node coordination uses a raw TCP socket on port `SERVER_PORT + 1` for out-of-band PSM2 endpoint information exchange only.

## EXAMPLES

### Example 1 — Start the server and wait for a client

Run on the node that will act as the server. The process listens for an incoming client connection:

```bash
# On node "opa-server-01":
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

Run on the client node, pointing at the server hostname. Only the `ping_pong` test executes:

```bash
# On node "opa-client-01":
./test_tool opa-server-01
```

Expected output on both nodes:

```text
# PSM2 Connection Test Tool
# opa-client-01 — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with the default 4 KiB data-integrity payload

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -a
```

This executes `ping_pong`, `data_integrity` (4096 bytes), and `tag_match` in sequence.

### Example 4 — Run all tests with a 64 KiB data-integrity payload

Use the `-s` flag to increase the data-integrity message size to 65536 bytes:

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -a -s 65536
```

### Example 5 — Run all tests with the maximum 1 MiB payload

Stress the data path with the largest supported message:

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -a -s 1048576
```

### Example 6 — Build from source

```bash
cd opa-psm2-tests/test_tool
make clean
make
```

!!! warning "Build dependency"
    The Makefile expects `libpsm2.o` and `psm2perf.o` in the parent directory. If they have not been built yet, `make` will invoke `make -C ..` to produce them. Ensure the `libpsm2-devel` package (providing `psm2.h` and `-lpsm2`) is installed on the build host.

### Example 7 — Scripted pass/fail gate in CI

```bash
#!/bin/bash
# Run on the client after the server is already listening.
./test_tool opa-server-01 -a -s 4096
rc=$?
if [ $rc -ne 0 ]; then
    echo "FATAL: PSM2 connectivity test failed" >&2
    exit 1
fi
echo "PSM2 fabric link verified."
```

## ENVIRONMENT

| Variable              | Required | Default          | Description                                                                                          |
|-----------------------|----------|------------------|------------------------------------------------------------------------------------------------------|
| `PSM2_DEVICES`        | No       | *(auto)*         | Comma-separated list of PSM2 device types to use (e.g., `self,shm,hfi`).                            |
| `HFI_UNIT`            | No       | `0`              | HFI unit number to open. Relevant on multi-HFI systems.                                             |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No  | *(library default)* | Rendezvous window size for large messages. May affect `data_integrity` at large `-s` values.      |
| `PSM2_TRACEMASK`      | No       | `0x1`            | PSM2 internal trace mask for debug output.                                                           |
| `PSM2_MULTI_EP`       | No       | `0`              | Set to `1` to enable multi-endpoint mode in PSM2.                                                    |

!!! tip "Debugging failures"
    Set `PSM2_TRACEMASK=0xffff` to enable verbose PSM2 library tracing when diagnosing connectivity or tag-matching failures.

## FILES

| Path                              | Description                                                                                     |
|-----------------------------------|-------------------------------------------------------------------------------------------------|
| `test_tool/test_tool.c`          | Main source file containing test implementations and CLI entry point.                           |
| `test_tool/test_tool.h`          | Header defining test constants, result structures, `fill_pattern()`, and `verify_pattern()`.    |
| `test_tool/Makefile`             | Build rules for `test_tool`. Depends on `../libpsm2.o` and `../psm2perf.o`.                    |
| `test_tool/README`               | Brief plain-text usage notes.                                                                   |
| `libpsm2.h` / `libpsm2.c`       | Shared PSM2 wrapper library (endpoint init, send/recv helpers, shutdown).                       |
| `psm2perf.h` / `psm2perf.c`     | Shared benchmark infrastructure (socket setup, info exchange, timing macros).                   |

## EXIT STATUS

| Code | Meaning                                                                                          |
|------|--------------------------------------------------------------------------------------------------|
| `0`  | All executed tests passed (`num_fail == 0`).                                                     |
| `1`  | One or more tests failed, **or** a fatal initialization error occurred (socket open, PSM2 init, info exchange, or invalid CLI arguments). |

!!! warning "Server-side exit code"
    The server process uses the same exit-code convention. A `data_integrity` failure detected on the server side (echo mismatch) will cause the server to exit with code `1` as well.

## SEE ALSO

- [`opa-psm2-tests` repository](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and shared infrastructure.
- `libpsm2(7)` — PSM2 Matched-Queue API reference (provided by the `libpsm2-devel` package).
- `psm2perf` — Shared performance benchmark infrastructure used by `test_tool`.
- Cornelis Networks OPA documentation — [www.cornelisnetworks.com](https://www.cornelisnetworks.com)

---

*Copyright © 2026 Cornelis Networks. Dual-licensed under BSD and GPLv2.*
```