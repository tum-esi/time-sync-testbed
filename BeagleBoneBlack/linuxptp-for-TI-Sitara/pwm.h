/**
 * @file pwm.h
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
#ifndef HAVE_PWM_H
#define HAVE_PWM_H

struct pwm_chan;

/**
 * Enable a PWM channel
 *
 * @param chan:		The pwm channel to enable
 *
 * @return		0 if PWM channel is enabled
 *			else appropriate error value.
 */
int pwm_chan_enable(struct pwm_chan *chan);

/**
 * Disable a PWM channel
 *
 * @param chan:		The pwm channel to disable
 */
void pwm_chan_disable(struct pwm_chan *chan);

/**
 * Set the period for a PWM channel
 *
 * @param chan:		The pwm channel for period update
 * @param period:	New period of the pwm channel in ns
 *
 * @return		0 if PWM channel's period is updated
 *			else appropriate error value.
 */
int pwm_chan_set_period(struct pwm_chan *chan, unsigned long long period);

/**
 * Set the duty cycle for a PWM channel
 *
 * @param chan:		The pwm channel for duty cycle update
 * @param period:	New duty cycle of the pwm channel in ns
 *
 * @return		0 if PWM channel's duty cycle is updated
 *			else appropriate error value.
 */
int pwm_chan_set_duty_cycle(struct pwm_chan *chan, unsigned long long dc);

/**
 * Destroy a PWM channel
 *
 * @param chan:		The pwm channel to destroy
 */
void pwm_chan_destroy(struct pwm_chan *chan);

/**
 * Create a PWM channel
 *
 * @param pwm_id:	PWM chip id of the pwm channel
 * @param chan_id:	PWM channel id
 *
 * @return		Pointer to the pwm channel instance
 */
struct pwm_chan *pwm_chan_create(int pwm_id, int chan_id);

#endif
