# Test Tool — Design Reference

```markdown
---
title: "Test Tool — Design Reference"
description: "PSM2 connection test tool for verifying connectivity, data integrity, and tag-matching over the OPA fabric."
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

The tool reuses the shared `libpsm2` and `psm2perf` infrastructure already present in the repository's latency and bandwidth benchmarks. One node runs as the **server** (listener) and the other as the **client** (initiator). The server role is assumed when no positional hostname argument is provided; supplying a hostname causes the process to connect to that server as a client. All tests follow a symmetric send/receive pattern so that both sides participate in verification.

Three built-in tests are available. The **ping_pong** test (always executed) performs 100 round-trip 64-byte message exchanges and reports per-iteration latency. The **data_integrity** test fills a buffer with a deterministic byte pattern, transmits it, and verifies the pattern on both the receiver and the echo path back to the sender. The **tag_match** test sends two messages concurrently using distinct PSM2 tag values and confirms that each message is delivered to the correct tag-selective receive buffer. The `-a` flag enables the data-integrity and tag-match tests in addition to the default ping-pong test.

## SUBCOMMANDS

`test_tool` does not use subcommands. Test selection is controlled through command-line options and the positional `server` argument.

| Positional Argument | Description |
|---|---|
| `server` | Hostname or IP address of the server node. Omit this argument to run as the server (listener). |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-s SIZE` | integer | `4096` | Message size in bytes for the data-integrity test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run all tests. Without this flag only the `ping_pong` test executes. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., `--show-mqstats`) before `test_tool` parses its own flags. Refer to the `psm2perf` documentation for the full set of inherited options.

### Test-Specific Constants (compile-time)

The following parameters are defined in `test_tool.h` and can be changed at build time:

| Constant | Default Value | Description |
|---|---|---|
| `PING_ITERS` | `100` | Number of round-trip iterations in the ping-pong test. |
| `TEST_MAX_MSG` | `1048576` (1 MiB) | Maximum payload size for data-integrity tests. |
| `FILL_SEED` | `0xCAFE` | Deterministic seed used to generate and verify fill patterns. |

## Shannon Commands

`test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework. This section is not applicable.

| Command | Method | Description |
|---|---|---|
| — | — | No Shannon integration. |

## API Endpoints

`test_tool` does not expose a REST API. Communication between the server and client occurs over a raw TCP socket (for PSM2 endpoint exchange) and the PSM2 MQ messaging layer (for test traffic).

| Method | Path | Description |
|---|---|---|
| — | — | No REST API endpoints. |

## EXAMPLES

!!! warning "Prerequisites"
    Both nodes must have the OPA fabric driver loaded, the `libpsm2` library installed, and the `hfi1` device accessible. The server process must be started **before** the client.

### Example 1 — Start the server (listener)

Run `test_tool` without a positional argument to enter server mode. The process listens for an incoming client connection and then participates in whatever tests the client requests.

```bash
# On node "opa-server-01":
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

Connect to the server and execute only the `ping_pong` round-trip latency test (the default when `-a` is not specified).

```bash
# On node "opa-client-01":
./test_tool opa-server-01
```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# opa-client-01 — CLIENT
#
  [PASS] ping_pong                          234.50 us  (100 iters, 2.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with the default message size

Enable the data-integrity and tag-match tests in addition to ping-pong by passing `-a`.

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -a
```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# opa-client-01 — CLIENT
#
  [PASS] ping_pong                          230.12 us  (100 iters, 2.30 us/iter)
  [PASS] data_integrity                      48.71 us  (4096 bytes verified)
  [PASS] tag_match                           12.34 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 4 — Run all tests with a custom 64 KiB message size

Use `-s` to override the data-integrity payload size. The value must be between `1` and `1048576`.

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -s 65536 -a
```

### Example 5 — Run all tests with the maximum 1 MiB payload

Stress the data path with the largest supported message size.

```bash
# Server:
./test_tool

# Client:
./test_tool opa-server-01 -s 1048576 -a
```

### Example 6 — Use the exit code in a CI script

`test_tool` returns `0` when all tests pass and `1` when any test fails, making it suitable for automated pass/fail gating.

```bash
#!/bin/bash
# ci_fabric_check.sh — run on the client node

./test_tool opa-server-01 -a
rc=$?

if [ $rc -ne 0 ]; then
    echo "FABRIC CHECK FAILED" >&2
    exit 1
fi

echo "Fabric connectivity verified."
```

### Example 7 — Build from source

Compile `test_tool` and its shared dependencies from the repository root.

```bash
cd opa-psm2-tests/test_tool
make clean
make
```

The build produces the `test_tool` binary in the current directory. The shared objects `../libpsm2.o` and `../psm2perf.o` are built automatically via the parent Makefile.

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | (auto) | Comma-separated list of PSM2 device names to use. When unset, PSM2 auto-detects available HFI devices. |
| `HFI_UNIT` | No | `0` | HFI unit number to open when multiple adapters are present. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal tracing verbosity. Useful for debugging connection failures. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | (library default) | Rendezvous window size in bytes. May affect data-integrity test performance at large message sizes. |
| `PSM2_MQ_EAGER_SDMA_SZ` | No | (library default) | Threshold above which eager sends use SDMA instead of PIO. |

!!! tip "Debugging connection issues"
    Set `PSM2_TRACEMASK=0xFFFF` on both server and client to enable verbose PSM2 tracing when diagnosing handshake or endpoint-exchange failures.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and the CLI entry point. |
| `test_tool/test_tool.h` | Header defining test constants, result structures, tag values, and inline pattern-fill/verify helpers. |
| `test_tool/Makefile` | Build rules for `test_tool` and its shared-object dependencies. |
| `test_tool/README` | Brief plain-text usage notes. |
| `libpsm2.h` | Shared PSM2 wrapper API header (parent directory). |
| `psm2perf.h` | Shared benchmark infrastructure header (parent directory). |
| `libpsm2.o` | Compiled PSM2 wrapper object linked into `test_tool`. |
| `psm2perf.o` | Compiled benchmark infrastructure object linked into `test_tool`. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (`num_fail == 0`). |
| `1` | One or more tests failed, or a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! warning "Partial execution"
    If the tool exits with code `1` due to an initialization error (e.g., socket or PSM2 failure), no test results are printed. Check `stderr` for diagnostic messages in this case.

## SEE ALSO

- [`opa-psm2-tests` repository](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and the shared `libpsm2`/`psm2perf` infrastructure.
- `libpsm2(7)` — PSM2 Matched-Queue API reference.
- `hfi1(4)` — Kernel driver for Cornelis Networks OPA HFI adapters.
- `opainfo(1)` — OPA fabric information and diagnostic utility.
- `psm2perf` — Shared benchmark framework used by `test_tool` for endpoint initialization, socket management, and statistics reporting.
```