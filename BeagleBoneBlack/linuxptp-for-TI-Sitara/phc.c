/**
 * @file phc.c
 * @note Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/ptp_clock.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "phc.h"
#include "print.h"

/*
 * On 32 bit platforms, the PHC driver's maximum adjustment (type
 * 'int' in units of ppb) can overflow the timex.freq field (type
 * 'long'). So in this case we clamp the maximum to the largest
 * possible adjustment that fits into a 32 bit long.
 */
#define BITS_PER_LONG	(sizeof(long)*8)
#define MAX_PPB_32	32767999	/* 2^31 - 1 / 65.536 */

static int phc_get_caps(clockid_t clkid, struct ptp_clock_caps *caps);

clockid_t phc_open(const char *phc)
{
	clockid_t clkid;
	struct timespec ts;
	struct timex tx;
	int fd;

	memset(&tx, 0, sizeof(tx));

	fd = open(phc, O_RDWR);
	if (fd < 0)
		return CLOCK_INVALID;

	clkid = FD_TO_CLOCKID(fd);
	/* check if clkid is valid */
	if (clock_gettime(clkid, &ts)) {
		close(fd);
		return CLOCK_INVALID;
	}
	if (clock_adjtime(clkid, &tx)) {
		close(fd);
		return CLOCK_INVALID;
	}

	return clkid;
}

void phc_close(clockid_t clkid)
{
	if (clkid == CLOCK_INVALID)
		return;

	close(CLOCKID_TO_FD(clkid));
}

static int phc_get_caps(clockid_t clkid, struct ptp_clock_caps *caps)
{
	int fd = CLOCKID_TO_FD(clkid), err;

	err = ioctl(fd, PTP_CLOCK_GETCAPS, caps);
	if (err)
		perror("PTP_CLOCK_GETCAPS");
	return err;
}

int phc_max_adj(clockid_t clkid)
{
	int max;
	struct ptp_clock_caps caps;

	if (phc_get_caps(clkid, &caps))
		return 0;

	max = caps.max_adj;

	if (BITS_PER_LONG == 32 && max > MAX_PPB_32)
		max = MAX_PPB_32;

	return max;
}

int phc_number_pins(clockid_t clkid)
{
	struct ptp_clock_caps caps;

	if (phc_get_caps(clkid, &caps)) {
		return 0;
	}
	return caps.n_pins;
}

int phc_pin_setfunc(clockid_t clkid, struct ptp_pin_desc *desc)
{
	int err = ioctl(CLOCKID_TO_FD(clkid), PTP_PIN_SETFUNC2, desc);
	if (err) {
		fprintf(stderr, PTP_PIN_SETFUNC_FAILED "\n");
	}
	return err;
}

int phc_has_pps(clockid_t clkid)
{
	struct ptp_clock_caps caps;

	if (phc_get_caps(clkid, &caps))
		return 0;
	return caps.pps;
}

int phc_enable_extts(clockid_t clkid, int chan_index)
{
	struct ptp_extts_request extts_request;
	int fd = CLOCKID_TO_FD(clkid), err;

	memset(&extts_request, 0, sizeof(extts_request));
	extts_request.index = chan_index;
	extts_request.flags = PTP_ENABLE_FEATURE;
	err = ioctl(fd, PTP_EXTTS_REQUEST, &extts_request);
	if (err)
		pr_err("Enabling PTP_EXTTS_REQUEST on ptp dev: FAILED\n");

	return err;
}

int phc_disable_extts(clockid_t clkid, int chan_index)
{
	struct ptp_extts_request extts_request;
	int fd = CLOCKID_TO_FD(clkid), err;

	memset(&extts_request, 0, sizeof(extts_request));
	extts_request.index = chan_index;
	extts_request.flags = 0;
	err = ioctl(fd, PTP_EXTTS_REQUEST, &extts_request);
	if (err)
		pr_err("Disabling PTP_EXTTS_REQUEST on ptp dev: FAILED\n");

	return err;
}

int phc_read_extts(clockid_t clkid, uint64_t *ts)
{
	int fd = CLOCKID_TO_FD(clkid), count;
	struct ptp_extts_event event;

	count = read(fd, &event, sizeof(event));
	if (count != sizeof(event)) {
		pr_err("PTP event read %d: FAILED\n", count);
		return -EINVAL;
	}

	*ts = event.t.sec * 1000000000ULL + event.t.nsec;

	return 0;
}

int phc_has_writephase(clockid_t clkid)
{
	struct ptp_clock_caps caps;

	if (phc_get_caps(clkid, &caps)) {
		return 0;
	}
	return caps.adjust_phase;
}
