/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2024 The PipeWire contributors */
/* SPDX-License-Identifier: MIT */

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <spa/utils/defs.h>
#include <spa/utils/string.h>

#include <pipewire/log.h>

#include "rtp-clock.h"

PW_LOG_TOPIC_EXTERN(mod_topic);
#define PW_LOG_TOPIC_DEFAULT mod_topic

/*
 * Convert a file descriptor to a POSIX dynamic clock ID.
 * This is the standard Linux kernel formula used by <linux/posix-timers.h>.
 * PHC devices (/dev/ptpN) can be used with clock_gettime() via this ID.
 */
#ifdef __linux__
#ifndef FD_TO_CLOCKID
#define FD_TO_CLOCKID(fd) ((clockid_t)((((unsigned int) ~(fd)) << 3) | 3))
#endif
#endif

int rtp_clock_init(struct rtp_clock *clk, const char *source, const char *phc_device)
{
	clk->phc_fd = -1;
	clk->clock_id = CLOCK_MONOTONIC;

	if (source == NULL || spa_streq(source, "monotonic")) {
		clk->clock_id = CLOCK_MONOTONIC;
		pw_log_info("rtp clock source: CLOCK_MONOTONIC");
	} else if (spa_streq(source, "realtime")) {
		clk->clock_id = CLOCK_REALTIME;
		pw_log_info("rtp clock source: CLOCK_REALTIME");
	} else if (spa_streq(source, "tai")) {
#ifdef CLOCK_TAI
		clk->clock_id = CLOCK_TAI;
		pw_log_info("rtp clock source: CLOCK_TAI");
#else
		pw_log_error("CLOCK_TAI not supported on this platform, "
			     "falling back to CLOCK_MONOTONIC");
		clk->clock_id = CLOCK_MONOTONIC;
#endif
	} else if (spa_streq(source, "phc")) {
#ifdef __linux__
		struct timespec ts;

		if (phc_device == NULL) {
			pw_log_error("rtp.clock-source=phc requires rtp.phc-device, "
				     "falling back to CLOCK_MONOTONIC");
			return -EINVAL;
		}

		clk->phc_fd = open(phc_device, O_RDONLY | O_CLOEXEC);
		if (clk->phc_fd < 0) {
			int err = errno;
			pw_log_error("failed to open PHC device %s: %m, "
				     "falling back to CLOCK_MONOTONIC", phc_device);
			return -err;
		}

		clk->clock_id = FD_TO_CLOCKID(clk->phc_fd);

		/* Verify the clock is readable */
		if (clock_gettime(clk->clock_id, &ts) < 0) {
			int err = errno;
			pw_log_error("PHC device %s opened but clock_gettime failed: %m, "
				     "falling back to CLOCK_MONOTONIC", phc_device);
			close(clk->phc_fd);
			clk->phc_fd = -1;
			clk->clock_id = CLOCK_MONOTONIC;
			return -err;
		}

		pw_log_info("rtp clock source: PHC device %s (fd=%d, clock_id=%d)",
			    phc_device, clk->phc_fd, (int)clk->clock_id);
#else
		pw_log_error("PHC clock source is only supported on Linux, "
			     "falling back to CLOCK_MONOTONIC");
		return -ENOTSUP;
#endif
	} else {
		pw_log_error("unknown rtp.clock-source '%s', "
			     "falling back to CLOCK_MONOTONIC", source);
		return -EINVAL;
	}

	return 0;
}

void rtp_clock_destroy(struct rtp_clock *clk)
{
	if (clk->phc_fd >= 0) {
		pw_log_info("closing PHC device fd=%d", clk->phc_fd);
		close(clk->phc_fd);
		clk->phc_fd = -1;
	}
	clk->clock_id = CLOCK_MONOTONIC;
}

uint64_t rtp_clock_gettime_ns(struct rtp_clock *clk)
{
	struct timespec ts;

	if (clock_gettime(clk->clock_id, &ts) < 0)
		return 0;

	return SPA_TIMESPEC_TO_NSEC(&ts);
}
