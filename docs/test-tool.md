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

`test_tool` is a client/server diagnostic utility that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that two nodes can exchange messages correctly, that payload data survives transit without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool ships as part of the `opa-psm2-tests` repository and reuses the shared `libpsm2` and `psm2perf` infrastructure that underpins the latency and bandwidth benchmarks in the same tree. It operates in a two-node model: one process runs as the **server** (listener) and the other as the **client** (initiator). The server side is selected by omitting the positional `server` hostname argument; the client side is selected by providing it.

Three built-in tests are available. By default only the lightweight **ping_pong** round-trip test executes, making `test_tool` suitable as a quick smoke test after fabric bring-up. Passing the `-a` flag adds the **data_integrity** and **tag_match** tests for deeper verification. All results are printed in a structured, human-readable report with per-test pass/fail status, elapsed time, and diagnostic detail.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. Test selection is controlled through the `-a` option flag. The individual tests executed are:

| Test Name        | Description                                                                                         |
| :--------------- | :-------------------------------------------------------------------------------------------------- |
| `ping_pong`      | Sends 100 round-trip 64-byte messages between server and client, measuring per-iteration latency.   |
| `data_integrity` | Sends a deterministic fill-pattern payload, receives it back, and verifies every byte on both sides. |
| `tag_match`      | Sends two concurrent messages with distinct PSM2 tags and verifies each lands in the correct buffer. |

## OPTIONS

### Positional Arguments

| Argument | Type   | Default       | Description                                                                 |
| :------- | :----- | :------------ | :-------------------------------------------------------------------------- |
| `server` | string | *(none)*      | Hostname of the server node. Omit this argument to run as the server side.  |

### Optional Flags

| Flag     | Type   | Default | Description                                                                                  |
| :------- | :----- | :------ | :------------------------------------------------------------------------------------------- |
| `-s SIZE`| long   | `4096`  | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB).  |
| `-a`     | bool   | off     | Run all tests. Without this flag only `ping_pong` executes.                                  |
| `-h`     | bool   | off     | Print usage information and exit.                                                            |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., HFI unit selection, MQ statistics display). Consult the `psm2perf` documentation for the full set of inherited options.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. It communicates over a raw TCP socket (port `SERVER_PORT + 1`) solely for out-of-band PSM2 endpoint information exchange between the server and client processes.

## EXAMPLES

### Example 1 — Start the server side

On the node that will act as the server, launch `test_tool` with no positional argument. It will listen for an incoming client connection and run the default `ping_pong` test.

```bash
# On server node (e.g., opa-node01)
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

On a second node, specify the server hostname. Only the `ping_pong` test runs by default.

```bash
# On client node (e.g., opa-node02)
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

### Example 3 — Run all tests with the default 4 KiB data-integrity payload

Pass `-a` to enable the `data_integrity` and `tag_match` tests in addition to `ping_pong`.

=== "Server"

    ```bash
    ./test_tool -a
    ```

=== "Client"

    ```bash
    ./test_tool opa-node01 -a
    ```

### Example 4 — Run all tests with a 64 KiB data-integrity payload

Use `-s` to override the message size for the `data_integrity` test.

=== "Server"

    ```bash
    ./test_tool -a -s 65536
    ```

=== "Client"

    ```bash
    ./test_tool opa-node01 -a -s 65536
    ```

### Example 5 — Run all tests with the maximum 1 MiB payload

Stress the data path with the largest supported message size.

=== "Server"

    ```bash
    ./test_tool -a -s 1048576
    ```

=== "Client"

    ```bash
    ./test_tool opa-node01 -a -s 1048576
    ```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# opa-node02 — CLIENT
#
  [PASS] ping_pong                          240.12 us  (100 iters, 2.40 us/iter)
  [PASS] data_integrity                     185.30 us  (1048576 bytes verified)
  [PASS] tag_match                           18.44 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 6 — Build from source

```bash
# From the repository root
cd test_tool
make clean
make
```

!!! warning "Build prerequisites"
    The build requires `libpsm2` development headers and the shared objects `../libpsm2.o` and `../psm2perf.o`. Run `make` in the parent directory first if those objects do not yet exist.

### Example 7 — Display help

```bash
./test_tool -h
```

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

| Variable            | Required | Default          | Description                                                                                          |
| :------------------ | :------- | :--------------- | :--------------------------------------------------------------------------------------------------- |
| `PSM2_DEVICES`      | No       | `self,shm,hfi`   | Comma-separated list of PSM2 device types to enable. Inherited from the PSM2 runtime.                |
| `HFI_UNIT`          | No       | `0`              | HFI unit number to open. Relevant on multi-HFI systems.                                             |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Controls the rendezvous window size for large messages. May affect `data_integrity` at large sizes. |
| `PSM2_TRACEMASK`    | No       | `0x1`            | PSM2 internal trace mask for debug output.                                                           |
| `PSM2_MQ_RECVREQS_MAX` | No   | *(library default)* | Maximum number of pre-posted receive requests. Relevant to `tag_match` concurrency.                |

!!! tip
    Set `PSM2_TRACEMASK=0xFFFF` to enable verbose PSM2 library tracing when diagnosing connectivity failures.

## FILES

| Path                          | Description                                                                                     |
| :---------------------------- | :---------------------------------------------------------------------------------------------- |
| `test_tool/test_tool.c`       | Main source file containing test implementations and CLI entry point.                           |
| `test_tool/test_tool.h`       | Header defining test constants, result structures, `fill_pattern()`, and `verify_pattern()`.    |
| `test_tool/Makefile`          | Build rules for compiling `test_tool` and linking against shared objects and `libpsm2`.         |
| `test_tool/README`            | Brief plain-text usage notes.                                                                   |
| `libpsm2.h`                   | Shared PSM2 wrapper API header (parent directory).                                              |
| `psm2perf.h`                  | Shared benchmark infrastructure header providing `init_benchmark()`, socket helpers, and timers.|
| `libpsm2.o`                   | Compiled shared object for PSM2 endpoint management.                                            |
| `psm2perf.o`                  | Compiled shared object for benchmark scaffolding (socket I/O, info exchange, timing).           |

## EXIT STATUS

| Code | Meaning                                                                                          |
| :--- | :----------------------------------------------------------------------------------------------- |
| `0`  | All executed tests passed (`num_fail == 0`).                                                     |
| `1`  | One or more tests failed, or a fatal initialization error occurred (socket open, info exchange, PSM2 init, or invalid command-line argument). |

!!! warning "Partial execution"
    If initialization fails (e.g., the TCP control socket cannot be opened or `libpsm2_init()` returns an error), the tool exits with code `1` before any tests run. The summary line will not be printed in this case.

## SEE ALSO

- **opa-psm2-tests repository** — [github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests)
- **libpsm2** — PSM2 wrapper library used by all tools in the repository (`libpsm2.h`, `libpsm2.c`)
- **psm2perf** — Shared benchmark infrastructure (`psm2perf.h`, `psm2perf.c`)
- **psm2_mq_wait(3)** — PSM2 Matched Queue wait call used for synchronous completion
- **psm2_mq_isend(3)** / **psm2_mq_irecv(3)** — PSM2 non-blocking send/receive primitives
- **opa-fm(8)** — Omni-Path Fabric Manager; must be running for fabric connectivity
- **opainfo(1)** — Utility to verify HFI link state before running tests

---

*Copyright © 2026 Cornelis Networks. Dual licensed under BSD and GPLv2.*
```