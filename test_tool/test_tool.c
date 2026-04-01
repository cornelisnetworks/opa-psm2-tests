/*
 *  This file is provided under a dual BSD/GPLv2 license.  When using or
 *  redistributing this file, you may do so under either license.
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2026 Cornelis Networks.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  Contact Information:
 *  Cornelis Networks, www.cornelisnetworks.com
 *
 *  BSD LICENSE
 *
 *  Copyright(c) 2026 Cornelis Networks.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Cornelis Networks nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include "libpsm2.h"
#include "psm2perf.h"
#include "test_tool.h"

static char test_sbuf[TEST_MAX_MSG];
static char test_rbuf[TEST_MAX_MSG];

static int num_pass;
static int num_fail;
static int num_skip;

static void report(struct test_result *r)
{
	const char *label;

	switch (r->status) {
	case TEST_PASS:
		label = "PASS";
		num_pass++;
		break;
	case TEST_FAIL:
		label = "FAIL";
		num_fail++;
		break;
	default:
		label = "SKIP";
		num_skip++;
		break;
	}

	printf("  [%s] %-30s", label, r->name);
	if (r->elapsed_us > 0.0)
		printf("  %10.2f us", r->elapsed_us);
	if (r->detail[0])
		printf("  (%s)", r->detail);
	printf("\n");
}

/* ---------- Test 1: PSM2 ping-pong round-trip ---------- */

int run_ping_pong(struct benchmark_info *info, int sock)
{
	struct test_result res;
	struct timespec t0, t1;
	psm2_mq_req_t req;
	int i;

	memset(&res, 0, sizeof(res));
	res.name = "ping_pong";

	TIMER(t0);

	if (info->is_server) {
		for (i = 0; i < PING_ITERS; i++) {
			post_send(test_sbuf, 64, TAG_PING, info->partner);
			post_irecv(test_rbuf, 64, TAG_PONG, TAG_PONG,
				   info->partner, &req);
			psm2_mq_wait(&req, NULL);
		}
	} else {
		for (i = 0; i < PING_ITERS; i++) {
			post_irecv(test_rbuf, 64, TAG_PING, TAG_PING,
				   info->partner, &req);
			psm2_mq_wait(&req, NULL);
			post_send(test_sbuf, 64, TAG_PONG, info->partner);
		}
	}

	TIMER(t1);

	res.elapsed_us = ts_diff(t0, t1) / 1e3;
	snprintf(res.detail, sizeof(res.detail),
		 "%d iters, %.2f us/iter",
		 PING_ITERS, res.elapsed_us / PING_ITERS);
	res.status = TEST_PASS;
	report(&res);
	return res.status;
}

/* ---------- Test 2: data-integrity verification ---------- */

int run_data_integrity(struct benchmark_info *info, int sock, long msg_sz)
{
	struct test_result res;
	struct timespec t0, t1;
	psm2_mq_req_t req;
	int mismatch;

	memset(&res, 0, sizeof(res));
	res.name = "data_integrity";

	if (msg_sz > TEST_MAX_MSG)
		msg_sz = TEST_MAX_MSG;

	fill_pattern(test_sbuf, msg_sz, FILL_SEED);
	memset(test_rbuf, 0, msg_sz);

	TIMER(t0);

	if (info->is_server) {
		post_send(test_sbuf, (uint32_t)msg_sz,
			  TAG_DATA, info->partner);
		post_irecv(test_rbuf, (uint32_t)msg_sz,
			   TAG_VERIFY, TAG_VERIFY, info->partner, &req);
		psm2_mq_wait(&req, NULL);
	} else {
		post_irecv(test_rbuf, (uint32_t)msg_sz,
			   TAG_DATA, TAG_DATA, info->partner, &req);
		psm2_mq_wait(&req, NULL);

		mismatch = verify_pattern(test_rbuf, msg_sz, FILL_SEED);
		if (mismatch >= 0) {
			res.status = TEST_FAIL;
			snprintf(res.detail, sizeof(res.detail),
				 "mismatch at offset %d, got 0x%02x expected 0x%02x",
				 mismatch,
				 (unsigned char)test_rbuf[mismatch],
				 (unsigned char)((FILL_SEED + mismatch) & 0xFF));
			TIMER(t1);
			res.elapsed_us = ts_diff(t0, t1) / 1e3;
			report(&res);
			post_send(test_sbuf, (uint32_t)msg_sz,
				  TAG_VERIFY, info->partner);
			return res.status;
		}

		post_send(test_rbuf, (uint32_t)msg_sz,
			  TAG_VERIFY, info->partner);
	}

	TIMER(t1);

	if (info->is_server) {
		mismatch = verify_pattern(test_rbuf, msg_sz, FILL_SEED);
		if (mismatch >= 0) {
			res.status = TEST_FAIL;
			snprintf(res.detail, sizeof(res.detail),
				 "echo mismatch at offset %d", mismatch);
		} else {
			res.status = TEST_PASS;
			snprintf(res.detail, sizeof(res.detail),
				 "%ld bytes verified", msg_sz);
		}
	} else {
		res.status = TEST_PASS;
		snprintf(res.detail, sizeof(res.detail),
			 "%ld bytes verified", msg_sz);
	}

	res.elapsed_us = ts_diff(t0, t1) / 1e3;
	report(&res);
	return res.status;
}

/* ---------- Test 3: tag-match isolation ----------
 *
 * Send two messages with different tags concurrently, verify each
 * lands in the correct receive buffer using tag-selective matching.
 */

int run_tag_match(struct benchmark_info *info, int sock)
{
	struct test_result res;
	struct timespec t0, t1;
	psm2_mq_req_t req_a, req_b;

	memset(&res, 0, sizeof(res));
	res.name = "tag_match";

	TIMER(t0);

	if (info->is_server) {
		fill_pattern(test_sbuf, 64, 0xAA);
		fill_pattern(test_sbuf + 64, 64, 0xBB);

		post_send(test_sbuf, 64, TAG_MULTI_A, info->partner);
		post_send(test_sbuf + 64, 64, TAG_MULTI_B, info->partner);

		post_irecv(test_rbuf, 64, TAG_MULTI_A, TAG_MULTI_A,
			   info->partner, &req_a);
		post_irecv(test_rbuf + 64, 64, TAG_MULTI_B, TAG_MULTI_B,
			   info->partner, &req_b);
		psm2_mq_wait(&req_a, NULL);
		psm2_mq_wait(&req_b, NULL);

		if (verify_pattern(test_rbuf, 64, 0xAA) >= 0 ||
		    verify_pattern(test_rbuf + 64, 64, 0xBB) >= 0) {
			res.status = TEST_FAIL;
			snprintf(res.detail, sizeof(res.detail),
				 "echo data corrupted after tag-selective recv");
		} else {
			res.status = TEST_PASS;
			snprintf(res.detail, sizeof(res.detail),
				 "2 tags verified");
		}
	} else {
		post_irecv(test_rbuf, 64, TAG_MULTI_A, TAG_MULTI_A,
			   info->partner, &req_a);
		post_irecv(test_rbuf + 64, 64, TAG_MULTI_B, TAG_MULTI_B,
			   info->partner, &req_b);
		psm2_mq_wait(&req_a, NULL);
		psm2_mq_wait(&req_b, NULL);

		post_send(test_rbuf, 64, TAG_MULTI_A, info->partner);
		post_send(test_rbuf + 64, 64, TAG_MULTI_B, info->partner);
		res.status = TEST_PASS;
	}

	TIMER(t1);
	res.elapsed_us = ts_diff(t0, t1) / 1e3;
	report(&res);
	return res.status;
}

/* ---------- CLI ---------- */

static void print_usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [server] [-s size] [-a] [-h]\n"
		"\n"
		"PSM2 connection test tool — verifies connectivity and data\n"
		"integrity over the OPA fabric using the PSM2 messaging API.\n"
		"\n"
		"positional:\n"
		"  server        hostname of the server node (omit to be server)\n"
		"\n"
		"options:\n"
		"  -s SIZE       message size for data-integrity test (default 4096)\n"
		"  -a            run all tests (default: ping-pong only)\n"
		"  -h            show this help\n",
		name);
}

int main(int argc, char **argv)
{
	int ret = 0;
	int sock = -1;
	int run_all = 0;
	long data_sz = 4096;
	int c;

	struct benchmark_info *info = init_benchmark(argc, argv);
	if (info == NULL)
		return 1;

	optind = 1;
	while ((c = getopt(argc, argv, "s:ah")) != -1) {
		switch (c) {
		case 's':
			data_sz = atol(optarg);
			if (data_sz <= 0 || data_sz > TEST_MAX_MSG) {
				fprintf(stderr, "size must be 1..%d\n",
					TEST_MAX_MSG);
				free(info);
				return 1;
			}
			break;
		case 'a':
			run_all = 1;
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			free(info);
			return 0;
		}
	}

	sock = open_socket(info->server, info->is_server, SERVER_PORT + 1);
	if (sock < 0) {
		ret = 1;
		goto bail;
	}

	ret = exchange_info(sock, info);
	if (ret == -1) {
		ret = 1;
		goto bail;
	}

	ret = libpsm2_init(sock, info->is_server);
	if (ret == -1) {
		ret = 1;
		goto bail;
	}

	num_pass = num_fail = num_skip = 0;
	printf("# PSM2 Connection Test Tool\n");
	printf("# %s — %s\n", info->hostname,
	       info->is_server ? "SERVER" : "CLIENT");
	printf("#\n");

	run_ping_pong(info, sock);

	if (run_all) {
		run_data_integrity(info, sock, data_sz);
		run_tag_match(info, sock);
	}

	printf("#\n# Summary: %d passed, %d failed, %d skipped\n",
	       num_pass, num_fail, num_skip);

	if (info->show_mqstats)
		print_psm2_stats();

	libpsm2_shutdown();
	ret = (num_fail > 0) ? 1 : 0;

bail:
	if (sock >= 0)
		close(sock);
	free(info);
	return ret;
}
