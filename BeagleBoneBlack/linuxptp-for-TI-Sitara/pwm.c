/**
 * @file pwm.c
 * @note Copyright (C) 2020 Texas Instruments Incorporated - http://www.ti.com/
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
#include <ctype.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pwm.h"
#include "print.h"

#define PWM_CHAN_EXPORT		"/sys/class/pwm/pwmchip%d/export"
#define PWM_CHAN_UNEXPORT	"/sys/class/pwm/pwmchip%d/unexport"
#define PWM_CHAN_ENABLE		"/sys/class/pwm/pwmchip%d/pwm%d/enable"
#define PWM_CHAN_PERIOD		"/sys/class/pwm/pwmchip%d/pwm%d/period"
#define PWM_CHAN_DUTY_CYCLE	"/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle"

struct pwm_chan {
	int pwm_id;
	int chan_id;
	int period_fd;
	int dc_fd;
};

static int open_device(int perm, char const *dev, ...)
{
	char dev_buf[128];
	va_list ap;
	int fd;

	va_start(ap, dev);
	vsnprintf(dev_buf, sizeof(dev_buf), dev, ap);
	va_end(ap);

	fd = open(dev_buf, perm);
	if (fd < 0) {
		pr_err("opening %s: Failed\n", dev_buf);
		return fd;
	}

	pr_debug("Opening %s: SUCCESS\n", dev_buf);

	return fd;
}

static int pwm_chan_export(struct pwm_chan *chan)
{
	int pwm_fd, ret;
	char buf[8];

	pwm_fd = open_device(O_WRONLY, PWM_CHAN_EXPORT, chan->pwm_id);
	if (pwm_fd < 0)
		return pwm_fd;

	sprintf(buf, "%u",chan->chan_id);
	ret =  write(pwm_fd, buf, sizeof(buf));
	if (ret < 0)
		pr_err("Exporting pwm %d channel %s: FAILED\n",
			chan->pwm_id, buf);
	else
		pr_debug("Exporting pwm %d channel %d: SUCCESS\n",
			 chan->pwm_id, chan->chan_id);

	close(pwm_fd);
	return (ret < 0) ? ret : 0;
}

static int pwm_chan_unexport(struct pwm_chan *chan)
{
	int pwm_fd, ret;
	char buf[8];

	pwm_fd = open_device(O_WRONLY, PWM_CHAN_UNEXPORT, chan->pwm_id);
	if (pwm_fd < 0)
		return pwm_fd;

	sprintf(buf, "%u", chan->chan_id);
	ret =  write(pwm_fd, buf, sizeof(buf));
	if (ret < 0)
		pr_err("Un-Exporting pwm %d channel %s: FAILED\n",
			chan->pwm_id, buf);

	close(pwm_fd);
	return (ret < 0) ? ret : 0;
}

int pwm_chan_enable(struct pwm_chan *chan)
{
	int pwm_fd, ret;

	pwm_fd = open_device(O_WRONLY, PWM_CHAN_ENABLE, chan->pwm_id,
			     chan->chan_id);
	if (pwm_fd < 0)
		return pwm_fd;

	ret =  write(pwm_fd, "1", 1);
	if (ret < 0)
		pr_err("Enabling pwm %d channel %d: FAILED\n",
			chan->pwm_id, chan->chan_id);
	else
		pr_debug("Enabling pwm %d channel %d: SUCCESS\n",
			 chan->pwm_id, chan->chan_id);

	close(pwm_fd);
	return (ret < 0) ? ret : 0;
}

void pwm_chan_disable(struct pwm_chan *chan)
{
	int pwm_fd;

	pwm_fd = open_device(O_WRONLY, PWM_CHAN_ENABLE, chan->pwm_id,
			     chan->chan_id);
	if (pwm_fd < 0)
		return;

	if (write(pwm_fd, "0", 1) < 0)
		pr_err("Disabling pwm %d channel %d: FAILED\n",
			chan->pwm_id, chan->chan_id);

	close(pwm_fd);
}

int pwm_chan_set_period(struct pwm_chan *chan, unsigned long long period)
{
	char dev_buf[128];
	int ret;

	snprintf(dev_buf, sizeof(dev_buf), "%llu", period);
	ret =  write(chan->period_fd, dev_buf, sizeof(dev_buf));
	if (ret < 0)
		pr_err("Setting period %llu on pwm %d chan %d: FAILED\n",
			period, chan->pwm_id, chan->chan_id);
	else
		pr_debug("Setting period %llu on pwm %d chan %d: SUCCESS\n",
			 period, chan->pwm_id, chan->chan_id);

	return (ret < 0) ? ret : 0;
}

int pwm_chan_set_duty_cycle(struct pwm_chan *chan, unsigned long long dc)
{
	char dev_buf[128];
	int ret;

	snprintf(dev_buf, sizeof(dev_buf), "%llu", dc);
	ret =  write(chan->dc_fd, dev_buf, sizeof(dev_buf));
	if (ret < 0)
		pr_err("Setting duty cycle %llu on chan %d pwm %d: FAILED\n",
			dc, chan->pwm_id, chan->chan_id);
	else
		pr_debug("Setting duty cycle %llu on pwm %d chan %d: SUCCESS\n",
			 dc, chan->pwm_id, chan->chan_id);

	return (ret < 0) ? ret : 0;
}

void pwm_chan_destroy(struct pwm_chan *chan)
{
	close(chan->dc_fd);
	close(chan->period_fd);
	pwm_chan_disable(chan);
	pwm_chan_unexport(chan);
	free(chan);
}

struct pwm_chan *pwm_chan_create(int pwm_id, int chan_id)
{
	struct pwm_chan *chan;

	chan = calloc(1, sizeof(*chan));
	if (!chan) {
		pr_err("Memory allocation for pwm chan: Failed\n");
		return NULL;
	}

	chan->pwm_id = pwm_id;
	chan->chan_id = chan_id;

	if (pwm_chan_export(chan))
		goto clean_chan;

	chan->period_fd = open_device(O_RDWR, PWM_CHAN_PERIOD, pwm_id, chan_id);
	if (chan->period_fd < 0)
		goto unexport_pwm;

	chan->dc_fd = open_device(O_RDWR, PWM_CHAN_DUTY_CYCLE, pwm_id, chan_id);
	if (chan->dc_fd < 0)
		goto close_period;

	return chan;

close_period:
	close(chan->period_fd);
unexport_pwm:
	pwm_chan_unexport(chan);
clean_chan:
	free(chan);
	return NULL;
}
