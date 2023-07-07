/****************************************************
 *
 *	ethernet.c
 *
 *	Ethernet-specific parsing and sending functions. Functionality related to parsing ethernet header and ethernet bridging
 *
 ****************************************************/

#include "sosdef.h"
#include "inet.h"

//#define	DEBUG_ETH_HEADER	1
//#define	DEBUG_SNAP_HEADER	1

extern	struct net_if	net_interfaces[MAX_NET_INTERFACES];

/*
 *	Ethernet table is organized as hash table with open addressing.
 *	If the table is full, the new entry gets the most precise position (with no bias)
 *	Implementation is copied from ARP tables
 */
void	add_eth_entry(struct eth_device *eth_dev, unsigned char *addr)
{
	dword	pos;
	int	i;

	pos = (unsigned)addr[0]+(unsigned)addr[1]+(unsigned)addr[2]+(unsigned)addr[3]+(unsigned)addr[4]+(unsigned)addr[5];
	for(i = pos % ETH_TBL_SIZE; i < ETH_TBL_SIZE; ++i)
		if (0 == eth_dev->eth_tbl[i].addr[0] && 0 == eth_dev->eth_tbl[i].addr[1] && 0 == eth_dev->eth_tbl[i].addr[2] && 0 == eth_dev->eth_tbl[i].addr[3] &&
			0 == eth_dev->eth_tbl[i].addr[4] && 0 == eth_dev->eth_tbl[i].addr[5])
		{
			memcpy(eth_dev->eth_tbl + i, addr, 6);
			return;
		}

	memcpy(eth_dev->eth_tbl + pos % ETH_TBL_SIZE, addr, 6);
}


struct	eth_tbl_entry	*find_eth_entry(struct eth_device *eth_dev, unsigned char *addr)
{
	dword	pos;
	int	i;

	pos = (unsigned)addr[0]+(unsigned)addr[1]+(unsigned)addr[2]+(unsigned)addr[3]+(unsigned)addr[4]+(unsigned)addr[5];
	for(i = pos % ETH_TBL_SIZE; i < ETH_TBL_SIZE; ++i)
		if (memcmp(eth_dev->eth_tbl[i].addr, addr, 6) == 0)
			return	eth_dev->eth_tbl + i;

	return	NULL;
}


/*
 *	A generic ethernet frame processing function
 */
void	eth_parse_packet(struct net_if *this, void *pkt)
{
	struct eth_frame_hdr	*eth_frame_hdr = (struct eth_frame_hdr*)pkt;
	word	frame_size;
	word	proto_type;
	void	*pdata;
	struct	llc_hdr	*llc_hdr;
	struct	snap_hdr *snap_hdr;
	
	// Prior to any parsing, add source address to this'es ethernet table (if it's not a multicast/broadcast address)
	if (0 == (eth_frame_hdr->src_addr[0] & 0x80))
		if (NULL == find_eth_entry(this->eth_dev, eth_frame_hdr->src_addr))
			add_eth_entry(this->eth_dev, eth_frame_hdr->src_addr);

	// Parse frame
	frame_size = ntohs(eth_frame_hdr->frame_size);
	if (frame_size > 0x600)
	{
#ifdef	DEBUG_ETH_HEADER
		serial_printf("%s(): Ethernet-II frame: eth. type=%04X\r\n", __func__, frame_size);
#endif
		proto_type = frame_size;
		pdata = (unsigned char*)eth_frame_hdr + sizeof(struct eth_frame_hdr);
	}
	else
	{
		llc_hdr = (struct llc_hdr*)((unsigned char*)pkt + sizeof(struct eth_frame_hdr));
#ifdef	DEBUG_ETH_HEADER
		serial_printf("%s(): 802.3 frame: length=%04X. LLC header: DSAP=%c, %d SSAP=%c, %d Control = %02X\r\n", __func__, 
			frame_size,
			(llc_hdr->dsap & 1) != 0 ? 'G' : 'I',
			llc_hdr->dsap & ~1,
			(llc_hdr->ssap & 1) != 0 ? 'C' : 'R',
			llc_hdr->ssap & ~1,
			llc_hdr->control
		);
#endif

		/* SNAP */
		if ((llc_hdr->dsap & ~1) == 0xAA && (llc_hdr->ssap & ~1) == 0xAA)
		{
			snap_hdr = (struct snap_hdr*)((unsigned char*)llc_hdr + 3);
			snap_hdr->type = htons(snap_hdr->type);
#ifdef DEBUG_SNAP_HEADER
			serial_printf("%s(): SNAP: OUI=%02X%02X%02X, type = %04X\r\n", __func__, 
				snap_hdr->oui[0], snap_hdr->oui[1], snap_hdr->oui[2],
				snap_hdr->type
			);
#endif

			proto_type = ntohs(snap_hdr->type);
			pdata = (unsigned char*)llc_hdr + 3 + 5;
		}
	}

	// Temporary interface - until send works
//	transmit_data = (char*)(*(dword*)&am79970_xmit_desc_ring[curr_td].tmd0 & 0xFFFFFF) + sizeof(struct eth_frame_hdr);
	
	// Handle ARP
	if (PROTO_TYPE_ARP == proto_type)
	{
		parse_arp(this, pdata);
	}
	// Handle IP
	else if (PROTO_TYPE_IP == proto_type)
	{
		struct	ip_hdr	*ip_hdr = (struct ip_hdr*)pdata;
		
		// Save remote IP address (header is better?..)
		//memmove(remote_ip_addr, &ip_hdr->src_ip, 4);
		this->eth_dev->remote_ip_addr = ip_hdr->src_ip;

		// Update ARP tables with the newcomming address
		update_arp_tbl((unsigned char*)&this->eth_dev->remote_ip_addr, eth_frame_hdr);
		parse_ip(this, pdata);
	}
}


/*
 *	eth_get_send_packet() and eth_send_packet() are introduced in order to allow generic locking of buffer -
 *	it will not be distributed to another client in case that the caller gets preempted before calling eth_send_packet()
 */
void    eth_get_send_packet(struct net_if *this, unsigned char **payload)
{
	if (this->eth_dev->flags & ETH_DEV_SEND_LOCKED)
	{
		*payload = NULL;
		return;
	}

	this->eth_dev->flags |= ETH_DEV_SEND_LOCKED;
	return	this->eth_dev->get_send_packet(payload);
}

int	eth_send_packet(struct net_if *this, unsigned char *dest_addr, word protocol, unsigned size)
{
	int	rv;

	if (!(this->eth_dev->flags & ETH_DEV_SEND_LOCKED))
		return	-1;
	
	rv = this->eth_dev->send_packet(dest_addr, protocol, size);
	this->eth_dev->flags &= ~ETH_DEV_SEND_LOCKED;
	return	rv;
}


