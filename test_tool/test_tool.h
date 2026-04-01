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

#ifndef _TEST_TOOL_H_
#define _TEST_TOOL_H_

#include <stdint.h>

/* Test result codes */
#define TEST_PASS  0
#define TEST_FAIL  1
#define TEST_SKIP  2

/* Test tag values — distinct from the default 0xF used by perf tools,
 * so we can verify the tag-matching path independently. */
#define TAG_PING     0x10
#define TAG_PONG     0x11
#define TAG_DATA     0x20
#define TAG_VERIFY   0x30
#define TAG_MULTI_A  0x40
#define TAG_MULTI_B  0x41

/* Maximum payload for data-integrity tests */
#define TEST_MAX_MSG  (1 * 1048576)  /* 1 MiB */

/* Default iteration count for the ping-pong smoke test */
#define PING_ITERS    100

/* Fill pattern seed — deterministic so both sides can verify */
#define FILL_SEED     0xCAFE

/* Per-test result record */
struct test_result {
	const char *name;
	int         status;   /* TEST_PASS / TEST_FAIL / TEST_SKIP */
	double      elapsed_us;
	char        detail[256];
};

/* Forward declarations — implemented in test_tool.c */
int run_ping_pong(struct benchmark_info *info, int sock);
int run_data_integrity(struct benchmark_info *info, int sock, long msg_sz);
int run_tag_match(struct benchmark_info *info, int sock);

/* Fill buffer with deterministic pattern based on seed + offset */
static inline void fill_pattern(void *buf, long len, uint16_t seed)
{
	uint8_t *p = (uint8_t *)buf;
	long i;
	for (i = 0; i < len; i++)
		p[i] = (uint8_t)((seed + i) & 0xFF);
}

/* Verify buffer matches the expected deterministic pattern */
static inline int verify_pattern(const void *buf, long len, uint16_t seed)
{
	const uint8_t *p = (const uint8_t *)buf;
	long i;
	for (i = 0; i < len; i++) {
		if (p[i] != (uint8_t)((seed + i) & 0xFF))
			return (int)i;  /* return offset of first mismatch */
	}
	return -1;  /* -1 means all bytes match */
}

#endif /* _TEST_TOOL_H_ */
