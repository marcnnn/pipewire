/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2024 The PipeWire contributors */
/* SPDX-License-Identifier: MIT */

#ifndef PIPEWIRE_RTP_CLOCK_H
#define PIPEWIRE_RTP_CLOCK_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Clock source abstraction for RTP timestamp generation.
 *
 * Supports CLOCK_MONOTONIC (default), CLOCK_REALTIME, CLOCK_TAI,
 * and PTP Hardware Clocks (PHC) on Linux via /dev/ptpN devices.
 */
struct rtp_clock {
	clockid_t clock_id;
	int phc_fd;  /* -1 if not using PHC */
};

/**
 * Initialize an RTP clock source.
 *
 * @param clk        Clock structure to initialize
 * @param source     Clock source name: "monotonic", "realtime", "tai", or "phc"
 *                   NULL defaults to "monotonic"
 * @param phc_device PHC device path (e.g. "/dev/ptp0"), required when source="phc"
 * @return 0 on success, negative errno on error (clock falls back to CLOCK_MONOTONIC)
 */
int rtp_clock_init(struct rtp_clock *clk, const char *source, const char *phc_device);

/**
 * Destroy an RTP clock source, closing any open PHC device.
 */
void rtp_clock_destroy(struct rtp_clock *clk);

/**
 * Read the current time from the configured clock source.
 *
 * @return Current time in nanoseconds, or 0 on error
 */
uint64_t rtp_clock_gettime_ns(struct rtp_clock *clk);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_RTP_CLOCK_H */
