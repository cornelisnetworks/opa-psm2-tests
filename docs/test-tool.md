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

`test_tool` is a client/server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched-Queue messaging API over Cornelis Omni-Path Architecture (OPA) fabrics. It is designed to validate that two nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool builds on the shared `libpsm2` and `psm2perf` infrastructure used by the existing latency and bandwidth benchmarks in the repository. It establishes a TCP out-of-band socket for endpoint information exchange, initializes a PSM2 endpoint and matched queue on each side, and then runs one or more test cases. Each test case produces a PASS, FAIL, or SKIP verdict along with timing information.

`test_tool` is intended for fabric bring-up, regression testing, and troubleshooting. Run it after installing new firmware, drivers, or PSM2 library versions to confirm basic end-to-end functionality before moving on to performance benchmarks. The tool operates in a two-node model: one node acts as the server (listener) and the other as the client (connector). The server role is assumed when no hostname argument is provided.

## SUBCOMMANDS

`test_tool` does not use explicit subcommands. Test selection is controlled through the `-a` flag and the positional `server` argument. The individual tests executed are:

| Test Name | Description |
|---|---|
| `ping_pong` | Round-trip latency smoke test. Exchanges 64-byte messages for 100 iterations and reports per-iteration latency. Always runs. |
| `data_integrity` | Sends a deterministic fill-pattern payload from server to client, the client verifies byte-by-byte, echoes it back, and the server re-verifies. Runs only with '-a'. |
| `tag_match` | Sends two concurrent messages with distinct PSM2 tags (`TAG_MULTI_A`, `TAG_MULTI_B`) and verifies each lands in the correct receive buffer via tag-selective matching. Runs only with '-a'. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `server` | positional string | *(none — act as server)* | Hostname or IP address of the server node. Omit this argument to run in server mode. |
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: 1 to 1048576 (1 MiB). |
| `-a` | boolean flag | off | Run all tests. Without this flag only the `ping_pong` test executes. |
| `-h` | boolean flag | off | Print usage information and exit. |

!!! tip
    The `-s` flag only affects the `data_integrity` test. The `ping_pong` and `tag_match` tests always use 64-byte messages.

!!! warning
    The positional `server` argument must appear **before** any dash-options because `init_benchmark()` consumes it from `argv` before `getopt` runs.

### Inherited Options

Options parsed by the shared `init_benchmark()` / `psm2perf` infrastructure (e.g., `--show-mqstats`) are also accepted. When `info->show_mqstats` is set, PSM2 internal matched-queue statistics are printed after the test summary.

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. It communicates over a private TCP socket (`SERVER_PORT + 1`) solely for PSM2 endpoint information exchange between the server and client processes.

## EXAMPLES

### Example 1 — Start the server (listener) side

On the node that will act as the server, run `test_tool` with no positional argument. It will listen for an incoming client connection and execute the default `ping_pong` test.

```bash
# On server node (e.g., opa-node01):
cd test_tool
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

On the client node, specify the server hostname. Only the `ping_pong` test runs by default.

```bash
# On client node (e.g., opa-node02):
cd test_tool
./test_tool opa-node01
```

Expected output on both sides:

```
# PSM2 Connection Test Tool
# opa-node02 — CLIENT
#
  [PASS] ping_pong                          234.50 us  (100 iters, 2.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with the default 4 KiB data-integrity payload

Use the `-a` flag to execute `ping_pong`, `data_integrity`, and `tag_match`.

```bash
# Server side:
./test_tool -a

# Client side:
./test_tool opa-node01 -a
```

### Example 4 — Run all tests with a 64 KiB data-integrity payload

Combine `-a` with `-s` to stress the data path with a larger message.

```bash
# Server side:
./test_tool -a -s 65536

# Client side:
./test_tool opa-node01 -a -s 65536
```

### Example 5 — Run all tests with the maximum 1 MiB payload

This exercises the largest supported transfer size, useful for catching corruption in multi-packet eager or rendezvous code paths.

```bash
# Server side:
./test_tool -a -s 1048576

# Client side:
./test_tool opa-node01 -a -s 1048576
```

### Example 6 — Build from source

```bash
# Clone the repository and build the test_tool target:
git clone https://github.com/cornelisnetworks/opa-psm2-tests.git
cd opa-psm2-tests/test_tool
make
```

### Example 7 — Scripted pass/fail check in CI

```bash
#!/bin/bash
# Run on the client node after the server is already listening.
./test_tool opa-node01 -a -s 4096
rc=$?
if [ $rc -ne 0 ]; then
    echo "FATAL: PSM2 connectivity test failed" >&2
    exit 1
fi
echo "PSM2 connectivity verified."
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | *(auto)* | Colon-separated list of PSM2 device names to use. Inherited by the PSM2 library. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple adapters are present. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal tracing verbosity. Useful for debugging connection failures. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Rendezvous window size in bytes. May affect `data_integrity` results at large message sizes. |
| `PSM2_MTU` | No | *(fabric default)* | Override the path MTU. Affects packetization of messages larger than a single packet. |

!!! tip
    Set `PSM2_TRACEMASK=0xFFFF` on both sides when diagnosing connection establishment failures. The trace output goes to `stderr`.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header defining test constants, result structures, `fill_pattern()`, and `verify_pattern()` inline helpers. |
| `test_tool/Makefile` | Build rules. Compiles `test_tool.c` and links against shared objects `libpsm2.o` and `psm2perf.o` plus `-lpsm2`. |
| `test_tool/README` | Brief plain-text usage notes shipped with the source. |
| `../libpsm2.h` | Shared PSM2 wrapper API header (endpoint init, send, recv, shutdown). |
| `../psm2perf.h` | Shared benchmark infrastructure header (socket helpers, `benchmark_info`, timer macros). |
| `../libpsm2.o` | Compiled shared object providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, etc. |
| `../psm2perf.o` | Compiled shared object providing `init_benchmark()`, `open_socket()`, `exchange_info()`, `print_psm2_stats()`, etc. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed (no failures). |
| `1` | One or more tests failed **or** a fatal initialization error occurred (e.g., socket open failure, PSM2 init failure, invalid `-s` argument). |

!!! warning
    A return code of `0` only guarantees that no test reported `TEST_FAIL`. Skipped tests (`TEST_SKIP`) do **not** cause a non-zero exit. Always check the summary line for skip counts in automated pipelines.

## BREAKING CHANGES

| Version / Date | Change |
|---|---|
| 2026-04-02 (initial) | Initial release. No breaking changes — this is the first version of `test_tool`. |

!!! tip
    Because this is a new addition to the repository, all interfaces (CLI flags, exit codes, test names) should be considered **unstable** until the tool reaches a `1.0` designation. Pin your CI scripts to a specific commit hash if you depend on exact output formatting.

## SEE ALSO

- **opa-psm2-tests repository** — [https://github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests)
- **libpsm2** — Shared PSM2 endpoint management wrapper (`../libpsm2.h`, `../libpsm2.c`)
- **psm2perf** — Shared benchmark infrastructure for socket setup, info exchange, and timer utilities (`../psm2perf.h`, `../psm2perf.c`)
- **opa-psm2** — The PSM2 user-space library: [https://github.com/cornelisnetworks/opa-psm2](https://github.com/cornelisnetworks/opa-psm2)
- **psm2(7)** — PSM2 library man page (installed with `libpsm2-devel`)
- **opainfo(1)** — Cornelis OPA fabric diagnostic utility for verifying link state before running `test_tool`