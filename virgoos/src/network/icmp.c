/********************************************
 *
 *	icmp.c
 *
 *	Implementation of necessary ICMP (meanwhile only response to echo-request)
 *
 ********************************************/
#include "sosdef.h"
#include "config.h"
#include "inet.h"

#define	DEBUG_ICMP	1

extern word	ip_id;
extern struct	net_if	net_interfaces[MAX_NET_INTERFACES];

/*
 *	Parse (and may be reply to) ICMP
 *
 *	For now, we just respond to the sender.
 *	main IP parser updates ARP table with sender's details.
 *	(!) Called in IRQ context
 */
void	parse_icmp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr)
{
	struct	icmp_hdr	*icmp_hdr;
	struct	icmp_echo_msg	*echo_req, *echo_reply;
	struct	ip_hdr	*ip_hdr;
	int	length;
	unsigned char	*transmit_data;

	icmp_hdr = (struct icmp_hdr*)pdata;

	icmp_hdr->hdr_checksum = htons(icmp_hdr->hdr_checksum);
#ifdef	DEBUG_ICMP
	serial_printf("ICMP: type=%02X, code=%02X hdr_checksum=%04X\r\n",
		(unsigned)icmp_hdr->type, (unsigned)icmp_hdr->code, (unsigned)icmp_hdr->hdr_checksum);
#endif

	switch(icmp_hdr->type)
	{
	default:
		break;
	case ICMP_TYPE_ECHOREQUEST:
		echo_req = (struct icmp_echo_msg*)pdata;
		echo_req->id = htons(echo_req->id);
		echo_req->seq_num = htons(echo_req->seq_num);

		// Respond to ECHO_REQUEST
#ifdef	DEBUG_ICMP
		serial_printf("Responding to \"Echo request\" message (id=%04X, seq_num=%04X); remote_ip_addr=%d.%d.%d.%d\r\n",
			(unsigned)echo_req->id, (unsigned)echo_req->seq_num,
			((unsigned char*)&net_if->eth_dev->remote_ip_addr)[0], ((unsigned char*)&net_if->eth_dev->remote_ip_addr)[1],
			((unsigned char*)&net_if->eth_dev->remote_ip_addr)[2], ((unsigned char*)&net_if->eth_dev->remote_ip_addr)[3]);
#endif

		eth_get_send_packet(net_if, &transmit_data);
		if (!transmit_data)
		{
#ifdef DEBUG_ICMP
			serial_printf("%s(): get_send_packet() returned NULL\n", __func__);
#endif
			return;
		}

		ip_hdr = (struct ip_hdr*)transmit_data;

		length = remote_ip_hdr->total_len;

		// Fill ICMP message
		echo_reply = (struct icmp_echo_msg*)((char*)ip_hdr + 20);
		echo_reply->hdr.type = ICMP_TYPE_ECHOREPLY;
		echo_reply->hdr.code = 0;
		echo_reply->hdr.hdr_checksum = 0;
		echo_reply->id = htons(echo_req->id);
		echo_reply->seq_num = htons(echo_req->seq_num);

		memcpy((char*)ip_hdr + 28, (char*)remote_ip_hdr + 28, length - 28);

		echo_reply->hdr.hdr_checksum = calc_ip_checksum((char*)echo_reply, length - 20);
		echo_reply->hdr.hdr_checksum = htons(echo_reply->hdr.hdr_checksum);

		// Prepare IP header
		prep_ip_hdr(ip_hdr, length, // ECHO_RESPONSE message length (push back original message)
			IPPROTO_ICMP, (dword*)net_if->ip_addr, &remote_ip_hdr->src_ip);
			
		// Consider using eth_send_packet() or move response to ICMP echo_request out of IRQ
//		eth_send_packet(remote_eth_addr, htons(PROTO_TYPE_IP), length);	// WORKS

		// This call may optionally invoke arp_discover(), which is not acceptable in IRQ
		// However, this is irrelevant because we *know* that remote address is already in ARP table (if it wasn't already, upon packet reception the IRQ handler added it)
		// Therefore, principally, this function is safe to use in IRQ handler for *response* packets
		ip_send_packet(net_if, (unsigned char*)&net_if->eth_dev->remote_ip_addr, length);
		
		break;

	case ICMP_TYPE_ECHOREPLY:
		break;
	}
}
