# Test Tool — Design Reference

---

## NAME

**test_tool** — PSM2 connection test tool for verifying connectivity, data integrity, and tag-matching over the OPA fabric.

---

## SYNOPSIS

```
test_tool [server] [-s size] [-a] [-h]
```

---

## DESCRIPTION

`test_tool` is a client-server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched Queue (MQ) messaging API over an Omni-Path Architecture (OPA) fabric. It is designed to validate that two nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool reuses the shared `libpsm2` and `psm2perf` infrastructure already present in the repository's latency and bandwidth benchmarks. One node runs as the **server** (listener) and the other as the **client** (initiator). A TCP out-of-band socket is used for initial endpoint exchange before all test traffic moves to the PSM2 path. By default only the lightweight ping-pong smoke test executes; the '-a' flag enables the full suite including data-integrity and tag-match isolation tests.

`test_tool` is intended for fabric bring-up validation, regression testing of PSM2 library updates, and quick health checks before running production workloads. It produces a concise pass/fail summary with per-test timing that is easy to parse in CI pipelines.

---

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled through the '-a' option flag. The individual tests executed are:

| Test Name | Description |
|---|---|
| `ping_pong` | Round-trip message exchange using 64-byte messages over 100 iterations. Always runs. Measures per-iteration latency. |
| `data_integrity` | Sends a deterministic fill-pattern payload from server to client, client verifies and echoes back, server re-verifies. Configurable message size up to 1 MiB. Runs only with '-a'. |
| `tag_match` | Sends two concurrent messages with distinct PSM2 tags (`TAG_MULTI_A`, `TAG_MULTI_B`), verifies each lands in the correct receive buffer via tag-selective matching. Runs only with '-a'. |

---

## OPTIONS

### Positional Arguments

| Argument | Type | Default | Description |
|---|---|---|---|
| `server` | string | *(none — act as server)* | Hostname or IP address of the server node. When omitted, the process assumes the server role and listens for an incoming connection. |

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-s SIZE` | long | `4096` | Message size in bytes for the `data_integrity` test. Valid range is 1 to 1048576 (1 MiB). Ignored when '-a' is not specified. |
| `-a` | boolean | off | Run all tests. Without this flag only the `ping_pong` test executes. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip
    The tool also inherits options parsed by `init_benchmark()` from the shared `psm2perf` infrastructure, including the `--show-mqstats` flag that prints internal PSM2 MQ statistics after the test run.

---

## Shannon Commands

*Not applicable.* `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

---

## API Endpoints

*Not applicable.* `test_tool` does not expose a REST API. Inter-node communication uses a TCP out-of-band socket (port `SERVER_PORT + 1`) for PSM2 endpoint exchange, followed by native PSM2 MQ messaging for all test traffic.

---

## EXAMPLES

### Example 1 — Start the server and wait for a client

Run on the node that will act as the server. The process blocks until a client connects.

```bash
# On node "opa-node01" — server role (no positional argument)
./test_tool
```

### Example 2 — Run the default ping-pong test from the client

Run on a second node, pointing at the server hostname. Only the `ping_pong` test executes.

```bash
# On node "opa-node02" — client role
./test_tool opa-node01
```

Expected output (both sides):

```
# PSM2 Connection Test Tool
# opa-node02 — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run the full test suite with default message size

```bash
# Server
./test_tool -a

# Client
./test_tool opa-node01 -a
```

This executes `ping_pong`, `data_integrity` (4096 bytes), and `tag_match` in sequence.

### Example 4 — Run all tests with a large data-integrity payload

Use a 64 KiB message for the data-integrity verification to stress larger transfer paths (e.g., eager-to-rendezvous transition).

```bash
# Server
./test_tool -a -s 65536

# Client
./test_tool opa-node01 -a -s 65536
```

### Example 5 — Run the maximum 1 MiB data-integrity test

```bash
# Server
./test_tool -a -s 1048576

# Client
./test_tool opa-node01 -a -s 1048576
```

### Example 6 — Quick connectivity check in a CI script

```bash
#!/bin/bash
# ci_fabric_check.sh — exits non-zero on any test failure

SERVER_HOST="$1"

if [ -z "$SERVER_HOST" ]; then
    echo "Usage: $0 <server-hostname>"
    exit 2
fi

./test_tool "$SERVER_HOST" -a -s 8192
exit $?
```

### Example 7 — Build from source and run

```bash
cd test_tool
make clean && make
./test_tool -h
```

---

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | *(auto)* | Comma-separated list of PSM2 device names to use. Inherited by the PSM2 library. |
| `PSM2_MULTIRAIL` | No | `0` | Enable multi-rail support in PSM2 when set to `1`. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number when multiple adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Controls the rendezvous window size for large messages. Relevant when testing with large '-s' values. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask enabling PSM2 internal tracing. Useful for debugging test failures. |
| `PSM2_MQ_EAGER_SDMA_SZ` | No | *(library default)* | Threshold at which PSM2 switches from PIO to SDMA for eager sends. Affects which code path the data-integrity test exercises. |

!!! warning
    Setting `PSM2_TRACEMASK` to high values can produce extremely verbose output and may affect timing measurements in the `ping_pong` test. Use only for debugging.

---

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header file defining constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values), the `test_result` struct, and inline `fill_pattern`/`verify_pattern` helpers. |
| `test_tool/Makefile` | Build rules for the `test_tool` binary. Links against `libpsm2.o`, `psm2perf.o`, and `-lpsm2`. |
| `test_tool/README` | Plain-text quick-start reference. |
| `../libpsm2.h` | Shared PSM2 wrapper API header (endpoint open, send, recv, shutdown). |
| `../psm2perf.h` | Shared benchmark infrastructure header (`init_benchmark`, `open_socket`, `exchange_info`, timing macros). |
| `../libpsm2.o` | Compiled shared object providing PSM2 lifecycle and messaging wrappers. |
| `../psm2perf.o` | Compiled shared object providing socket exchange, benchmark init, and statistics printing. |

---

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed. |
| `1` | One or more tests failed, or a fatal initialization error occurred (socket open failure, PSM2 init failure, invalid arguments). |

!!! tip
    The summary line printed at the end of execution (`# Summary: N passed, M failed, K skipped`) provides a human-readable breakdown. For scripted use, rely on the exit code: any non-zero return indicates a problem that requires investigation.

---

## SEE ALSO

- **opa-psm2-tests repository** — [github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency and bandwidth benchmarks that share the `libpsm2` / `psm2perf` infrastructure.
- **PSM2 API documentation** — Cornelis Networks PSM2 Programmer's Guide for details on `psm2_mq_isend`, `psm2_mq_irecv`, `psm2_mq_wait`, and tag-matching semantics.
- **libpsm2(7)** — System man page for the PSM2 library (installed with the `libpsm2-devel` package).
- **opa-fm(8)** — Omni-Path Fabric Manager; required for fabric initialization before running `test_tool`.
- **opainfo(1)** — Quick check that the local HFI is up and linked before running connectivity tests.