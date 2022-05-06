/**
 * @file redinfo.h
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

#ifndef REDINFO_H
#define REDINFO_H

#define BIT(nr)              (1 << (nr))

#ifndef SO_REDUNDANT
#define SO_REDUNDANT         80
#define SCM_REDUNDANT        SO_REDUNDANT
#endif

#ifndef SO_RED_TIMESTAMPING
#define SO_RED_TIMESTAMPING  81
#define SCM_RED_TIMESTAMPING SO_RED_TIMESTAMPING
#endif

#define PTP_MSG_IN           (0x3 << 6)
#define PTP_EVT_OUT          (0x2 << 6)
#define DIRECTED_TX          (0x1 << 6)
#define RED_PORT_B           BIT(1)
#define RED_PORT_A           BIT(0)

#define MSG_REDINFO(m)       (&(m)->redinfo)
#define MSG_HWTS(m)          (&(m)->hwts.ts)
#define MSG_RED_HWTS(m)      (&(m)->red_hwts.ts)
#define MSG_REDINFO_B(m)     (&(m)->redinfo_b)
#define MSG_HWTS_B(m)        (&(m)->hwts_b.ts)
#define MSG_RED_HWTS_B(m)    (&(m)->red_hwts_b.ts)

#define REDINFO_T(r)         ((r)->io_port & (0x3 << 6))
#define REDINFO_PORTS(r)     ((r)->io_port & 0x3)
#define REDINFO_PATHID(r)    ((r)->pathid)
#define REDINFO_SEQNR(r)     ((r)->seqnr)

#define MSG_RED_PORTS(m)  (REDINFO_PORTS(MSG_REDINFO(m)))
#define MSG_RED_T(m)      (REDINFO_T(MSG_REDINFO(m)))
#define IS_PTP_MSG_IN(r)  (REDINFO_T(r) == PTP_MSG_IN)
#define IS_PTP_EVT_OUT(r) (REDINFO_T(r) == PTP_EVT_OUT)
#define IS_PTP_DIR_OUT(r) (REDINFO_T(r) == DIRECTED_TX)

#endif /* REDINFO_H  */
