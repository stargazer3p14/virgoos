/****************************************************
 *
 *	ARP necessary implementation: ARP tables search, update, discovery and response
 *
 ****************************************************/
 
#include "sosdef.h"
#include "config.h"
#include "inet.h"
#include "errno.h"

//#define	DEBUG_ARP	1
//#define	DEBUG_ARP_TBL	1

extern dword	timer_counter;
extern struct net_if   net_interfaces[MAX_NET_INTERFACES];

struct	arp_tbl_entry	arp_tbl[ARP_TBL_SIZE];
int	arp_replied;

/*
 *	ARP table is organized as hash table with open addressing.
 *	If the table is full, the new entry gets the most precise position (with no bias)
 */
void	add_arp_entry(struct arp_tbl_entry *entry)
{
	dword	pos;
	int	i;

	pos = (unsigned)entry->ip_addr[0]+(unsigned)entry->ip_addr[1]+(unsigned)entry->ip_addr[2]+(unsigned)entry->ip_addr[3];
	for(i = pos % ARP_TBL_SIZE; i < ARP_TBL_SIZE; ++i)
		if (0 == arp_tbl[i].ip_addr[0] && 0 == arp_tbl[i].ip_addr[1] && 0 == arp_tbl[i].ip_addr[2] && 0 == arp_tbl[i].ip_addr[3])
		{
			memcpy(arp_tbl + i, entry, sizeof(struct arp_tbl_entry));
			return;
		}

	memcpy(arp_tbl + pos % ARP_TBL_SIZE, entry, sizeof(struct arp_tbl_entry));
}


struct	arp_tbl_entry	*find_arp_entry(unsigned char *ip_addr)
{
	dword	pos;
	int	i;

	pos = (unsigned)ip_addr[0]+(unsigned)ip_addr[1]+(unsigned)ip_addr[2]+(unsigned)ip_addr[3];
	for(i = pos % ARP_TBL_SIZE; i < ARP_TBL_SIZE; ++i)
		if (memcmp(arp_tbl[i].ip_addr, ip_addr, 4) == 0)
			return	arp_tbl + i;

	return	NULL;
}


int	arp_discover(char *addr)
// Send ARP request to find out peer entry
{
	struct	arp_packet	*arp;
	dword	curr_time;
	int	i;
	struct net_if	*net_if;

	arp_replied = 0;
	// Send ARP "discover" to ALL interfaces
	// Here we may send to all interfaces in a loop; since we're broadcasting there's no point in looking up ethernet tables
	// TODO: fix that to send only to interface that matches the desired network
	net_if = get_net_interface(addr);
	if (!net_if)
	{
		errno = ENETUNREACH;
		return	0;
	}

	net_if->eth_dev->get_send_packet((unsigned char**)&arp);
	arp->hw_type = htons(HW_TYPE_ETHERNET);
	arp->proto_type = htons(PROTO_TYPE_IP);
	arp->hw_len = ETH_ADDR_SIZE;
	arp->proto_len = IP_ADDR_SIZE;
	arp->oper = htons(ARP_OPER_REQUEST);
	memcpy(arp->sender_hw_addr, net_interfaces[i].eth_dev->addr, ETH_ADDR_SIZE);
	memcpy(arp->sender_proto_addr, net_interfaces[i].ip_addr, IP_ADDR_SIZE);
	memset(arp->target_hw_addr, 0, ETH_ADDR_SIZE);
	memcpy(arp->target_proto_addr, addr, IP_ADDR_SIZE);
	net_if->eth_dev->send_packet(ETH_ADDR_BROADCAST, htons(PROTO_TYPE_ARP), sizeof(struct arp_packet));

#ifdef	DEBUG_ARP
	serial_printf("Sent ARP packet, waiting 1(s)...\n");
#endif

	// Wait up to 1 (s)
	enable_irqs();
	curr_time = timer_counter;
	while (curr_time + 1 * TICKS_PER_SEC > timer_counter)
		if (arp_replied)
			break;
			
#ifdef	DEBUG_ARP
	if (arp_replied)
		serial_printf("ARP replied in %u(ms)\n", timer_counter - curr_time);
	else
		serial_printf("ARP NOT replied in %u(ms)\n", timer_counter - curr_time);
#endif

	return	arp_replied;
}


/*
 *	Parse ARP requests and decide what to do.
 *
 *	Currently we don't check whether dest IP is ours...
 */
void	parse_arp(struct net_if *net_if, char *pdata)
{
	struct arp_packet	*arp, *ask_arp, *ans_arp;

	arp = (struct arp_packet*)pdata;
	arp->hw_type = htons(arp->hw_type);
	arp->proto_type = htons(arp->proto_type);
	arp->oper = htons(arp->oper);
#ifdef	DEBUG_ARP
	{
		int	i;
	
		serial_printf("ARP: hw_type=%04X proto_type=%04X hw_len=%d, proto_len=%d, oper=%04X\r\n",
			(unsigned)arp->hw_type, (unsigned)arp->proto_type, (int)arp->hw_len, (int)arp->proto_len, (unsigned)arp->oper);

		serial_printf("+++\r\n");
		serial_printf("Sender hw address: ");
		for (i = 0; i < arp->hw_len; ++i)
			serial_printf("%02X:", (unsigned)arp->sender_hw_addr[i]);
		serial_printf("\r\n");
		serial_printf("Target hw address: ");
		for (i = 0; i < arp->hw_len; ++i)
			serial_printf("%02X:", (unsigned)arp->target_hw_addr[i]);
		serial_printf("\r\n");
		serial_printf("+++\r\n");
		if ((unsigned)arp->oper == ARP_OPER_REQUEST)
			serial_printf("Tell ");
		else if ((unsigned)arp->oper == ARP_OPER_REPLY)
			serial_printf("Answer ");
		for (i = 0; i < arp->proto_len; ++i)
			serial_printf("%d.", (unsigned)arp->sender_proto_addr[i]);
		if ((unsigned)arp->oper == ARP_OPER_REQUEST)
			serial_printf(" who has ");
		else if ((unsigned)arp->oper == ARP_OPER_REPLY)
			serial_printf(" is at ");
		for (i = 0; i < arp->proto_len; ++i)
			serial_printf("%d.", (unsigned)arp->target_proto_addr[i]);
		serial_printf("\r\n");
		serial_printf("+++\r\n");
	}
#endif

	if (ARP_OPER_REQUEST == arp->oper)
	{
		int	nnn;
		
#if 0
serial_printf("ARP_OPER_REQUEST: net_if->ip_addr=%d.%d.%d.%d arp->target_proto_addr=%d.%d.%d.%d memcmp()=%d\r\n",
	(int)net_if->ip_addr[0], (int)net_if->ip_addr[1], (int)net_if->ip_addr[2], (int)net_if->ip_addr[3],
	(int)arp->target_proto_addr[0], (int)arp->target_proto_addr[1], (int)arp->target_proto_addr[2], (int)arp->target_proto_addr[3],
	memcmp(net_if->ip_addr, arp->target_proto_addr, IP_ADDR_SIZE));
#endif
		// Send ARP reply...
		if (memcmp(net_if->ip_addr, arp->target_proto_addr, IP_ADDR_SIZE) == 0)
		{
//#ifdef	DEBUG_ARP
			serial_printf("Responding to ARP request...\r\n");
//#endif

#ifdef	DEBUG_ARP
			serial_printf("%s(): net_if=%08X ip_addr=%d.%d.%d.%d target_proto_addr=%d.%d.%d.%d\n", __func__, net_if, (int)net_if->ip_addr[0], (int)net_if->ip_addr[1], (int)net_if->ip_addr[2], (int)net_if->ip_addr[3],
				(int)arp->target_proto_addr[0], (int)arp->target_proto_addr[1], (int)arp->target_proto_addr[2], (int)arp->target_proto_addr[3]);
#endif
			ans_arp = NULL;
#ifndef evmdm6467
			eth_get_send_packet(net_if, (unsigned char**)&ans_arp);
			if (!ans_arp)
				return;			// If we were refused, nothing more to do
#else
			net_if->eth_dev->get_send_packet((unsigned char**)&ans_arp);
#endif
#ifdef	DEBUG_ARP
			serial_printf("%s(): result from get_send_packet(): ans_arp=%08X [get_send_packet=%08X]\n", __func__, ans_arp, net_if->eth_dev->get_send_packet);
#endif
			ans_arp->hw_type = htons(HW_TYPE_ETHERNET);
			ans_arp->proto_type = htons(PROTO_TYPE_IP);
			ans_arp->hw_len = ETH_ADDR_SIZE;
			ans_arp->proto_len = IP_ADDR_SIZE;
			ans_arp->oper = htons(ARP_OPER_REPLY);
			memcpy(ans_arp->sender_hw_addr, net_if->eth_dev->addr, ETH_ADDR_SIZE);
			memcpy(ans_arp->sender_proto_addr, net_if->ip_addr, IP_ADDR_SIZE);
			memcpy(ans_arp->target_hw_addr, arp->sender_hw_addr, ETH_ADDR_SIZE);
			memcpy(ans_arp->target_proto_addr, arp->sender_proto_addr, IP_ADDR_SIZE);
#ifdef	DEBUG_ARP
			serial_printf("%s(): calling net_if's send_packet() -- %08X\n", __func__, net_if->eth_dev->send_packet);
#endif

#ifndef	evmdm6467
			eth_send_packet(net_if, arp->sender_hw_addr, htons(PROTO_TYPE_ARP), sizeof(struct arp_packet));
#else
{
// There is something weird about DM6467 EMAC in this part
extern	void dm646x_emac_send_packet(unsigned char *dest_addr, word protocol, unsigned size);
			dm646x_emac_send_packet(arp->sender_hw_addr, htons(PROTO_TYPE_ARP), sizeof(struct arp_packet));
}
#endif
#ifdef	DEBUG_ARP
			serial_printf("%s(): returned from net_if's send_packet()\n", __func__);
#endif
		}
	}
	// Response to an ARP request. If the sender is not known to us, add it to ARP table
	else if (ARP_OPER_REPLY == arp->oper)
	{
		struct	arp_tbl_entry	arp_entry, *found_arp_entry;
		char	*remote_ip_addr;

		remote_ip_addr = arp->sender_proto_addr;
		found_arp_entry = find_arp_entry(remote_ip_addr);

		if (NULL == found_arp_entry)
		{
			memcpy(arp_entry.ip_addr, remote_ip_addr, IP_ADDR_SIZE);
			memcpy(arp_entry.eth_addr, arp->sender_hw_addr, ETH_ADDR_SIZE);
			add_arp_entry(&arp_entry);
#ifdef	DEBUG_ARP_TBL
			serial_printf("Added ARP entry\r\n");
#endif
		}
		arp_replied = 1;
	}
}


void	update_arp_tbl(unsigned char *remote_ip_addr, struct eth_frame_hdr *frame_hdr)
{
	struct	arp_tbl_entry	arp_entry, *found_arp_entry;
	
	// Search for sender in ARP table and update if it is not found
	// What should be done if found and another MAC claims the same IP? (IP collision) - for now, ignore the
	// newcomer
	found_arp_entry = find_arp_entry(remote_ip_addr);
#ifdef	DEBUG_ARP_TBL
	serial_printf("Rcv ARP-interesting packet: IP=%d.%d.%d.%d, ETH=%02X:%02X:%02X:%02X:%02X:%02X, found_arp_entry=%08X\r\n",
		(int)remote_ip_addr[0], (int)remote_ip_addr[1], (int)remote_ip_addr[2], (int)remote_ip_addr[3],
		(unsigned)frame_hdr->src_addr[0], (unsigned)frame_hdr->src_addr[1],
		(unsigned)frame_hdr->src_addr[2], (unsigned)frame_hdr->src_addr[3],
		(unsigned)frame_hdr->src_addr[4], (unsigned)frame_hdr->src_addr[5],
		found_arp_entry);
	if (found_arp_entry != NULL)
		serial_printf("found_arp_entry->ip_addr=%d.%d.%d.%d, found_arp_entry->eth_addr=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			(int)found_arp_entry->ip_addr[0], (int)found_arp_entry->ip_addr[1], (int)found_arp_entry->ip_addr[2], 
			(int)found_arp_entry->ip_addr[3],
			(unsigned)found_arp_entry->eth_addr[0], (unsigned)found_arp_entry->eth_addr[1], 
			(unsigned)found_arp_entry->eth_addr[2], (unsigned)found_arp_entry->eth_addr[3],
			(unsigned)found_arp_entry->eth_addr[4], (unsigned)found_arp_entry->eth_addr[5]);
#endif
	if (NULL == found_arp_entry)
	{
		memcpy(arp_entry.ip_addr, remote_ip_addr, 4);
		memcpy(arp_entry.eth_addr, frame_hdr->src_addr, 6);
		add_arp_entry(&arp_entry);
#ifdef	DEBUG_ARP_TBL
		serial_printf("Added ARP entry\r\n");
#endif
	}
}


/*
 *	Send gratuitious (advertisement) ARP request on net_if
 */
void	send_grat_arp(struct net_if *net_if)
{
	struct  arp_packet *grat_arp;
	net_if->eth_dev->get_send_packet((unsigned char**)&grat_arp);

	grat_arp->hw_type = htons(HW_TYPE_ETHERNET);
	grat_arp->proto_type = htons(PROTO_TYPE_IP);
	grat_arp->hw_len = 6;
	grat_arp->proto_len = 4;
	grat_arp->oper = htons(ARP_OPER_REQUEST);
	memcpy(grat_arp->sender_hw_addr, net_if->eth_dev->addr, 6);
	memcpy(grat_arp->sender_proto_addr, net_if->ip_addr, 4);
	memset(grat_arp->target_hw_addr, 0, 6);
	memcpy(grat_arp->target_proto_addr, net_if->ip_addr, 4);
	net_if->eth_dev->send_packet(ETH_ADDR_BROADCAST, htons(PROTO_TYPE_ARP), sizeof(struct arp_packet));
}

