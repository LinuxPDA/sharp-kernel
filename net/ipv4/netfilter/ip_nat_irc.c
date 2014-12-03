/* IRC extension for TCP NAT alteration.
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 * based on a copy of RR's ip_nat_ftp.c
 *
 * ip_nat_irc.c,v 1.9 2000/11/07 18:26:42 laforge Exp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Module load syntax:
 * 	insmod ip_nat_irc.o ports=port1,port2,...port<MAX_PORTS>
 *	
 * 	please give the ports of all IRC servers You wish to connect to.
 *	If You don't specify ports, the default will be port 6667
 */

#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/kernel.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_nat_irc.h>
#include <linux/netfilter_ipv4/ip_conntrack_irc.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int ports_c = 0;

MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_DESCRIPTION("IRC (DCC) network address translation module");
#ifdef MODULE_PARM
MODULE_PARM(ports, "1-" __MODULE_STRING(MAX_PORTS) "i");
MODULE_PARM_DESC(ports, "port numbers of IRC servers");
#endif

/* FIXME: Time out? --RR */

static int
irc_nat_expected(struct sk_buff **pskb,
		 unsigned int hooknum,
		 struct ip_conntrack *ct,
		 struct ip_nat_info *info,
		 struct ip_conntrack *master,
		 struct ip_nat_info *masterinfo, unsigned int *verdict)
{
	struct ip_nat_multi_range mr;
	u_int32_t newdstip, newsrcip, newip;
	struct ip_ct_irc *ircinfo;

	IP_NF_ASSERT(info);
	IP_NF_ASSERT(master);
	IP_NF_ASSERT(masterinfo);

	IP_NF_ASSERT(!(info->initialized & (1 << HOOK2MANIP(hooknum))));

	DEBUGP("nat_expected: We have a connection!\n");

	/* Master must be an irc connection */
	ircinfo = &master->help.ct_irc_info;
	LOCK_BH(&ip_irc_lock);
	if (ircinfo->is_irc != IP_CONNTR_IRC) {
		UNLOCK_BH(&ip_irc_lock);
		DEBUGP("nat_expected: master not irc\n");
		return 0;
	}

	newdstip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
	newsrcip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
	DEBUGP("nat_expected: DCC cmd. %u.%u.%u.%u->%u.%u.%u.%u\n",
	       NIPQUAD(newsrcip), NIPQUAD(newdstip));

	UNLOCK_BH(&ip_irc_lock);

	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)
		newip = newsrcip;
	else
		newip = newdstip;

	DEBUGP("nat_expected: IP to %u.%u.%u.%u\n", NIPQUAD(newip));

	mr.rangesize = 1;
	/* We don't want to manip the per-protocol, just the IPs. */
	mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
	mr.range[0].min_ip = mr.range[0].max_ip = newip;

	*verdict = ip_nat_setup_info(ct, &mr, hooknum);

	return 1;
}

/* This is interesting.  We simply use the port given us by the client
   or server.  In practice it's extremely unlikely to clash; if it
   does, the rule won't be able to get a unique tuple and will drop
   the packets. */
static int
mangle_packet(struct sk_buff **pskb,
	      u_int32_t newip,
	      u_int16_t port,
	      unsigned int matchoff,
	      unsigned int matchlen, struct ip_nat_irc_info *this_way)
{

	/*      strlen("\1DCC CHAT chat AAAAAAAA P\1\n")=27
	 *      strlen("\1DCC SCHAT chat AAAAAAAA P\1\n")=28
	 *      strlen("\1DCC SEND F AAAAAAAA P S\1\n")=26
	 *      strlen("\1DCC MOVE F AAAAAAAA P S\1\n")=26
	 *      strlen("\1DCC TSEND F AAAAAAAA P S\1\n")=27
	 *              AAAAAAAAA: bound addr (1.0.0.0==16777216, min 8 digits,
	 *                      255.255.255.255==4294967296, 10 digits)
	 *              P:         bound port (min 1 d, max 5d (65635))
	 *              F:         filename   (min 1 d )
	 *              S:         size       (min 1 d )
	 *              0x01, \n:  terminators
	 */

	struct iphdr *iph = (*pskb)->nh.iph;
	struct tcphdr *tcph;
	unsigned char *data;
	unsigned int tcplen, newlen, newtcplen;
	/* "4294967296 65635 " */
	char buffer[18];

	MUST_BE_LOCKED(&ip_irc_lock);
	sprintf(buffer, "%u %u", ntohl(newip), port);

	tcplen = (*pskb)->len - iph->ihl * 4;
	newtcplen = tcplen - matchlen + strlen(buffer);
	newlen = iph->ihl * 4 + newtcplen;

	/* So there I am, in the middle of my `netfilter-is-wonderful'
	   talk in Sydney, and someone asks `What happens if you try
	   to enlarge a 64k packet here?'.  I think I said something
	   eloquent like `fuck'. - RR */
	if (newlen > 65535) {
		if (net_ratelimit())
			printk
			    ("nat_irc cheat: %u.%u.%u.%u->%u.%u.%u.%u %u\n",
			     NIPQUAD((*pskb)->nh.iph->saddr),
			     NIPQUAD((*pskb)->nh.iph->daddr),
			     (*pskb)->nh.iph->protocol);
		return NF_DROP;
	}

	if (newlen > (*pskb)->len + skb_tailroom(*pskb)) {
		struct sk_buff *newskb;
		newskb =
		    skb_copy_expand(*pskb, skb_headroom(*pskb), newlen,
				    GFP_ATOMIC);
		if (!newskb) {
			DEBUGP("irc: oom\n");
			return 0;
		} else {
			kfree_skb(*pskb);
			*pskb = newskb;
			iph = (*pskb)->nh.iph;
		}
	}

	tcph = (void *) iph + iph->ihl * 4;
	data = (void *) tcph + tcph->doff * 4;

	DEBUGP("Mapping `%.*s' [%u %u %u] to new `%s' [%u]\n",
	       (int) matchlen, data + matchoff,
	       data[matchoff], data[matchoff + 1],
	       matchlen, buffer, strlen(buffer));

	/* SYN adjust.  If it's uninitialized, or this is after last
	   correction, record it: we don't handle more than one
	   adjustment in the window, but do deal with common case of a
	   retransmit. */
	if (this_way->syn_offset_before == this_way->syn_offset_after
	    || before(this_way->syn_correction_pos, ntohl(tcph->seq))) {
		this_way->syn_correction_pos = ntohl(tcph->seq);
		this_way->syn_offset_before = this_way->syn_offset_after;
		this_way->syn_offset_after = (int32_t)
		    this_way->syn_offset_before + newlen - (*pskb)->len;
	}

	/* Move post-replacement */
	memmove(data + matchoff + strlen(buffer),
		data + matchoff + matchlen,
		(*pskb)->tail - (data + matchoff + matchlen));
	memcpy(data + matchoff, buffer, strlen(buffer));

	DEBUGP("ip_nat_irc: Inserting '%s' == %u.%u.%u.%u, port %u\n",
	       buffer, NIPQUAD(newip), port);

	/* Resize packet. */
	if (newlen > (*pskb)->len) {
		DEBUGP("ip_nat_irc: Extending packet by %u to %u bytes\n",
		       newlen - (*pskb)->len, newlen);
		skb_put(*pskb, newlen - (*pskb)->len);
	} else {
		DEBUGP
		    ("ip_nat_irc: Shrinking packet from %u to %u bytes\n",
		     (*pskb)->len, newlen);
		skb_trim(*pskb, newlen);
	}

	/* Fix checksums */
	iph->tot_len = htons(newlen);
	(*pskb)->csum = csum_partial((char *) tcph + tcph->doff * 4,
				     newtcplen - tcph->doff * 4, 0);
	tcph->check = 0;
	tcph->check = tcp_v4_check(tcph, newtcplen, iph->saddr, iph->daddr,
				   csum_partial((char *) tcph,
						tcph->doff * 4,
						(*pskb)->csum));
	ip_send_check(iph);
	return 1;
}

/* Grrr... SACK.  Fuck me even harder.  Don't want to fix it on the
   fly, so blow it away. */
static void delete_sack(struct sk_buff *skb, struct tcphdr *tcph)
{
	unsigned int i;
	u_int8_t *opt = (u_int8_t *) tcph;

	DEBUGP("Seeking SACKPERM in SYN packet (doff = %u).\n",
	       tcph->doff * 4);
	for (i = sizeof(struct tcphdr); i < tcph->doff * 4;) {
		DEBUGP("%u ", opt[i]);
		switch (opt[i]) {
		case TCPOPT_NOP:
		case TCPOPT_EOL:
			i++;
			break;

		case TCPOPT_SACK_PERM:
			goto found_opt;

		default:
			/* Worst that can happen: it will take us over. */
			i += opt[i + 1] ? : 1;
		}
	}
	DEBUGP("\n");
	return;

      found_opt:
	DEBUGP("\n");
	DEBUGP("Found SACKPERM at offset %u.\n", i);

	/* Must be within TCP header, and valid SACK perm. */
	if (i + opt[i + 1] <= tcph->doff * 4 && opt[i + 1] == 2) {
		/* Replace with NOPs. */
		tcph->check
		    =
		    ip_nat_cheat_check(*((u_int16_t *) (opt + i)) ^ 0xFFFF,
				       0, tcph->check);
		opt[i] = opt[i + 1] = 0;
	} else
		DEBUGP("Something wrong with SACK_PERM.\n");
}

static int irc_data_fixup(const struct ip_ct_irc *ct_irc_info,
			  struct ip_conntrack *ct,
			  struct ip_nat_irc_info *irc,
			  unsigned int datalen, struct sk_buff **pskb)
{
	u_int32_t newip;
	struct ip_conntrack_tuple t;
	struct iphdr *iph = (*pskb)->nh.iph;
	struct tcphdr *tcph = (void *) iph + iph->ihl * 4;
	int port;

	MUST_BE_LOCKED(&ip_irc_lock);
	DEBUGP("IRC_NAT: info (seq %u + %u) packet(seq %u + %u)\n",
	       ct_irc_info->seq, ct_irc_info->len,
	       ntohl(tcph->seq), datalen);

	newip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;

	/* Alter conntrack's expectations. */

	/* We can read expect here without conntrack lock, since it's
	   only set in ip_conntrack_irc, with ip_irc_lock held
	   writable */

	t = ct->expected.tuple;
	t.dst.ip = newip;
	for (port = ct_irc_info->port; port != 0; port++) {
		t.dst.u.tcp.port = htons(port);
		if (ip_conntrack_expect_related(ct, &t,
						&ct->expected.mask,
						NULL) == 0) {
			DEBUGP("using port %d", port);
			break;
		}

	}
	if (port == 0)
		return 0;
	if (!mangle_packet(pskb, newip, ct_irc_info->port,
			   ct_irc_info->seq - ntohl(tcph->seq),
			   ct_irc_info->len, irc))
		return 0;
	return 1;
}

static unsigned int help(struct ip_conntrack *ct,
			 struct ip_nat_info *info,
			 enum ip_conntrack_info ctinfo,
			 unsigned int hooknum, struct sk_buff **pskb)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct tcphdr *tcph = (void *) iph + iph->ihl * 4;
	u_int32_t newseq, newack;
	unsigned int datalen;
	int dir;
	int score;
	struct ip_ct_irc *ct_irc_info = &ct->help.ct_irc_info;
	struct ip_nat_irc_info *irc = &ct->nat.help.irc_info;

	/* Delete SACK_OK on initial TCP SYNs. */
	if (tcph->syn && !tcph->ack)
		delete_sack(*pskb, tcph);

	/* Only mangle things once: original direction in POST_ROUTING
	   and reply direction on PRE_ROUTING. */
	dir = CTINFO2DIR(ctinfo);
	if (!((hooknum == NF_IP_POST_ROUTING && dir == IP_CT_DIR_ORIGINAL)
	      || (hooknum == NF_IP_PRE_ROUTING && dir == IP_CT_DIR_REPLY))) {
		DEBUGP("nat_irc: Not touching dir %s at hook %s\n",
		       dir == IP_CT_DIR_ORIGINAL ? "ORIG" : "REPLY",
		       hooknum == NF_IP_POST_ROUTING ? "POSTROUTING"
		       : hooknum == NF_IP_PRE_ROUTING ? "PREROUTING"
		       : hooknum == NF_IP_LOCAL_OUT ? "OUTPUT" : "???");
		return NF_ACCEPT;
	}
	DEBUGP("got beyond not touching\n");

	datalen = (*pskb)->len - iph->ihl * 4 - tcph->doff * 4;
	score = 0;
	LOCK_BH(&ip_irc_lock);
	if (ct_irc_info->len) {
		DEBUGP("got beyond ct_irc_info->len\n");

		/* If it's in the right range... */
		score += between(ct_irc_info->seq, ntohl(tcph->seq),
				 ntohl(tcph->seq) + datalen);
		score += between(ct_irc_info->seq + ct_irc_info->len,
				 ntohl(tcph->seq),
				 ntohl(tcph->seq) + datalen);
		if (score == 1) {
			/* Half a match?  This means a partial retransmisison.
			   It's a cracker being funky. */
			if (net_ratelimit()) {
				printk
				    ("IRC_NAT: partial packet %u/%u in %u/%u\n",
				     ct_irc_info->seq, ct_irc_info->len,
				     ntohl(tcph->seq),
				     ntohl(tcph->seq) + datalen);
			}
			UNLOCK_BH(&ip_irc_lock);
			return NF_DROP;
		} else if (score == 2) {
			DEBUGP("IRC_NAT: score=2, calling fixup\n");
			if (!irc_data_fixup(ct_irc_info, ct, irc, datalen,
					    pskb)) {
				UNLOCK_BH(&ip_irc_lock);
				return NF_DROP;
			}
			/* skb may have been reallocated */
			iph = (*pskb)->nh.iph;
			tcph = (void *) iph + iph->ihl * 4;
		}
	}

	/* Sequence adjust */
	if (after(ntohl(tcph->seq), irc[dir].syn_correction_pos))
		newseq = ntohl(tcph->seq) + irc[dir].syn_offset_after;
	else
		newseq = ntohl(tcph->seq) + irc[dir].syn_offset_before;
	newseq = htonl(newseq);

	/* Ack adjust: other dir sees offset seq numbers */
	if (after(ntohl(tcph->ack_seq) - irc[!dir].syn_offset_before,
		  irc[!dir].syn_correction_pos))
		newack = ntohl(tcph->ack_seq) - irc[!dir].syn_offset_after;
	else
		newack =
		    ntohl(tcph->ack_seq) - irc[!dir].syn_offset_before;
	newack = htonl(newack);
	UNLOCK_BH(&ip_irc_lock);

	DEBUGP("ip_nat_irc: after syn/ack adjust\n");
	tcph->check = ip_nat_cheat_check(~tcph->seq, newseq,
					 ip_nat_cheat_check(~tcph->ack_seq,
							    newack,
							    tcph->check));
	tcph->seq = newseq;
	tcph->ack_seq = newack;

	return NF_ACCEPT;
}

static struct ip_nat_helper ip_nat_irc_helpers[MAX_PORTS];
static char ip_nih_names[MAX_PORTS][6];

static struct ip_nat_expect irc_expect
    = { {NULL, NULL}, irc_nat_expected };


static void fini(void)
{
	int i;

	for (i = 0; i < ports_c; i++) {
		DEBUGP("ip_nat_irc: unregistering helper for port %d\n",
		       ports[i]);
		ip_nat_helper_unregister(&ip_nat_irc_helpers[i]);
	}
	ip_nat_expect_unregister(&irc_expect);
}
static int __init init(void)
{
	int ret;
	int i;
	struct ip_nat_helper *hlpr;
	char *tmpname;

	ret = ip_nat_expect_register(&irc_expect);
	if (ret == 0) {

		if (ports[0] == 0) {
			ports[0] = 6667;
		}

		for (i = 0; ports[i] != 0; i++) {
			hlpr = &ip_nat_irc_helpers[i];
			memset(hlpr, 0,
			       sizeof(struct ip_nat_helper));

			hlpr->tuple.dst.protonum = IPPROTO_TCP;
			hlpr->tuple.src.u.tcp.port = htons(ports[i]);
			hlpr->mask.src.u.tcp.port = 0xFFFF;
			hlpr->mask.dst.protonum = 0xFFFF;
			hlpr->help = help;

			tmpname = &ip_nih_names[i][0];
			sprintf(tmpname, "irc%2.2d", i);

			hlpr->name = tmpname;
			DEBUGP
			    ("ip_nat_irc: Trying to register helper for port %d: name %s\n",
			     ports[i], hlpr->name);
			ret = ip_nat_helper_register(hlpr);

			if (ret) {
				printk
				    ("ip_nat_irc: error registering helper for port %d\n",
				     ports[i]);
				fini();
				return 1;
			}
			ports_c++;
		}
	}
	return ret;
}


module_init(init);
module_exit(fini);
