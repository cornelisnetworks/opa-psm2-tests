/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2021 Cornelis Networks.
  Copyright(c) 2018 Intel Corporation.

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
  Copyright(c) 2018 Intel Corporation.

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

#include <stdio.h>
#include <stdlib.h>

#include "libpsm2.h"
#include "psm2perf.h"

int run_bi_bw_mrate(struct benchmark_info *info, int sock);

int main(int argc, char **argv)
{
	int ret = 0;
	int sock = -1;

	struct benchmark_info *info = init_benchmark(argc, argv);
	if (info == NULL) {
		ret = -1;
		goto bail;
	}

	sock = open_socket(info->server, info->is_server, SERVER_PORT);
	if (sock < 0) {
		ret = -1;
		goto bail;
	}

	ret = exchange_info(sock, info);
	if (ret == -1)
		goto bail;

	ret = libpsm2_init(sock, info->is_server);
	if (ret == -1)
		goto bail;

	if (info->run_flush) {
		printf("Flushing L3 Cache... will take a few seconds\n");
		printf("Flushed L3 cache (%ld)\n",flush_l3cache());
	}

	ret = run_bi_bw_mrate(info, sock);
	if (info->show_mqstats)
		print_psm2_stats();
	libpsm2_shutdown();

bail:
	if (sock >= 0)
		close(sock);
	if (info != NULL)
		free(info);
	return ret;
}

int run_bi_bw_mrate(struct benchmark_info *info, int sock)
{
	int msize, i, w;
	psm2_mq_req_t sreq[WINDOW], rreq[WINDOW];
	struct timespec time_start;
	struct timespec time_end;
	double te, bw, mrate;
	int iters[2] = {ITERS_LARGE, ITERS_SMALL}, iter = iters[0];

	printf("# PSM2 Bi-directional Bandwidth, Message Rate Test\n");
	printf("# Message Size(B)  Bandwidth(MB/s)  Message Rate(Mmps)\n");

	for (msize = info->min_msg_sz; msize <= info->max_msg_sz; msize *= 2) {
		if (msize > LARGE_MSG)
			iter = iters[1];

		if (info->is_server) {
			// warmup
			for (i = 0; i < iter; i++) {
				for (w = 0; w < WINDOW; w++)
					post_irecv(rbuff, msize, PSM2_TAG, PSM2_TAGSEL, info->partner, &sreq[w]);
				for (w = 0; w < WINDOW; w++)
					post_isend(sbuff, msize, PSM2_TAG, info->partner, &rreq[w]);

				psm2_waitall(WINDOW, &rreq[0], NULL);
				psm2_waitall(WINDOW, &sreq[0], NULL);
			}

			TIMER(time_start);
			for (i = 0; i < iter; i++) {
				for (w = 0; w < WINDOW; w++)
					post_irecv(rbuff, msize, PSM2_TAG, PSM2_TAGSEL, info->partner, &sreq[w]);
				for (w = 0; w < WINDOW; w++)
					post_isend(sbuff, msize, PSM2_TAG, info->partner, &rreq[w]);

				psm2_waitall(WINDOW, &rreq[0], NULL);
				psm2_waitall(WINDOW, &sreq[0], NULL);
			}
			TIMER(time_end);

			te = ts_diff(time_start, time_end);
			// msize is on the order of 1e6 but the timer is on the order
			// of 1e9, so we need to scale the result back up by 1e3 to get
			// a result in Mbps.
			bw = 2 * msize / te * iter * WINDOW * 1e3; 
			mrate = bw / msize;
		} else {
			for (i = 0; i < 2 * iter; i++) {
				for (w = 0; w < WINDOW; w++)
					post_irecv(rbuff, msize, PSM2_TAG, PSM2_TAGSEL, info->partner, &rreq[w]);
				for (w = 0; w < WINDOW; w++)
					post_isend(sbuff, msize, PSM2_TAG, info->partner, &sreq[w]);

				psm2_waitall(WINDOW, &rreq[0], NULL);
				psm2_waitall(WINDOW, &sreq[0], NULL);
			}
		}

		if (info->is_server) {
			SEND(sock, bw, double);
			SEND(sock, mrate, double);
		} else {
			RECV(sock, bw, double);
			RECV(sock, mrate, double);
		}

		printf("%-15d  %15.2f  %18.2f\n",
				msize, bw, mrate);
	}

	return 0;
bail:
	return -1;
}
