/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2024 The PipeWire contributors */
/* SPDX-License-Identifier: MIT */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <spa/utils/defs.h>
#include <spa/utils/string.h>

#include <pipewire/log.h>

#include <module-rtp/rtp-clock.h>

PW_LOG_TOPIC_EXTERN(mod_topic);
#define PW_LOG_TOPIC_DEFAULT mod_topic

/*
 * Convert a file descriptor to a POSIX dynamic clock ID. This is the formula
 * the kernel uses for the dynamic POSIX clocks that back the /dev/ptpN
 * character devices, so a PHC can be read with clock_gettime(). Kept
 * identical to the definition in spa/plugins/support/node-driver.c .
 */
#ifdef __linux__
#define CLOCKFD			3
#define FD_TO_CLOCKID(fd)	((~(clockid_t) (fd) << 3) | CLOCKFD)
#endif

int rtp_clock_init(struct rtp_clock *clk, const char *source, const char *phc_device)
{
	clk->phc_fd = -1;
	clk->clock_id = CLOCK_MONOTONIC;
	clk->external = false;

	if (source == NULL || spa_streq(source, "monotonic")) {
		/* The graph driver normally runs on CLOCK_MONOTONIC as well, so
		 * this is not an external clock and no re-basing is done. */
		pw_log_debug("rtp clock source: CLOCK_MONOTONIC (default)");
		return 0;
	} else if (spa_streq(source, "realtime")) {
		clk->clock_id = CLOCK_REALTIME;
		clk->external = true;
		pw_log_info("rtp clock source: CLOCK_REALTIME");
	} else if (spa_streq(source, "tai")) {
#ifdef CLOCK_TAI
		clk->clock_id = CLOCK_TAI;
		clk->external = true;
		pw_log_info("rtp clock source: CLOCK_TAI");
#else
		pw_log_error("CLOCK_TAI is not supported on this platform, "
				"falling back to CLOCK_MONOTONIC");
		return -ENOTSUP;
#endif
	} else if (spa_streq(source, "phc")) {
#ifdef __linux__
		struct timespec ts;
		int fd, res;

		if (phc_device == NULL) {
			pw_log_error("rtp.clock-source=phc requires rtp.phc-device, "
					"falling back to CLOCK_MONOTONIC");
			return -EINVAL;
		}

		if ((fd = open(phc_device, O_RDONLY | O_CLOEXEC)) < 0) {
			res = -errno;
			pw_log_error("failed to open PHC device %s: %m, "
					"falling back to CLOCK_MONOTONIC", phc_device);
			return res;
		}

		/* Verify that the clock can actually be read before committing
		 * to it, otherwise every timestamp would silently be 0. */
		if (clock_gettime(FD_TO_CLOCKID(fd), &ts) < 0) {
			res = -errno;
			pw_log_error("PHC device %s opened but clock_gettime() failed: %m, "
					"falling back to CLOCK_MONOTONIC", phc_device);
			close(fd);
			return res;
		}

		clk->phc_fd = fd;
		clk->clock_id = FD_TO_CLOCKID(fd);
		clk->external = true;

		pw_log_info("rtp clock source: PHC device %s (fd:%d clock_id:%d)",
				phc_device, fd, (int)clk->clock_id);
#else
		pw_log_error("the phc clock source is only supported on Linux, "
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
		pw_log_debug("closing PHC device fd:%d", clk->phc_fd);
		close(clk->phc_fd);
		clk->phc_fd = -1;
	}
	clk->clock_id = CLOCK_MONOTONIC;
	clk->external = false;
}

uint64_t rtp_clock_gettime_ns(struct rtp_clock *clk)
{
	struct timespec ts;

	if (clock_gettime(clk->clock_id, &ts) < 0)
		return 0;

	return SPA_TIMESPEC_TO_NSEC(&ts);
}
