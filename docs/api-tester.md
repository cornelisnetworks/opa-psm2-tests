---
title: "Api Tester User Guide"
date: "2026-04-02"
status: "draft"
---

# Api Tester User Guide

## NAME

`api_tester` — A minimal REST API server that wraps PSM2 messaging operations for remote integration testing and orchestration.

## SYNOPSIS

```
api_tester [-p <port>] [-h]
```

## DESCRIPTION

`api_tester` is a lightweight HTTP/1.1 server that exposes Cornelis Networks PSM2 (Performance Scaled Messaging 2) library operations as REST API endpoints. It enables operators to initialize PSM2 connections, send and receive tagged messages, query message queue statistics, and tear down connections — all through simple `curl` commands or any HTTP client.

The tool is designed for integration testing and remote orchestration of PSM2 benchmarks on Cornelis Omni-Path fabric nodes without requiring direct SSH access. It operates as a single-connection, non-concurrent server that maintains one PSM2 endpoint at a time. A typical workflow involves starting `api_tester` on two nodes, initializing one as a server and the other as a client, then driving send/receive operations from a central test harness.

Internally, `api_tester` reuses the `libpsm2` and `psm2perf` shared infrastructure from the `opa-psm2-tests` repository. It manages a TCP control channel between partner nodes for PSM2 UUID exchange and endpoint setup, then delegates actual messaging to the PSM2 matched-queue (MQ) API. All responses are returned as flat JSON objects with an `"ok"` field indicating success or failure.

## SUBCOMMANDS

`api_tester` does not use subcommands. All functionality is accessed through HTTP endpoints after the server process is started. See the [API Endpoints](#api-endpoints) section for the full list of operations.

## OPTIONS

### Global Options

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `-p <port>` | integer | `8080` | TCP port on which the HTTP server listens for incoming requests. |
| `-h`, `--help` | flag | — | Print usage information and exit immediately. |

!!! tip
    When running multiple `api_tester` instances on the same host (e.g., for loopback testing), assign each a unique port with `-p`.

## Shannon Commands

Not applicable. `api_tester` is not exposed via Shannon. It operates as a standalone HTTP server process.

## API Endpoints

All endpoints return `application/json` responses. Every response includes an `"ok"` boolean field. On failure, an `"error"` string field describes the problem.

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/v1/health` | Service liveness check. Always returns success if the process is running. |
| `GET` | `/v1/status` | Reports PSM2 initialization state, role (server/client), partner hostname, CPU frequency, and server uptime. |
| `GET` | `/v1/stats` | Returns PSM2 MQ statistics including rx/tx byte counts, message counts, eager/rendezvous split, and shared-memory counters. Requires prior initialization. |
| `POST` | `/v1/init` | Initializes the PSM2 endpoint. Requires a JSON body with `"partner"` (hostname) and `"role"` (`"server"` or `"client"`). |
| `POST` | `/v1/shutdown` | Tears down the PSM2 endpoint and closes the control socket. The HTTP server continues running. |
| `POST` | `/v1/send` | Sends a tagged message. Accepts optional JSON body with `"tag"`, `"size"` (1–4194304), and `"rank"`. Returns elapsed time in microseconds. |
| `POST` | `/v1/recv` | Posts a tagged receive and blocks until completion. Accepts optional JSON body with `"tag"`, `"tagsel"`, `"size"` (1–4194304), and `"rank"`. Returns actual received length and elapsed time. |

### Request Body Schemas

**POST /v1/init**

```json
{
  "partner": "<hostname>",
  "role": "server" | "client"
}
```

**POST /v1/send**

```json
{
  "tag": 15,
  "size": 64,
  "rank": 0
}
```

**POST /v1/recv**

```json
{
  "tag": 15,
  "tagsel": 15,
  "size": 64,
  "rank": 0
}
```

!!! warning
    The `"size"` field must be between 1 and 4194304 (4 MiB). Values outside this range return an error. The send buffer is filled with a deterministic pattern `(i * 0xCB + tag) & 0xFF` for verification purposes.

### Response Examples

**GET /v1/health**

```json
{"ok":true,"service":"psm2-api","version":"1.0.0"}
```

**GET /v1/status**

```json
{
  "ok": true,
  "psm2_initialized": true,
  "role": "server",
  "partner": "node-02",
  "cpu_freq_ghz": 2.10,
  "uptime_seconds": 142.3
}
```

**GET /v1/stats**

```json
{
  "ok": true,
  "rx_user_bytes": 4096,
  "rx_user_num": 64,
  "rx_sys_bytes": 0,
  "rx_sys_num": 0,
  "tx_num": 64,
  "tx_eager_num": 60,
  "tx_eager_bytes": 3840,
  "tx_rndv_num": 4,
  "tx_rndv_bytes": 256,
  "tx_shm_num": 0,
  "rx_shm_num": 0,
  "rx_sysbuf_num": 0,
  "rx_sysbuf_bytes": 0
}
```

**Error response**

```json
{"ok":false,"error":"not initialized"}
```

## EXAMPLES

### Example 1 — Start the server on the default port

Launch `api_tester` on a fabric-attached node. The process listens on port 8080 and prints a startup banner.

```bash
./api_tester
# Output: PSM2 API Tester v1.0.0 listening on port 8080
```

### Example 2 — Start the server on a custom port

Use the `-p` flag to bind to a non-default port, useful when port 8080 is already in use or when running multiple instances.

```bash
./api_tester -p 9090
# Output: PSM2 API Tester v1.0.0 listening on port 9090
```

### Example 3 — Check service health from a remote host

Verify that the `api_tester` process is alive and responding. This endpoint does not require PSM2 initialization.

```bash
curl -s http://node-01:8080/v1/health | python3 -m json.tool
```

Expected output:

```json
{
    "ok": true,
    "service": "psm2-api",
    "version": "1.0.0"
}
```

### Example 4 — Initialize a PSM2 server/client pair

On two separate nodes, initialize the PSM2 connection. The server side must be initialized first (or concurrently, as the TCP control socket will block until both sides connect).

=== "Server node (node-01)"

    ```bash
    curl -s -X POST http://node-01:8080/v1/init \
      -H 'Content-Type: application/json' \
      -d '{"partner":"node-02","role":"server"}'
    ```

=== "Client node (node-02)"

    ```bash
    curl -s -X POST http://node-02:8080/v1/init \
      -H 'Content-Type: application/json' \
      -d '{"partner":"node-01","role":"client"}'
    ```

Expected output (server side):

```json
{"ok":true,"role":"server","partner":"node-02","cpu_freq_ghz":2.10}
```

### Example 5 — Send and receive a tagged message

Drive a send on the client side and a matching receive on the server side. The receive should be posted before or concurrently with the send to avoid unexpected delays.

=== "Receiver (node-01, server)"

    ```bash
    curl -s -X POST http://node-01:8080/v1/recv \
      -H 'Content-Type: application/json' \
      -d '{"tag":42,"size":1024,"rank":0}'
    ```

=== "Sender (node-02, client)"

    ```bash
    curl -s -X POST http://node-02:8080/v1/send \
      -H 'Content-Type: application/json' \
      -d '{"tag":42,"size":1024,"rank":0}'
    ```

Expected output (receiver):

```json
{"ok":true,"tag":42,"size":1024,"actual_length":1024,"rank":0,"elapsed_us":15.73}
```

### Example 6 — Query MQ statistics after messaging

After one or more send/receive operations, retrieve the PSM2 matched-queue statistics to inspect eager vs. rendezvous transfer counts.

```bash
curl -s http://node-01:8080/v1/stats | python3 -m json.tool
```

### Example 7 — Tear down the PSM2 connection

Shut down the PSM2 endpoint on both nodes. The HTTP server remains running and can be re-initialized with a new partner.

```bash
curl -s -X POST http://node-01:8080/v1/shutdown
curl -s -X POST http://node-02:8080/v1/shutdown
```

Expected output:

```json
{"ok":true,"message":"PSM2 shutdown complete"}
```

### Example 8 — Full scripted integration test

A complete end-to-end test driven from a single control host:

```bash
#!/bin/bash
set -e

SERVER=node-01
CLIENT=node-02
PORT=8080

# Verify both nodes are alive
curl -sf http://${SERVER}:${PORT}/v1/health > /dev/null
curl -sf http://${CLIENT}:${PORT}/v1/health > /dev/null
echo "Both nodes healthy"

# Initialize PSM2 (server first, in background)
curl -s -X POST http://${SERVER}:${PORT}/v1/init \
  -d "{\"partner\":\"${CLIENT}\",\"role\":\"server\"}" &
SERVER_PID=$!

curl -s -X POST http://${CLIENT}:${PORT}/v1/init \
  -d "{\"partner\":\"${SERVER}\",\"role\":\"client\"}"
wait ${SERVER_PID}
echo "PSM2 initialized"

# Post receive on server, then send from client
curl -s -X POST http://${SERVER}:${PORT}/v1/recv \
  -d '{"tag":1,"size":256,"rank":0}' &
RECV_PID=$!

sleep 0.1
curl -s -X POST http://${CLIENT}:${PORT}/v1/send \
  -d '{"tag":1,"size":256,"rank":0}'
wait ${RECV_PID}
echo "Message exchange complete"

# Check stats
curl -s http://${SERVER}:${PORT}/v1/stats | python3 -m json.tool

# Shutdown
curl -s -X POST http://${SERVER}:${PORT}/v1/shutdown
curl -s -X POST http://${CLIENT}:${PORT}/v1/shutdown
echo "Done"
```

## ENVIRONMENT

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `PSM2_DEVICES` | No | (all) | Restricts which HFI devices PSM2 will use. Passed through to the underlying `libpsm2` layer. |
| `PSM2_MULTIRAIL` | No | `0` | Enables multi-rail operation when multiple HFIs are present. |
| `HFI_UNIT` | No | `0` | Selects the HFI unit number for the PSM2 endpoint. |
| `PSM2_MQ_RNDV_HFI_WINDOW` | No | `131072` | Rendezvous window size in bytes. Affects the eager-to-rendezvous crossover point visible in `/v1/stats`. |
| `PSM2_TRACEMASK` | No | `0` | Bitmask enabling PSM2 internal tracing. Useful for debugging initialization failures reported by `/v1/init`. |

!!! tip
    Set `PSM2_DEVICES` and `HFI_UNIT` in the shell environment **before** starting `api_tester`. These variables are read by the PSM2 library during the `/v1/init` call and cannot be changed at runtime.

## FILES

| Path | Description |
|------|-------------|
| `api_tester/api_tester.c` | Main source file containing the HTTP server loop, request router, and all endpoint handlers. |
| `api_tester/api_tester.h` | Header file defining constants (`API_DEFAULT_PORT`, `API_VERSION`, etc.), the `psm2_api_state` structure, and handler prototypes. |
| `api_tester/Makefile` | Build rules. Compiles `api_tester` and links against `libpsm2`, `librt`, and the shared `libpsm2.o`/`psm2perf.o` objects from the parent directory. |
| `api_tester/README` | Upstream plain-text README with build instructions and a quick-start example session. |
| `../libpsm2.h` / `../libpsm2.o` | Shared PSM2 wrapper library providing `libpsm2_init()`, `libpsm2_shutdown()`, `post_isend()`, `post_irecv()`, and related functions. |
| `../psm2perf.h` / `../psm2perf.o` | Shared performance infrastructure providing `open_socket()`, `get_cpu_rate()`, `ts_diff()`, and the `SERVER_PORT` constant. |

## EXIT STATUS

| Code | Description |
|------|-------------|
| `0` | Normal exit. The server was shut down gracefully via `SIGINT`, `SIGTERM`, or the process completed without error. |
| `1` | Startup failure. The server could not create, bind, or listen on the TCP socket. Check `stderr` for `perror` output (e.g., `bind: Address already in use`). |

!!! warning
    HTTP-level errors (e.g., `404 Not Found`, `400 Bad Request`, `405 Method Not Allowed`) are returned as HTTP responses with appropriate status codes and JSON error bodies. They do **not** cause the server process to exit. The `"ok":false` field in the JSON response indicates an application-level error.

## BREAKING CHANGES

This is the initial release (`v1.0.0`). No breaking changes have been introduced.

!!! warning
    The API is currently at `v1` and should be considered unstable. Future releases may change JSON response field names, add required request body fields, or alter the TCP control port offset (currently `SERVER_PORT + 10`). Pin your automation scripts to the `/v1/` path prefix to ease future migration.

## SEE ALSO

- **Repository**: [cornelisnetworks/opa-psm2-tests](https://github.com/cornelisnetworks/opa-psm2-tests) — Parent repository containing `libpsm2`, `psm2perf`, and related PSM2 benchmark tools.
- **libpsm2**: The shared PSM2 wrapper library (`../libpsm2.h`) used by `api_tester` for endpoint initialization, send/receive, and shutdown.
- **psm2perf**: The shared performance infrastructure (`../psm2perf.h`) providing socket helpers, CPU frequency detection, and timing utilities.
- **PSM2 Programmer's Guide**: Cornelis Networks documentation for the PSM2 API (`psm2_mq_isend`, `psm2_mq_irecv`, `psm2_mq_get_stats`, etc.).
- `curl(1)` — Command-line HTTP client used in all examples.
- `psm2(7)` — Linux man page for the PSM2 library (if installed via `libpsm2-devel`).