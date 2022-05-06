/**
 * @file phc2pwm.c
 * @brief Utility program to synchronize a pwm with ptp clock to generate PPS.
 * 	  This is mainly intended for the below hardware configuration:
 * 	  - A PTP supporting IP that is not capable of generating PPS signal
 * 	  - A PWM signal that is connected to the above PTP hardware
 * 	  - On every rising edge of PWM, PTP hardware should be able to send
 * 	    the current timestamp to userspace.
 * 	  - This PWM signal can be used as a PPS signal that is synchronized to
 * 	    PTP clock.
 *
 * @assumptions:
 * 	- PWM period and duty_cycle can be updated on the fly.
 * 	- Update to PWM period and duty_cycle gets reflected in next cycle.
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "pwm.h"
#include "phc.h"
#include "missing.h"
#include "print.h"
#include "version.h"

#define NS_PER_SEC		1000000000LL
#define PWM_PERIOD		1000000000
#define PWM_DUTY_CYCLE		300000000

static void usage(char *progname)
{
	fprintf(stderr,
		"\n"
		"usage: %s [options]\n\n"
		"\n"
		" -c [id]	PWM channel id from PWM chip\n"
		" -e [id]	PTP index for event/trigger\n"
		" -h		prints this message and exits\n"
		" -l [num]	set the logging level to 'num'\n"
		" -m            print messages to stdout\n"
		" -p [dev]	Clock device to use\n"
		" -v		prints the software version and exits\n"
		" -w [id]	PWM chip device id\n"
		"\n",
		progname);
}

#define PWM_SERVO_BUF	32
#define PWM_DRIFT_COUNT	16

struct pwm_servo {
	uint64_t period[PWM_SERVO_BUF];
	uint64_t ts[PWM_SERVO_BUF];
	int state;
	int count;
};

static uint64_t pwm_servo_sample(struct pwm_servo *ps, uint64_t ts)
{
	double avg_period, ts_diff;
	int offset, i, drift_cnt;
	uint64_t period, base;

	offset = ts %  NS_PER_SEC;
	if (offset > 500000000)
		offset = offset - NS_PER_SEC;

	switch (ps->state) {
		/*
		 * Since update to PWM period gets reflected in next cycle,
		 * Servo state 0 and 1 are used to bring down the error
		 * below 1ms.
		 */
	case 0:
		period = ps->period[1] = PWM_PERIOD;
		ps->state = 1;
		break;
	case 1:
		if (abs(offset) < 1000000) {
			period = ps->period[ps->count + 2] = PWM_PERIOD;
			ps->ts[ps->count] = ts;
			ps->state = 2;
			ps->count++;
		} else {
			period = PWM_PERIOD - offset;
			ps->state = 0;
		}
		break;
	case 2:
		/*
		 * Offset is calculated in 2 different methods:
		 * - samples collected are < PWM_DRIFT_COUNT: Calculate offset
		 *   		based on the last 2 timestamps
		 * - samples collected are > PWM_DRIFT_COUNT: Calculate offset
		 *		based on last PWM_DRIFT_COUNT timestamps.
		 */
		ps->ts[ps->count % PWM_SERVO_BUF] = ts;
		drift_cnt = (ps->count > PWM_DRIFT_COUNT) ? PWM_DRIFT_COUNT : 1;
		avg_period = 0;
		for (i = 0; i < drift_cnt; i++)
			avg_period += ps->period[(ps->count - i) % PWM_SERVO_BUF];
		ts_diff = ps->ts[ps->count % PWM_SERVO_BUF] -
			ps->ts[(ps->count - drift_cnt) % PWM_SERVO_BUF];

		/*
		 * Calculate the base period based on programmed period and
		 * observed timestamps.
		 */
		base = avg_period / (ts_diff*1e-9) + 0.5;

		/*
		 * Account for the drift in current period which is programmed
		 * in last cycle.
		 * */
		offset += ps->period[(ps->count + 1) % PWM_SERVO_BUF] - base;

		period = base - offset;
		ps->period[(ps->count + 2) % PWM_SERVO_BUF] = period;
		ps->count++;

		/* If error is out of bounds, reset the servo */
		if (abs(offset) > 1000000) {
			period = PWM_PERIOD;
			ps->count = 0;
			ps->state = 0;
		}
		break;
	}

	return period;
}

int main(int argc, char *argv[])
{
	unsigned int pwm_chip = 0, pwm_chan = 0, event_index = 0;
	int c, err, level = LOG_INFO, verbose = 0;
	char *progname, *ptp_dev;
	struct pwm_chan *chan;
	struct pwm_servo ps;
	clockid_t clkid;
	uint64_t ts;

	handle_term_signals();

	/* Process the command line arguments. */
	progname = strrchr(argv[0], '/');
	progname = progname ? 1+progname : argv[0];

	while (EOF != (c = getopt(argc, argv, "c:e:hl:mp:vw:"))) {
		switch (c) {
		case 'c':
			pwm_chan = atoi(optarg);
			break;
		case 'e':
			event_index = atoi(optarg);
			break;
		case 'h':
			usage(progname);
			return 0;
		case 'l':
			level = atoi(optarg);
			break;
		case 'm':
			verbose = 1;
			break;
		case 'p':
			ptp_dev = optarg;
			break;
		case 'v':
			version_show(stdout);
			return 0;
		case 'w':
			pwm_chip = atoi(optarg);
			break;
		case '?':
		default:
			usage(progname);
			return -1;
		}
	}

	if (!ptp_dev) {
		usage(progname);
		return -1;
	}

	handle_term_signals();
	print_set_progname(progname);
	print_set_level(level);
	print_set_verbose(verbose);

	clkid = phc_open(ptp_dev);
	if (clkid == CLOCK_INVALID)
		return -1;

	chan = pwm_chan_create(pwm_chip, pwm_chan);
	if (!chan) {
		err = -1;
		goto phc_clean;
	}

	err = phc_enable_extts(clkid, event_index);
	if (err)
		goto pwm_chan_clean;

	pwm_chan_set_period(chan, PWM_PERIOD);
	pwm_chan_set_duty_cycle(chan, PWM_DUTY_CYCLE);
	ps.period[0] = PWM_PERIOD;
	ps.count = ps.state = 0;

	err = pwm_chan_enable(chan);
	if (err)
		goto extts_clean;

	while (is_running()) {
		if (phc_read_extts(clkid, &ts))
			continue;

		pr_info("Timestamp = %lld.%09lld\n", ts / NS_PER_SEC, ts % NS_PER_SEC);
		pwm_chan_set_period(chan, pwm_servo_sample(&ps, ts));
	}

extts_clean:
	phc_disable_extts(clkid, event_index);
pwm_chan_clean:
	pwm_chan_destroy(chan);
phc_clean:
	phc_close(clkid);

	return err;
}
