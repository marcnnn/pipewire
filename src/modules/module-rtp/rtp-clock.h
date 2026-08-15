/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2024 The PipeWire contributors */
/* SPDX-License-Identifier: MIT */

#ifndef PIPEWIRE_RTP_CLOCK_H
#define PIPEWIRE_RTP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <spa/utils/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Clock source abstraction for RTP timestamp generation.
 *
 * Supports CLOCK_MONOTONIC (the default, and the same time base the graph
 * driver normally uses), CLOCK_REALTIME, CLOCK_TAI, and PTP Hardware Clocks
 * (PHC) on Linux via /dev/ptpN devices.
 *
 * This is only used to re-base the RTP timestamps of a stream onto a clock
 * that differs from the one that drives the graph. If the graph driver
 * itself already runs off the PHC (see the support.node.driver SPA node
 * with its clock.device / clock.interface properties, as set up by
 * pipewire-aes67.conf), then this is not needed at all, since the RTP
 * timestamps are derived from spa_io_clock::position in that case.
 */
struct rtp_clock {
	clockid_t clock_id;
	int phc_fd;		/**< -1 if not using PHC */
	bool external;		/**< true when a non-default clock source is in use */
};

/**
 * Initialize an RTP clock source.
 *
 * \param clk        Clock structure to initialize
 * \param source     Clock source name: "monotonic", "realtime", "tai", or "phc".
 *                   NULL or "monotonic" selects the default (non-external) clock.
 * \param phc_device PHC device path (e.g. "/dev/ptp0"), required when source is "phc"
 * \return 0 on success, negative errno on error. On error the clock falls back
 *         to CLOCK_MONOTONIC and \a clk->external is false.
 */
int rtp_clock_init(struct rtp_clock *clk, const char *source, const char *phc_device);

/**
 * Destroy an RTP clock source, closing any open PHC device.
 *
 * Safe to call on a clock that was initialized with rtp_clock_init(), and
 * only on such a clock (a zero-initialized struct has phc_fd == 0, which is
 * a valid file descriptor).
 */
void rtp_clock_destroy(struct rtp_clock *clk);

/**
 * Read the current time from the configured clock source.
 *
 * \return Current time in nanoseconds, or 0 on error
 */
uint64_t rtp_clock_gettime_ns(struct rtp_clock *clk);

/** True when the clock is a different time base than the graph driver clock. */
static inline bool rtp_clock_is_external(const struct rtp_clock *clk)
{
	return clk->external;
}

/**
 * Convert nanoseconds to sample ticks at the given rate.
 *
 * This is done in integer arithmetic on purpose: the values coming from a
 * PHC or from CLOCK_TAI are nanoseconds since an epoch (~1.8e18 by now),
 * which a double cannot represent with sample accuracy, and a plain
 * (nsec * rate) would overflow 64 bits.
 */
static inline uint64_t rtp_clock_ns_to_samples(uint64_t nsec, uint32_t rate)
{
	return (nsec / SPA_NSEC_PER_SEC) * rate +
		((nsec % SPA_NSEC_PER_SEC) * rate) / SPA_NSEC_PER_SEC;
}

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_RTP_CLOCK_H */
