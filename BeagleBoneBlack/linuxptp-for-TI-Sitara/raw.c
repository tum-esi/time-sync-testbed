/**
 * @file raw.c
 * @note Copyright (C) 2012 Richard Cochran <richardcochran@gmail.com>
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
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#include "address.h"
#include "config.h"
#include "contain.h"
#include "ether.h"
#include "print.h"
#include "raw.h"
#include "redinfo.h"
#include "sk.h"
#include "transport_private.h"
#include "util.h"

struct raw {
	struct transport t;
	struct address src_addr;
	struct address ptp_addr;
	struct address p2p_addr;
	int vlan;
	int efd_index;
	int gfd_index;
};

#define OP_AND  (BPF_ALU | BPF_AND | BPF_K)
#define OP_JEQ  (BPF_JMP | BPF_JEQ | BPF_K)
#define OP_JUN  (BPF_JMP | BPF_JA)
#define OP_LDB  (BPF_LD  | BPF_B   | BPF_ABS)
#define OP_LDH  (BPF_LD  | BPF_H   | BPF_ABS)
#define OP_RETK (BPF_RET | BPF_K)

#define PTP_GEN_BIT 0x08 /* indicates general message, if set in message type */

#define N_RAW_FILTER    12
#define RAW_FILTER_TEST 9

static struct sock_filter raw_filter[N_RAW_FILTER] = {
	{OP_LDH,  0, 0, OFF_ETYPE   },
	{OP_JEQ,  0, 4, ETH_P_8021Q          }, /*f goto non-vlan block*/
	{OP_LDH,  0, 0, OFF_ETYPE + 4        },
	{OP_JEQ,  0, 7, ETH_P_1588           }, /*f goto reject*/
	{OP_LDB,  0, 0, ETH_HLEN + VLAN_HLEN },
	{OP_JUN,  0, 0, 2                    }, /*goto test general bit*/
	{OP_JEQ,  0, 4, ETH_P_1588  }, /*f goto reject*/
	{OP_LDB,  0, 0, ETH_HLEN    },
	{OP_AND,  0, 0, PTP_GEN_BIT }, /*test general bit*/
	{OP_JEQ,  0, 1, 0           }, /*0,1=accept event; 1,0=accept general*/
	{OP_RETK, 0, 0, 1500        }, /*accept*/
	{OP_RETK, 0, 0, 0           }, /*reject*/
};

static int raw_configure(int fd, int event, int index,
			 unsigned char *addr1, unsigned char *addr2, int enable)
{
	int err1, err2, filter_test, option;
	struct packet_mreq mreq;
	struct sock_fprog prg = { N_RAW_FILTER, raw_filter };

	filter_test = RAW_FILTER_TEST;
	if (event) {
		raw_filter[filter_test].jt = 0;
		raw_filter[filter_test].jf = 1;
	} else {
		raw_filter[filter_test].jt = 1;
		raw_filter[filter_test].jf = 0;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &prg, sizeof(prg))) {
		pr_err("setsockopt SO_ATTACH_FILTER failed: %m");
		return -1;
	}

	option = enable ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP;

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = index;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = MAC_LEN;
	memcpy(mreq.mr_address, addr1, MAC_LEN);

	err1 = setsockopt(fd, SOL_PACKET, option, &mreq, sizeof(mreq));
	if (err1)
		pr_warning("setsockopt PACKET_MR_MULTICAST failed: %m");

	memcpy(mreq.mr_address, addr2, MAC_LEN);

	err2 = setsockopt(fd, SOL_PACKET, option, &mreq, sizeof(mreq));
	if (err2)
		pr_warning("setsockopt PACKET_MR_MULTICAST failed: %m");

	if (!err1 && !err2)
		return 0;

	mreq.mr_ifindex = index;
	mreq.mr_type = PACKET_MR_ALLMULTI;
	mreq.mr_alen = 0;
	if (!setsockopt(fd, SOL_PACKET, option, &mreq, sizeof(mreq))) {
		return 0;
	}
	pr_warning("setsockopt PACKET_MR_ALLMULTI failed: %m");

	mreq.mr_ifindex = index;
	mreq.mr_type = PACKET_MR_PROMISC;
	mreq.mr_alen = 0;
	if (!setsockopt(fd, SOL_PACKET, option, &mreq, sizeof(mreq))) {
		return 0;
	}
	pr_warning("setsockopt PACKET_MR_PROMISC failed: %m");

	pr_err("all socket options failed");
	return -1;
}

static int raw_close(struct transport *t, struct fdarray *fda)
{
	close(fda->fd[0]);
	close(fda->fd[1]);
	return 0;
}

static int open_socket(const char *name, int event, unsigned char *ptp_dst_mac,
		       unsigned char *p2p_dst_mac, int socket_priority)
{
	struct sockaddr_ll addr;
	int fd, index;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0) {
		pr_err("socket failed: %m");
		goto no_socket;
	}
	index = sk_interface_index(fd, name);
	if (index < 0)
		goto no_option;

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = index;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
		pr_err("bind failed: %m");
		goto no_option;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, name, strlen(name))) {
		pr_err("setsockopt SO_BINDTODEVICE failed: %m");
		goto no_option;
	}

	if (socket_priority > 0 &&
	    setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &socket_priority,
		       sizeof(socket_priority))) {
		pr_err("setsockopt SO_PRIORITY failed: %m");
		goto no_option;
	}
	if (raw_configure(fd, event, index, ptp_dst_mac, p2p_dst_mac, 1))
		goto no_option;

	return fd;
no_option:
	close(fd);
no_socket:
	return -1;
}

static void mac_to_addr(struct address *addr, void *mac)
{
	addr->sll.sll_family = AF_PACKET;
	addr->sll.sll_halen = MAC_LEN;
	memcpy(addr->sll.sll_addr, mac, MAC_LEN);
	addr->len = sizeof(addr->sll);
}

static void addr_to_mac(void *mac, struct address *addr)
{
	memcpy(mac, &addr->sll.sll_addr, MAC_LEN);
}

static int raw_open(struct transport *t, struct interface *iface,
		    struct fdarray *fda, enum timestamp_type ts_type)
{
	struct raw *raw = container_of(t, struct raw, t);
	unsigned char ptp_dst_mac[MAC_LEN];
	unsigned char p2p_dst_mac[MAC_LEN];
	int efd, gfd, socket_priority;
	const char *name;
	char *str;

	name = interface_label(iface);
	str = config_get_string(t->cfg, name, "ptp_dst_mac");
	if (str2mac(str, ptp_dst_mac)) {
		pr_err("invalid ptp_dst_mac %s", str);
		return -1;
	}
	str = config_get_string(t->cfg, name, "p2p_dst_mac");
	if (str2mac(str, p2p_dst_mac)) {
		pr_err("invalid p2p_dst_mac %s", str);
		return -1;
	}
	mac_to_addr(&raw->ptp_addr, ptp_dst_mac);
	mac_to_addr(&raw->p2p_addr, p2p_dst_mac);

	if (sk_interface_macaddr(name, &raw->src_addr))
		goto no_mac;

	socket_priority = config_get_int(t->cfg, "global", "socket_priority");

	efd = open_socket(name, 1, ptp_dst_mac, p2p_dst_mac, socket_priority);
	if (efd < 0)
		goto no_event;

	raw->efd_index = sk_interface_index(efd, name);
	if (raw->efd_index < 0)
		goto no_general;

	gfd = open_socket(name, 0, ptp_dst_mac, p2p_dst_mac, socket_priority);
	if (gfd < 0)
		goto no_general;

	raw->gfd_index = sk_interface_index(gfd, name);
	if (raw->gfd_index < 0)
		goto no_timestamping;

	if (sk_timestamping_init(efd, name, ts_type, TRANS_IEEE_802_3))
		goto no_timestamping;

	if (sk_general_init(gfd))
		goto no_timestamping;

	fda->fd[FD_EVENT] = efd;
	fda->fd[FD_GENERAL] = gfd;
	return 0;

no_timestamping:
	close(gfd);
no_general:
	close(efd);
no_event:
no_mac:
	return -1;
}

static int raw_recv(struct transport *t, int fd, void *buf, int buflen,
		    struct address *addr, struct hw_timestamp *hwts)
{
	int cnt, hlen;
	unsigned char *ptr = buf;
	struct eth_hdr *hdr;
	struct raw *raw = container_of(t, struct raw, t);

	if (raw->vlan) {
		hlen = sizeof(struct vlan_hdr);
	} else {
		hlen = sizeof(struct eth_hdr);
	}
	ptr    -= hlen;
	buflen += hlen;
	hdr = (struct eth_hdr *) ptr;

	cnt = sk_receive(fd, ptr, buflen, addr, hwts, MSG_DONTWAIT);

	if (cnt >= 0)
		cnt -= hlen;
	if (cnt < 0)
		return cnt;

	if (raw->vlan) {
		if (ETH_P_1588 == ntohs(hdr->type)) {
			pr_notice("raw: disabling VLAN mode");
			raw->vlan = 0;
		}
	} else {
		if (ETH_P_8021Q == ntohs(hdr->type)) {
			pr_notice("raw: switching to VLAN mode");
			raw->vlan = 1;
		}
	}
	return cnt;
}

static int raw_send(struct transport *t, struct fdarray *fda,
		    enum transport_event event, int peer, void *buf, int len,
		    struct address *addr, struct hw_timestamp *hwts)
{
	struct raw *raw = container_of(t, struct raw, t);
	ssize_t cnt;
	unsigned char pkt[1600], *ptr = buf;
	struct eth_hdr *hdr;
	int fd = -1;

	switch (event) {
	case TRANS_GENERAL:
		fd = fda->fd[FD_GENERAL];
		break;
	case TRANS_EVENT:
	case TRANS_ONESTEP:
	case TRANS_P2P1STEP:
	case TRANS_DEFER_EVENT:
		fd = fda->fd[FD_EVENT];
		break;
	}

	ptr -= sizeof(*hdr);
	len += sizeof(*hdr);

	if (!addr)
		addr = peer ? &raw->p2p_addr : &raw->ptp_addr;

	hdr = (struct eth_hdr *) ptr;
	addr_to_mac(&hdr->dst, addr);
	addr_to_mac(&hdr->src, &raw->src_addr);

	hdr->type = htons(ETH_P_1588);

	cnt = send(fd, ptr, len, 0);
	if (cnt < 1) {
		return -errno;
	}
	/*
	 * Get the time stamp right away.
	 */
	return event == TRANS_EVENT ? sk_receive(fd, pkt, len, NULL, hwts, MSG_ERRQUEUE) : cnt;
}

static void raw_release(struct transport *t)
{
	struct raw *raw = container_of(t, struct raw, t);
	free(raw);
}

static int raw_physical_addr(struct transport *t, uint8_t *addr)
{
	struct raw *raw = container_of(t, struct raw, t);
	addr_to_mac(addr, &raw->src_addr);
	return MAC_LEN;
}

static int raw_protocol_addr(struct transport *t, uint8_t *addr)
{
	struct raw *raw = container_of(t, struct raw, t);
	addr_to_mac(addr, &raw->src_addr);
	return MAC_LEN;
}

/*     addr: Dst addr. For hdr dst and msg_name.
 *           Use p2p_addr or ptp_addr if null.
 * src_addr: Use in hdr src
 */
static int raw_red_src_addr_sendmsg(struct transport *t, struct fdarray *fda,
				    int event, int peer, void *buf, int len,
				    struct address *addr,
				    struct address *src_addr,
				    struct hw_timestamp *hwts,
				    struct redundant_info *redinfo)
{
	struct raw *raw = container_of(t, struct raw, t);
	ssize_t cnt;
	int fd = event ? fda->fd[FD_EVENT] : fda->fd[FD_GENERAL];
	unsigned char pkt[1600], *ptr = buf;
	struct eth_hdr *hdr;

	struct msghdr msg;
	struct iovec iov;
	struct address msg_addr;
	struct cmsghdr *cmsgp = NULL;
	char cbuf[CMSG_SPACE(sizeof(struct redundant_info))];
	struct redundant_info *rp;

	ptr -= sizeof(*hdr);
	len += sizeof(*hdr);

	if (!addr)
		addr = peer ? &raw->p2p_addr : &raw->ptp_addr;

	memset(&msg_addr, 0, sizeof(msg_addr));
	memcpy(&msg_addr, addr, sizeof(msg_addr));
	msg_addr.sll.sll_ifindex = event ? raw->efd_index : raw->gfd_index;

	hdr = (struct eth_hdr *) ptr;
	addr_to_mac(&hdr->dst, addr);
	addr_to_mac(&hdr->src, src_addr);

	hdr->type = htons(ETH_P_1588);

	iov.iov_base = ptr;
	iov.iov_len = len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &msg_addr.sll;
	msg.msg_namelen = sizeof(msg_addr.sll);

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	memset(cbuf, 0, sizeof(cbuf));
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	cmsgp = CMSG_FIRSTHDR(&msg);
	cmsgp->cmsg_len = CMSG_LEN(sizeof(struct redundant_info));
	cmsgp->cmsg_level = SOL_SOCKET;
	cmsgp->cmsg_type = SCM_REDUNDANT;

	rp = (struct redundant_info *)CMSG_DATA(cmsgp);
	memcpy(rp, redinfo, sizeof(*rp));

	cnt = sendmsg(fd, &msg, 0);
	if (cnt < 0) {
		pr_err("red_src_addr_sendmsg: sendmsg: %m");
		return -1;
	}
	/*
	 * Get the time stamp right away.
	 */
	return event == TRANS_EVENT ?
			sk_red_receive(fd, pkt, len, NULL, hwts, redinfo, NULL,
				       MSG_ERRQUEUE) :
			cnt;
}

static int raw_red_recv(struct transport *t, int fd, void *buf, int buflen,
			struct address *addr, struct hw_timestamp *hwts,
			struct redundant_info *redinfo,
			struct hw_timestamp *red_hwts)
{
	int cnt, hlen;
	unsigned char *ptr = buf;
	struct eth_hdr *hdr;
	struct raw *raw = container_of(t, struct raw, t);

	if (raw->vlan) {
		hlen = sizeof(struct vlan_hdr);
	} else {
		hlen = sizeof(struct eth_hdr);
	}
	ptr    -= hlen;
	buflen += hlen;
	hdr = (struct eth_hdr *) ptr;

	/* There may be a cut-through timestamp, hence the red_hwts. */
	cnt = sk_red_receive(fd, ptr, buflen, addr, hwts, redinfo, red_hwts, 0);

	if (cnt >= 0)
		cnt -= hlen;
	if (cnt < 0)
		return cnt;

	if (raw->vlan) {
		if (ETH_P_1588 == ntohs(hdr->type)) {
			pr_notice("raw: disabling VLAN mode");
			raw->vlan = 0;
		}
	} else {
		if (ETH_P_8021Q == ntohs(hdr->type)) {
			pr_notice("raw: switching to VLAN mode");
			raw->vlan = 1;
		}
	}
	return cnt;
}

static int raw_red_sendmsg(struct transport *t, struct fdarray *fda, int event,
			   int peer, void *buf, int len, struct address *addr,
			   struct hw_timestamp *hwts,
			   struct redundant_info *redinfo)
{
	struct raw *raw = container_of(t, struct raw, t);

	return raw_red_src_addr_sendmsg(t, fda, event, peer, buf, len,
					addr, &raw->src_addr, hwts, redinfo);
}

static int raw_red_cut_thru_msg(struct transport *t, struct fdarray *fda,
				int event, int peer, void *buf, int len,
				struct address *addr,
				struct address *orig_src_addr,
				struct hw_timestamp *hwts,
				struct redundant_info *redinfo)
{
	return raw_red_src_addr_sendmsg(t, fda, event, peer, buf, len,
					addr, orig_src_addr, hwts, redinfo);
}

struct transport *raw_transport_create(void)
{
	struct raw *raw;
	raw = calloc(1, sizeof(*raw));
	if (!raw)
		return NULL;
	raw->t.close   = raw_close;
	raw->t.open    = raw_open;
	raw->t.recv    = raw_recv;
	raw->t.send    = raw_send;
	raw->t.release = raw_release;
	raw->t.physical_addr = raw_physical_addr;
	raw->t.protocol_addr = raw_protocol_addr;
	raw->t.red_recv = raw_red_recv;
	raw->t.red_sendmsg = raw_red_sendmsg;
	raw->t.red_cut_thru_msg = raw_red_cut_thru_msg;
	return &raw->t;
}
