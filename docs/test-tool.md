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

The tool reuses the shared `libpsm2` and `psm2perf` infrastructure already present in the repository's latency and bandwidth benchmarks. One node runs as the **server** (listener) and the other as the **client** (initiator). The server is started without a positional argument; the client is started with the server's hostname. After a TCP-based out-of-band handshake to exchange PSM2 endpoint addresses, the tool runs one or more tests and prints a pass/fail summary.

Three built-in tests are provided. The **ping_pong** test (always executed) performs 100 round-trip message exchanges at 64 bytes to confirm basic connectivity and measure per-iteration latency. The **data_integrity** test sends a deterministic fill-pattern payload (up to 1 MiB), receives it back, and verifies every byte on both sides. The **tag_match** test sends two messages with distinct tag values concurrently and confirms that each message is delivered to the correct tag-selective receive buffer. The latter two tests are gated behind the `-a` flag.

## SUBCOMMANDS

`test_tool` does not use subcommands. Test selection is controlled through command-line options and the role is determined by the presence or absence of the positional `server` argument.

| Role | Invocation | Description |
|------|-----------|-------------|
| Server | `test_tool` | Start as the listening (server) side of the test pair. |
| Client | `test_tool <server-hostname>` | Connect to the specified server and run the selected tests. |

## OPTIONS

### Positional Arguments

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `server` | string | *(none — act as server)* | Hostname or IP address of the server node. Omit this argument to run as the server. |

### Optional Flags

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `-s SIZE` | integer | `4096` | Message size in bytes for the `data_integrity` test. Valid range: `1` to `1048576` (1 MiB). |
| `-a` | boolean | off | Run **all** tests (`ping_pong`, `data_integrity`, `tag_match`). Without this flag only `ping_pong` is executed. |
| `-h` | boolean | off | Print usage information and exit. |

!!! tip "Inherited options"
    `test_tool` calls `init_benchmark(argc, argv)` from the shared `psm2perf` library, which may consume additional arguments (e.g., `--show-mqstats`). Refer to the `psm2perf` documentation for the full set of inherited options.

## Shannon Commands

`test_tool` is a standalone CLI binary and is not exposed through the Shannon agent framework. This section is not applicable.

## API Endpoints

`test_tool` does not expose a REST API. The only network communication is the internal TCP socket used for out-of-band PSM2 endpoint exchange (port `SERVER_PORT + 1`) and the PSM2 MQ data path over the OPA fabric.

## EXAMPLES

!!! tip "Prerequisites"
    Both nodes must have the `opa-psm2` user-space library installed, an active OPA fabric link, and the `hfi1` kernel driver loaded.

### Example 1 — Build the tool

```bash
cd opa-psm2-tests/test_tool
make
```

### Example 2 — Run the default ping-pong test

=== "Server (node-a)"

    ```bash
    # Start the server side — listens for the client connection
    ./test_tool
    ```

=== "Client (node-b)"

    ```bash
    # Connect to the server and run the ping-pong smoke test
    ./test_tool node-a
    ```

Expected output (client side):

```text
# PSM2 Connection Test Tool
# node-b — CLIENT
#
  [PASS] ping_pong                          1234.56 us  (100 iters, 12.35 us/iter)
#
# Summary: 1 passed, 0 failed, 0 skipped
```

### Example 3 — Run all tests with the default 4 KiB data-integrity payload

=== "Server (node-a)"

    ```bash
    ./test_tool -a
    ```

=== "Client (node-b)"

    ```bash
    ./test_tool node-a -a
    ```

Expected output (server side):

```text
# PSM2 Connection Test Tool
# node-a — SERVER
#
  [PASS] ping_pong                          1200.00 us  (100 iters, 12.00 us/iter)
  [PASS] data_integrity                      345.67 us  (4096 bytes verified)
  [PASS] tag_match                            89.12 us  (2 tags verified)
#
# Summary: 3 passed, 0 failed, 0 skipped
```

### Example 4 — Run all tests with a 64 KiB data-integrity payload

=== "Server (node-a)"

    ```bash
    ./test_tool -a -s 65536
    ```

=== "Client (node-b)"

    ```bash
    ./test_tool node-a -a -s 65536
    ```

### Example 5 — Run all tests with the maximum 1 MiB payload

=== "Server (node-a)"

    ```bash
    ./test_tool -a -s 1048576
    ```

=== "Client (node-b)"

    ```bash
    ./test_tool node-a -a -s 1048576
    ```

### Example 6 — Scripted pass/fail gate in CI

```bash
#!/bin/bash
# Run on the client node; assumes the server is already listening.
./test_tool node-a -a -s 16384
rc=$?
if [ $rc -ne 0 ]; then
    echo "FATAL: PSM2 connectivity test failed" >&2
    exit 1
fi
echo "PSM2 fabric link verified."
```

### Example 7 — Display help text

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
| `PSM2_DEVICES` | No | `self,shm,hfi` | Comma-separated list of PSM2 device types to use. Inherited by `libpsm2_init()`. |
| `HFI_UNIT` | No | `0` | HFI unit number to open when multiple adapters are present. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | *(library default)* | Rendezvous window size; may affect large-message data-integrity test performance. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask enabling PSM2 internal tracing. Useful for debugging failed tests. |
| `PSM2_MQ_RECVREQS_MAX` | No | *(library default)* | Maximum pre-posted receive requests. Relevant when running the `tag_match` test under constrained configurations. |

!!! warning "Fabric prerequisites"
    The `hfi1` kernel module must be loaded and the OPA link must be **Active** on both nodes before running `test_tool`. Verify with `opainfo` or `hfi1_control`.

## FILES

| Path | Description |
|------|-------------|
| `test_tool/test_tool.c` | Main source file containing test implementations and CLI entry point. |
| `test_tool/test_tool.h` | Header defining constants (`TEST_MAX_MSG`, `PING_ITERS`, tag values), the `test_result` structure, and inline `fill_pattern`/`verify_pattern` helpers. |
| `test_tool/Makefile` | Build rules; compiles `test_tool.c` and links against shared objects `libpsm2.o` and `psm2perf.o`. |
| `test_tool/README` | Plain-text quick-start reference. |
| `../libpsm2.h` | Shared PSM2 wrapper API header (endpoint init, send/recv helpers). |
| `../psm2perf.h` | Shared benchmark infrastructure header (`benchmark_info`, socket helpers, timer macros). |
| `../libpsm2.o` | Compiled shared object providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_send()`, `post_irecv()`, etc. |
| `../psm2perf.o` | Compiled shared object providing `init_benchmark()`, `open_socket()`, `exchange_info()`, `print_psm2_stats()`, etc. |

## EXIT STATUS

| Code | Meaning |
|------|---------|
| `0` | All executed tests passed. |
| `1` | One or more tests failed **or** a fatal initialization error occurred (e.g., socket open failure, PSM2 init failure, invalid `-s` argument). |

!!! warning "Partial execution"
    If the tool exits with code `1` due to an initialization error (socket, endpoint exchange, or PSM2 init), no test results are printed. Check `stderr` for diagnostic messages.

## SEE ALSO

- [`opa-psm2-tests` repository](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing latency/bandwidth benchmarks and shared infrastructure.
- `libpsm2.h` / `psm2perf.h` — Shared API headers documenting `post_send()`, `post_irecv()`, `init_benchmark()`, and timer macros.
- [`opa-psm2`](https://github.com/cornelisnetworks/opa-psm2) — The PSM2 user-space library that `test_tool` exercises.
- `opainfo(1)` — Cornelis OPA fabric status utility for verifying link state before running tests.
- `psm2_mq_isend(3)`, `psm2_mq_irecv(3)`, `psm2_mq_wait(3)` — PSM2 Matched-Queue API man pages.

---

*Copyright © 2026 Cornelis Networks. Dual-licensed under BSD and GPLv2.*
```