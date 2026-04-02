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

#ifndef _API_TESTER_H_
#define _API_TESTER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

/* -------------------------------------------------------------------
 * HTTP server configuration
 * ------------------------------------------------------------------- */

#define API_DEFAULT_PORT      8080
#define API_MAX_REQUEST_SZ    4096
#define API_MAX_RESPONSE_SZ   65536
#define API_MAX_CONNECTIONS   16
#define API_BACKLOG           8
#define API_VERSION           "1.0.0"

/* -------------------------------------------------------------------
 * JSON response helpers
 *
 * Minimal hand-rolled JSON — avoids pulling in a library dependency
 * for what amounts to six endpoints returning flat objects.
 * ------------------------------------------------------------------- */

#define JSON_OK(buf, sz, body) \
	snprintf(buf, sz, "{\"ok\":true,%s}", body)

#define JSON_ERR(buf, sz, msg) \
	snprintf(buf, sz, "{\"ok\":false,\"error\":\"%s\"}", msg)

/* -------------------------------------------------------------------
 * PSM2 connection state
 *
 * Tracks whether the PSM2 layer has been initialized so the API
 * can report readiness and refuse operations before init completes.
 * ------------------------------------------------------------------- */

struct psm2_api_state {
	int    initialized;       /* 1 after successful libpsm2_init()       */
	int    is_server;         /* 1 = server role, 0 = client role        */
	int    ctrl_sock;         /* TCP control socket to partner           */
	char   partner[256];      /* hostname of the remote side             */
	double cpu_freq;          /* cycles/sec from /proc/cpuinfo           */
	struct timespec start_ts; /* server start time for uptime            */
};

/* -------------------------------------------------------------------
 * API endpoint handlers
 *
 * Each returns the number of bytes written into resp_buf, or -1 on
 * error.  The caller owns resp_buf (stack-allocated in the request
 * loop) and is responsible for sending the HTTP wrapper.
 * ------------------------------------------------------------------- */

int handle_health(struct psm2_api_state *st, char *resp, int sz);
int handle_status(struct psm2_api_state *st, char *resp, int sz);
int handle_init(struct psm2_api_state *st, const char *body,
		char *resp, int sz);
int handle_shutdown(struct psm2_api_state *st, char *resp, int sz);
int handle_send(struct psm2_api_state *st, const char *body,
		char *resp, int sz);
int handle_recv(struct psm2_api_state *st, const char *body,
		char *resp, int sz);
int handle_stats(struct psm2_api_state *st, char *resp, int sz);

/* -------------------------------------------------------------------
 * HTTP helpers
 * ------------------------------------------------------------------- */

void send_http_response(int client_fd, int status_code,
			const char *status_text, const char *body,
			int body_len);
int  parse_request(const char *raw, int raw_len,
		   char *method, int method_sz,
		   char *path, int path_sz,
		   const char **body_start);

#endif /* _API_TESTER_H_ */
