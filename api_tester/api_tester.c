/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2021 Cornelis Networks.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  Cornelis Networks, www.cornelisnetworks.com

  BSD LICENSE

  Copyright(c) 2021 Cornelis Networks.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Cornelis Networks nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* -------------------------------------------------------------------
 * api_tester.c — REST API server for PSM2 operations
 *
 * Provides a minimal HTTP/1.1 server that wraps the PSM2 messaging
 * library.  Operators can initialize a PSM2 connection, send/receive
 * tagged messages, query MQ statistics, and tear down the connection
 * — all through simple curl commands.
 *
 * Designed for integration testing and remote orchestration of PSM2
 * benchmarks without requiring SSH access to the test nodes.
 * ------------------------------------------------------------------- */

#include "api_tester.h"
#include "../libpsm2.h"
#include "../psm2perf.h"

/* Global state — single-connection server, no concurrency */
static struct psm2_api_state g_state;
static volatile int g_running = 1;

/* -------------------------------------------------------------------
 * Signal handler — graceful shutdown on SIGINT/SIGTERM
 * ------------------------------------------------------------------- */
static void sig_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

/* -------------------------------------------------------------------
 * HTTP helpers
 * ------------------------------------------------------------------- */

void send_http_response(int client_fd, int status_code,
			const char *status_text, const char *body,
			int body_len)
{
	char header[512];
	int hlen;

	hlen = snprintf(header, sizeof(header),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n",
		status_code, status_text, body_len);

	send(client_fd, header, hlen, 0);
	if (body_len > 0)
		send(client_fd, body, body_len, 0);
}

/* Parses "METHOD /path HTTP/1.x\r\n...body" into components.
 * Returns 0 on success, -1 on parse failure.
 */
int parse_request(const char *raw, int raw_len,
		  char *method, int method_sz,
		  char *path, int path_sz,
		  const char **body_start)
{
	const char *sp1, *sp2, *eol;

	(void)raw_len;

	sp1 = strchr(raw, ' ');
	if (!sp1)
		return -1;

	sp2 = strchr(sp1 + 1, ' ');
	if (!sp2)
		return -1;

	/* Extract method */
	if ((sp1 - raw) >= method_sz)
		return -1;
	memcpy(method, raw, sp1 - raw);
	method[sp1 - raw] = '\0';

	/* Extract path */
	if ((sp2 - sp1 - 1) >= path_sz)
		return -1;
	memcpy(path, sp1 + 1, sp2 - sp1 - 1);
	path[sp2 - sp1 - 1] = '\0';

	/* Body starts after the blank line (\r\n\r\n) */
	*body_start = strstr(raw, "\r\n\r\n");
	if (*body_start)
		*body_start += 4;
	else {
		/* Try \n\n as fallback */
		eol = strstr(raw, "\n\n");
		if (eol)
			*body_start = eol + 2;
		else
			*body_start = NULL;
	}

	return 0;
}

/* -------------------------------------------------------------------
 * Minimal JSON field extractor
 *
 * Finds "key":"value" or "key":number in a flat JSON object.
 * No nesting support — sufficient for our flat request bodies.
 * Returns pointer to value start, writes length to *vlen.
 * ------------------------------------------------------------------- */
static const char *json_find(const char *json, const char *key, int *vlen)
{
	char pattern[128];
	const char *p, *end;

	snprintf(pattern, sizeof(pattern), "\"%s\":", key);
	p = strstr(json, pattern);
	if (!p)
		return NULL;

	p += strlen(pattern);
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '"') {
		/* String value */
		p++;
		end = strchr(p, '"');
		if (!end)
			return NULL;
		*vlen = end - p;
	} else {
		/* Numeric or boolean */
		end = p;
		while (*end && *end != ',' && *end != '}' && *end != ' ')
			end++;
		*vlen = end - p;
	}

	return p;
}

static int json_get_str(const char *json, const char *key,
			char *out, int out_sz)
{
	int vlen;
	const char *v = json_find(json, key, &vlen);

	if (!v)
		return -1;
	if (vlen >= out_sz)
		vlen = out_sz - 1;
	memcpy(out, v, vlen);
	out[vlen] = '\0';
	return 0;
}

static int json_get_int(const char *json, const char *key, int *out)
{
	int vlen;
	const char *v = json_find(json, key, &vlen);

	if (!v)
		return -1;
	*out = atoi(v);
	return 0;
}

/* -------------------------------------------------------------------
 * GET /v1/health
 *
 * Always succeeds — indicates the HTTP server process is alive.
 * ------------------------------------------------------------------- */
int handle_health(struct psm2_api_state *st, char *resp, int sz)
{
	(void)st;
	return snprintf(resp, sz,
		"{\"ok\":true,\"service\":\"psm2-api\","
		"\"version\":\"%s\"}", API_VERSION);
}

/* -------------------------------------------------------------------
 * GET /v1/status
 *
 * Reports PSM2 connection state, role, partner, and uptime.
 * ------------------------------------------------------------------- */
int handle_status(struct psm2_api_state *st, char *resp, int sz)
{
	struct timespec now;
	double uptime_sec = 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	uptime_sec = (now.tv_sec - st->start_ts.tv_sec) +
		     (now.tv_nsec - st->start_ts.tv_nsec) / 1e9;

	return snprintf(resp, sz,
		"{\"ok\":true,"
		"\"psm2_initialized\":%s,"
		"\"role\":\"%s\","
		"\"partner\":\"%s\","
		"\"cpu_freq_ghz\":%.2f,"
		"\"uptime_seconds\":%.1f}",
		st->initialized ? "true" : "false",
		st->is_server ? "server" : "client",
		st->partner[0] ? st->partner : "none",
		st->cpu_freq / 1e9,
		uptime_sec);
}

/* -------------------------------------------------------------------
 * POST /v1/init
 *
 * Body: {"partner":"<hostname>", "role":"server"|"client"}
 *
 * Opens a TCP control socket to the partner, then calls
 * libpsm2_init() to establish the PSM2 endpoint and MQ.
 * ------------------------------------------------------------------- */
int handle_init(struct psm2_api_state *st, const char *body,
		char *resp, int sz)
{
	char role[16] = {0};
	int sock, ret;

	if (st->initialized)
		return JSON_ERR(resp, sz, "already initialized");

	if (!body || strlen(body) < 2)
		return JSON_ERR(resp, sz, "missing request body");

	if (json_get_str(body, "partner", st->partner,
			 sizeof(st->partner)) != 0)
		return JSON_ERR(resp, sz, "missing 'partner' field");

	if (json_get_str(body, "role", role, sizeof(role)) != 0)
		return JSON_ERR(resp, sz, "missing 'role' field");

	if (strcmp(role, "server") == 0)
		st->is_server = 1;
	else if (strcmp(role, "client") == 0)
		st->is_server = 0;
	else
		return JSON_ERR(resp, sz, "role must be 'server' or 'client'");

	/* Open TCP control channel — reuses the psm2perf infrastructure */
	sock = open_socket(st->partner, st->is_server, SERVER_PORT + 10);
	if (sock < 0)
		return JSON_ERR(resp, sz, "TCP control socket failed");

	st->ctrl_sock = sock;

	/* Initialize PSM2 — UUID exchange, endpoint open, MQ init */
	ret = libpsm2_init(sock, st->is_server);
	if (ret != 0) {
		close(sock);
		st->ctrl_sock = -1;
		return JSON_ERR(resp, sz, "libpsm2_init failed");
	}

	st->initialized = 1;
	st->cpu_freq = get_cpu_rate() * 1e6;

	return snprintf(resp, sz,
		"{\"ok\":true,\"role\":\"%s\",\"partner\":\"%s\","
		"\"cpu_freq_ghz\":%.2f}",
		role, st->partner, st->cpu_freq / 1e9);
}

/* -------------------------------------------------------------------
 * POST /v1/shutdown
 *
 * Tears down the PSM2 endpoint and closes the control socket.
 * Does NOT stop the HTTP server — it continues accepting requests.
 * ------------------------------------------------------------------- */
int handle_shutdown(struct psm2_api_state *st, char *resp, int sz)
{
	if (!st->initialized)
		return JSON_ERR(resp, sz, "not initialized");

	libpsm2_shutdown();

	if (st->ctrl_sock >= 0) {
		close(st->ctrl_sock);
		st->ctrl_sock = -1;
	}

	st->initialized = 0;
	st->partner[0] = '\0';

	return JSON_OK(resp, sz, "\"message\":\"PSM2 shutdown complete\"");
}

/* -------------------------------------------------------------------
 * POST /v1/send
 *
 * Body: {"tag":15, "size":64, "rank":0}
 *
 * Sends a tagged message filled with a predictable pattern.
 * ------------------------------------------------------------------- */
int handle_send(struct psm2_api_state *st, const char *body,
		char *resp, int sz)
{
	int tag = PSM2_TAG, msg_sz = 64, rank = 0;
	char buf[MAX_MSG_SZ];
	psm2_mq_req_t req;
	struct timespec t0, t1;
	double elapsed_us;
	int i;

	if (!st->initialized)
		return JSON_ERR(resp, sz, "not initialized");

	if (body && strlen(body) > 2) {
		json_get_int(body, "tag", &tag);
		json_get_int(body, "size", &msg_sz);
		json_get_int(body, "rank", &rank);
	}

	if (msg_sz < 1 || msg_sz > MAX_MSG_SZ)
		return JSON_ERR(resp, sz, "size out of range (1..4194304)");

	/* Fill with deterministic pattern for verification */
	for (i = 0; i < msg_sz; i++)
		buf[i] = (char)((i * 0xCB + tag) & 0xFF);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	post_isend(buf, msg_sz, (uint64_t)tag, rank, &req);
	psm2_waitall(1, &req, NULL);
	clock_gettime(CLOCK_MONOTONIC, &t1);

	elapsed_us = ts_diff(t0, t1) / 1000.0;

	return snprintf(resp, sz,
		"{\"ok\":true,\"tag\":%d,\"size\":%d,\"rank\":%d,"
		"\"elapsed_us\":%.2f}",
		tag, msg_sz, rank, elapsed_us);
}

/* -------------------------------------------------------------------
 * POST /v1/recv
 *
 * Body: {"tag":15, "tagsel":15, "size":64, "rank":0}
 *
 * Posts a tagged receive and waits for completion.
 * ------------------------------------------------------------------- */
int handle_recv(struct psm2_api_state *st, const char *body,
		char *resp, int sz)
{
	int tag = PSM2_TAG, tagsel = PSM2_TAGSEL, msg_sz = 64, rank = 0;
	char buf[MAX_MSG_SZ];
	psm2_mq_req_t req;
	psm2_mq_status_t status;
	struct timespec t0, t1;
	double elapsed_us;
	uint32_t actual_len;

	if (!st->initialized)
		return JSON_ERR(resp, sz, "not initialized");

	if (body && strlen(body) > 2) {
		json_get_int(body, "tag", &tag);
		json_get_int(body, "tagsel", &tagsel);
		json_get_int(body, "size", &msg_sz);
		json_get_int(body, "rank", &rank);
	}

	if (msg_sz < 1 || msg_sz > MAX_MSG_SZ)
		return JSON_ERR(resp, sz, "size out of range (1..4194304)");

	memset(buf, 0, msg_sz);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	post_irecv(buf, msg_sz, (uint64_t)tag, (uint64_t)tagsel,
		   rank, &req);
	psm2_waitall(1, &req, &status);
	clock_gettime(CLOCK_MONOTONIC, &t1);

	elapsed_us = ts_diff(t0, t1) / 1000.0;
	actual_len = status.msg_length;

	return snprintf(resp, sz,
		"{\"ok\":true,\"tag\":%d,\"size\":%d,"
		"\"actual_length\":%u,\"rank\":%d,"
		"\"elapsed_us\":%.2f}",
		tag, msg_sz, actual_len, rank, elapsed_us);
}

/* -------------------------------------------------------------------
 * GET /v1/stats
 *
 * Returns PSM2 MQ statistics (rx/tx bytes, counts, eager/rndv split).
 * ------------------------------------------------------------------- */
int handle_stats(struct psm2_api_state *st, char *resp, int sz)
{
	psm2_mq_stats_t stats;

	if (!st->initialized)
		return JSON_ERR(resp, sz, "not initialized");

	psm2_mq_get_stats(libpsm2_mq, &stats);

	return snprintf(resp, sz,
		"{\"ok\":true,"
		"\"rx_user_bytes\":%lu,\"rx_user_num\":%lu,"
		"\"rx_sys_bytes\":%lu,\"rx_sys_num\":%lu,"
		"\"tx_num\":%lu,"
		"\"tx_eager_num\":%lu,\"tx_eager_bytes\":%lu,"
		"\"tx_rndv_num\":%lu,\"tx_rndv_bytes\":%lu,"
		"\"tx_shm_num\":%lu,\"rx_shm_num\":%lu,"
		"\"rx_sysbuf_num\":%lu,\"rx_sysbuf_bytes\":%lu}",
		stats.rx_user_bytes, stats.rx_user_num,
		stats.rx_sys_bytes, stats.rx_sys_num,
		stats.tx_num,
		stats.tx_eager_num, stats.tx_eager_bytes,
		stats.tx_rndv_num, stats.tx_rndv_bytes,
		stats.tx_shm_num, stats.rx_shm_num,
		stats.rx_sysbuf_num, stats.rx_sysbuf_bytes);
}

/* -------------------------------------------------------------------
 * Request router
 *
 * Maps method + path to the appropriate handler.
 * ------------------------------------------------------------------- */
static void handle_request(int client_fd, const char *raw, int raw_len)
{
	char method[16], path[256];
	const char *body = NULL;
	char resp[API_MAX_RESPONSE_SZ];
	int resp_len;

	if (parse_request(raw, raw_len, method, sizeof(method),
			  path, sizeof(path), &body) != 0) {
		resp_len = JSON_ERR(resp, sizeof(resp), "malformed request");
		send_http_response(client_fd, 400, "Bad Request",
				   resp, resp_len);
		return;
	}

	/* Route: GET endpoints */
	if (strcmp(method, "GET") == 0) {
		if (strcmp(path, "/v1/health") == 0) {
			resp_len = handle_health(&g_state, resp, sizeof(resp));
			send_http_response(client_fd, 200, "OK",
					   resp, resp_len);
		} else if (strcmp(path, "/v1/status") == 0) {
			resp_len = handle_status(&g_state, resp, sizeof(resp));
			send_http_response(client_fd, 200, "OK",
					   resp, resp_len);
		} else if (strcmp(path, "/v1/stats") == 0) {
			resp_len = handle_stats(&g_state, resp, sizeof(resp));
			send_http_response(client_fd, 200, "OK",
					   resp, resp_len);
		} else {
			resp_len = JSON_ERR(resp, sizeof(resp), "not found");
			send_http_response(client_fd, 404, "Not Found",
					   resp, resp_len);
		}
		return;
	}

	/* Route: POST endpoints */
	if (strcmp(method, "POST") == 0) {
		if (strcmp(path, "/v1/init") == 0) {
			resp_len = handle_init(&g_state, body,
					       resp, sizeof(resp));
		} else if (strcmp(path, "/v1/shutdown") == 0) {
			resp_len = handle_shutdown(&g_state,
						   resp, sizeof(resp));
		} else if (strcmp(path, "/v1/send") == 0) {
			resp_len = handle_send(&g_state, body,
					       resp, sizeof(resp));
		} else if (strcmp(path, "/v1/recv") == 0) {
			resp_len = handle_recv(&g_state, body,
					       resp, sizeof(resp));
		} else {
			resp_len = JSON_ERR(resp, sizeof(resp), "not found");
			send_http_response(client_fd, 404, "Not Found",
					   resp, resp_len);
			return;
		}
		send_http_response(client_fd, 200, "OK", resp, resp_len);
		return;
	}

	resp_len = JSON_ERR(resp, sizeof(resp), "method not allowed");
	send_http_response(client_fd, 405, "Method Not Allowed",
			   resp, resp_len);
}

/* -------------------------------------------------------------------
 * Main — HTTP server loop
 * ------------------------------------------------------------------- */
int main(int argc, char **argv)
{
	int server_fd, client_fd, port = API_DEFAULT_PORT;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	char req_buf[API_MAX_REQUEST_SZ];
	int opt = 1, n;

	/* Parse optional port argument */
	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0 ||
		    strcmp(argv[1], "--help") == 0) {
			printf("Usage: %s [-p port] [-h]\n"
			       "  -p port  HTTP listen port (default %d)\n"
			       "  -h       Show this help\n",
			       argv[0], API_DEFAULT_PORT);
			return 0;
		}
		if (strcmp(argv[1], "-p") == 0 && argc > 2)
			port = atoi(argv[2]);
	}

	/* Initialize state */
	memset(&g_state, 0, sizeof(g_state));
	g_state.ctrl_sock = -1;
	clock_gettime(CLOCK_MONOTONIC, &g_state.start_ts);

	/* Signal handling */
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGPIPE, SIG_IGN);

	/* Create listening socket */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("socket");
		return 1;
	}

	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(server_fd);
		return 1;
	}

	if (listen(server_fd, API_BACKLOG) < 0) {
		perror("listen");
		close(server_fd);
		return 1;
	}

	printf("PSM2 API Tester v%s listening on port %d\n",
	       API_VERSION, port);

	/* Accept loop */
	while (g_running) {
		client_fd = accept(server_fd, (struct sockaddr *)&addr,
				   &addr_len);
		if (client_fd < 0) {
			if (errno == EINTR)
				continue;
			perror("accept");
			break;
		}

		n = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
		if (n > 0) {
			req_buf[n] = '\0';
			handle_request(client_fd, req_buf, n);
		}

		close(client_fd);
	}

	/* Cleanup */
	printf("\nShutting down...\n");
	if (g_state.initialized)
		handle_shutdown(&g_state, req_buf, sizeof(req_buf));
	close(server_fd);

	return 0;
}
