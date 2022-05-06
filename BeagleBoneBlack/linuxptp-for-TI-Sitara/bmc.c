/**
 * @file bmc.c
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
#include <string.h>

#include "bmc.h"
#include "ds.h"

#define A_BETTER_TOPO  2
#define A_BETTER       1
#define B_BETTER      -1
#define B_BETTER_TOPO -2

int dscmp2(struct dataset *a, struct dataset *b, int a_qual, int b_qual)
{
	int diff;
	unsigned int A = a->stepsRemoved, B = b->stepsRemoved;

	if (A + 1 < B)
		return A_BETTER;
	if (B + 1 < A)
		return B_BETTER;
	/*
	 * We ignore the "error-1" conditions mentioned in the
	 * standard, since there is nothing we can do about it anyway.
	 */
	if (A < B) {
		diff = memcmp(&b->receiver, &b->sender, sizeof(b->receiver));
		if (diff < 0)
			return A_BETTER;
		if (diff > 0)
			return A_BETTER_TOPO;
		/* error-1 */
		return 0;
	}
	if (A > B) {
		diff = memcmp(&a->receiver, &a->sender, sizeof(a->receiver));
		if (diff < 0)
			return B_BETTER;
		if (diff > 0)
			return B_BETTER_TOPO;
		/* error-1 */
		return 0;
	}

	/* For redundancy, compare quality of A and B if available
	 * returns A_BETTER_TOPO or B_BETTER_TOPO.
	 * See IEC-62439-3-2016 A.7.3. p114.
	 */
	if (a_qual < b_qual)
		return A_BETTER_TOPO;
	if (a_qual > b_qual)
		return B_BETTER_TOPO;

	diff = memcmp(&a->sender, &b->sender, sizeof(a->sender));
	if (diff < 0)
		return A_BETTER_TOPO;
	if (diff > 0)
		return B_BETTER_TOPO;

	if (a->receiver.portNumber < b->receiver.portNumber)
		return A_BETTER_TOPO;
	if (a->receiver.portNumber > b->receiver.portNumber)
		return B_BETTER_TOPO;
	/*
	 * If we got this far, it means "error-2" has occured.
	 */
	return 0;
}

int dscmp(struct dataset *a, struct dataset *b, int a_qual, int b_qual)
{
	int diff;

	if (a == b)
		return 0;
	if (a && !b)
		return A_BETTER;
	if (b && !a)
		return B_BETTER;

	diff = memcmp(&a->identity, &b->identity, sizeof(a->identity));

	if (!diff)
		return dscmp2(a, b, a_qual, b_qual);

	if (a->priority1 < b->priority1)
		return A_BETTER;
	if (a->priority1 > b->priority1)
		return B_BETTER;

	if (a->quality.clockClass < b->quality.clockClass)
		return A_BETTER;
	if (a->quality.clockClass > b->quality.clockClass)
		return B_BETTER;

	if (a->quality.clockAccuracy < b->quality.clockAccuracy)
		return A_BETTER;
	if (a->quality.clockAccuracy > b->quality.clockAccuracy)
		return B_BETTER;

	if (a->quality.offsetScaledLogVariance <
	    b->quality.offsetScaledLogVariance)
		return A_BETTER;
	if (a->quality.offsetScaledLogVariance >
	    b->quality.offsetScaledLogVariance)
		return B_BETTER;

	if (a->priority2 < b->priority2)
		return A_BETTER;
	if (a->priority2 > b->priority2)
		return B_BETTER;

	return diff < 0 ? A_BETTER : B_BETTER;
}

enum port_state bmc_state_decision(struct clock *c, struct port *r,
				   int (*compare)(struct dataset *a, struct dataset *b, int a_qual, int b_qual))
{
	struct dataset *clock_ds, *clock_best, *port_best;
	enum port_state ps;
	int res;
	int best_fc_port_qual = 0, port_qual = 0;

	clock_ds = clock_default_ds(c);
	clock_best = clock_best_foreign(c);
	best_fc_port_qual = red_port_quality(clock_best_port(c));
	port_best = port_best_foreign(r);
	port_qual = red_port_quality(r);
	ps = port_state(r);

	/*
	 * This scenario is particularly important in the designated_slave_fsm
	 * when it is in PS_SLAVE state. In this scenario, there is no other
	 * foreign master and it will elect itself as master ultimately
	 * resulting in printing out some unnecessary warnings (see
	 * port_slave_priority_warning()).
	 */
	if (!port_best && port_bmca(r) == BMCA_NOOP) {
		return ps;
	}

	if (ps == PS_FAULTY)
		return ps;

	if (!port_best && PS_LISTENING == ps)
		return ps;

	if (clock_class(c) <= 127) {
		if (compare(clock_ds, port_best, 0, 0) > 0) {
			return PS_GRAND_MASTER; /*M1*/
		} else {
			return PS_PASSIVE; /*P1*/
		}
	}

	if (compare(clock_ds, clock_best, 0, 0) > 0) {
		return PS_GRAND_MASTER; /*M2*/
	}

	if (clock_best_port(c) == r) {
		return PS_SLAVE; /*S1*/
	}

	if (red_slave_port(r)) {
		/* clock best is not on this port, i.e., the other
		 * port will get PS_SLAVE. So this port should be
		 * PSLAVE
		 */
		return PS_PASSIVE_SLAVE;
	}

	res = dscmp(clock_best, port_best, best_fc_port_qual, port_qual);
	if (res == A_BETTER_TOPO) {
		return PS_PASSIVE; /*P2*/
	} else {
		return PS_MASTER; /*M3*/
	}
}
