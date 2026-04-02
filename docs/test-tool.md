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

`test_tool` is a client/server diagnostic utility from the `opa-psm2-tests` repository that exercises the PSM2 Matched-Queue (MQ) messaging API over Cornelis Omni-Path Architecture (OPA) fabrics. It is designed to validate that a pair of nodes can exchange messages correctly, that payload data survives the transfer without corruption, and that the PSM2 tag-matching logic correctly demultiplexes concurrent message streams.

The tool ships three built-in tests. The **ping_pong** test performs a configurable number of 64-byte round-trip exchanges and reports per-iteration latency. The **data_integrity** test fills a buffer with a deterministic byte pattern, sends it to the peer, and verifies the pattern on both sides (receiver checks the inbound copy, sender checks the echoed copy). The **tag_match** test sends two messages simultaneously using distinct tag values and confirms that each message is delivered to the correct receive buffer, exercising the tag-selective matching path independently of the default tag (`0xF`) used by the companion performance benchmarks.

`test_tool` reuses the shared `libpsm2` and `psm2perf` infrastructure that underpins the latency and bandwidth benchmarks in the same repository. One node runs in server mode (no positional argument), and the other runs in client mode by specifying the server's hostname. A TCP side-channel is used for endpoint-information exchange before PSM2 messaging begins.

## SUBCOMMANDS

`test_tool` does not use named subcommands. Test selection is controlled by the `-a` flag and the positional `server` argument determines the role (server or client).

| Positional Argument | Description |
|---|---|
| *(none)* | Run as the **server** node. Listens for an incoming client connection. |
| `server` | Run as the **client** node. Connects to the specified server hostname. |

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests (`ping_pong`, `data_integrity`, `tag_match`). Without this flag only `ping_pong` is executed. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip
    The `-s` flag only affects the `data_integrity` test. The `ping_pong` and `tag_match` tests always use 64-byte messages.

!!! warning
    The `-s` value is silently clamped to `TEST_MAX_MSG` (1 MiB) inside the `run_data_integrity` function, but the CLI parser rejects out-of-range values with an error. Always specify a value between `1` and `1048576`.

### Inherited Options (from `init_benchmark`)

The tool delegates initial argument parsing to `init_benchmark()` from the shared `psm2perf` library. Options recognized by that layer (such as MQ statistics display) are also available. Consult the `psm2perf` documentation for the full list.

| Flag | Type | Default | Description |
|---|---|---|---|
| `--show-mqstats` | boolean | off | Print PSM2 MQ statistics after the test run completes. Handled via `info->show_mqstats`. |

## Shannon Commands

Not applicable. `test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework.

## API Endpoints

Not applicable. `test_tool` does not expose a REST API. Inter-node communication uses a TCP side-channel socket (port `SERVER_PORT + 1`) for PSM2 endpoint exchange, followed by native PSM2 MQ messaging for test traffic.

## EXAMPLES

### Example 1 — Run the default ping-pong smoke test

Start the server on one node, then the client on another:

=== "Server node"

    ```bash
    # On node-a (server): listen for incoming connection
    ./test_tool
    ```

=== "Client node"

    ```bash
    # On node-b (client): connect to the server
    ./test_tool node-a
    ```

Expected output (client side):

```
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          234.50 us  (100 iters, 2.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 2 — Run all tests with the default 4 KiB data-integrity payload

=== "Server node"

    ```bash
    ./test_tool -a
    ```

=== "Client node"

    ```bash
    ./test_tool node-a -a
    ```

### Example 3 — Run all tests with a 64 KiB data-integrity payload

=== "Server node"

    ```bash
    ./test_tool -a -s 65536
    ```

=== "Client node"

    ```bash
    ./test_tool node-a -a -s 65536
    ```

### Example 4 — Run all tests with the maximum 1 MiB payload

=== "Server node"

    ```bash
    ./test_tool -a -s 1048576
    ```

=== "Client node"

    ```bash
    ./test_tool node-a -a -s 1048576
    ```

### Example 5 — Run all tests and display PSM2 MQ statistics

=== "Server node"

    ```bash
    ./test_tool -a --show-mqstats
    ```

=== "Client node"

    ```bash
    ./test_tool node-a -a --show-mqstats
    ```

### Example 6 — Build from source and run

```bash
# Clone the repository and build the test tool
git clone https://github.com/cornelisnetworks/opa-psm2-tests.git
cd opa-psm2-tests/test_tool
make

# Run (server on current terminal, client on remote node)
./test_tool -a
```

### Example 7 — Scripted pass/fail gate in CI

```bash
#!/bin/bash
# ci_fabric_check.sh — exits non-zero if any PSM2 test fails

SERVER_HOST="$1"
if [ -z "$SERVER_HOST" ]; then
    echo "Usage: $0 <server-hostname>" >&2
    exit 2
fi

./test_tool "$SERVER_HOST" -a -s 131072
exit $?
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|---|---|---|---|
| `PSM2_DEVICES` | No | (all) | Comma-separated list of PSM2 device names to use. Inherited by the PSM2 library. |
| `HFI_UNIT` | No | `0` | HFI unit number to open. Passed through to the PSM2 runtime. |
| `PSM2_TRACEMASK` | No | `0x1` | Bitmask controlling PSM2 internal tracing verbosity. Useful for debugging connection failures. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | (library default) | Rendezvous window size for large messages. May affect `data_integrity` test performance at 1 MiB. |
| `PSM2_MQ_RNDV_SHM_THRESH` | No | (library default) | Shared-memory rendezvous threshold. Relevant only when both endpoints share a node (loopback testing). |

!!! tip
    Set `PSM2_TRACEMASK=0xFFFF` to get verbose PSM2 debug output when diagnosing connection or tag-matching failures.

## FILES

| Path | Description |
|---|---|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header defining constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values), the `test_result` struct, and inline `fill_pattern`/`verify_pattern` helpers. |
| `test_tool/Makefile` | Build rules. Compiles `test_tool.c` and links against `../libpsm2.o`, `../psm2perf.o`, and `-lpsm2`. |
| `test_tool/README` | Brief plain-text usage notes shipped with the source. |
| `../libpsm2.h` | Shared PSM2 wrapper API header (endpoint init, send/recv helpers, shutdown). |
| `../psm2perf.h` | Shared benchmark infrastructure header (`init_benchmark`, `open_socket`, `exchange_info`, timing macros). |
| `../libpsm2.o` | Compiled shared object providing `libpsm2_init`, `libpsm2_shutdown`, `post_send`, `post_irecv`, `print_psm2_stats`. |
| `../psm2perf.o` | Compiled shared object providing `init_benchmark`, `open_socket`, `exchange_info`, `TIMER`, `ts_diff`. |

## EXIT STATUS

| Code | Meaning |
|---|---|
| `0` | All executed tests passed. |
| `1` | One or more tests failed, **or** a fatal initialization error occurred (PSM2 init failure, socket error, invalid arguments). |

!!! warning
    The tool does not distinguish between test failures and infrastructure errors in its exit code. Both conditions produce exit code `1`. Inspect the standard output for the `# Summary:` line to differentiate.

## BREAKING CHANGES

This is the initial release of `test_tool`. No breaking changes have been introduced.

!!! tip
    Future versions may add new tests behind the `-a` flag. Scripts that parse output should match on the `[PASS]`, `[FAIL]`, or `[SKIP]` markers and the `# Summary:` line rather than assuming a fixed number of result lines.

## SEE ALSO

- **opa-psm2-tests repository** — [https://github.com/cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests)
- **libpsm2 / psm2perf** — Shared infrastructure used by all benchmarks and test tools in the repository. See `../libpsm2.h` and `../psm2perf.h`.
- **PSM2 Programmer's Guide** — Cornelis Networks documentation covering the `psm2_mq_*` matched-queue API, tag semantics, and endpoint lifecycle.
- **opa-psm2** — The PSM2 user-space library: [https://github.com/cornelisnetworks/opa-psm2](https://github.com/cornelisnetworks/opa-psm2)
- **psm2_mq_isend(3)**, **psm2_mq_irecv(3)**, **psm2_mq_wait(3)** — PSM2 library man pages for the messaging primitives used by `test_tool`.