/******************************************************
*
*	IP protocol-specific routines
*
******************************************************/

#include "sosdef.h"
#include "config.h"
#include "inet.h"
#include "socket.h"
#include "errno.h"

//#define	DEBUG_IP	1
//#define	DEBUG_IP_SEND	1

#if defined (pc)
extern	struct eth_device       am79970_eth_device;
extern	struct eth_device       i8255x_eth_device;
#elif defined (evmdm6467)
extern	struct eth_device       dm646x_emac_eth_device;
#elif defined (versatile)
extern	struct eth_device       smsc91c111_eth_device;
#endif

extern dword	timer_counter;

struct net_if	net_interfaces[MAX_NET_INTERFACES] =
{
#if defined (pc)
	{&am79970_eth_device, DEF_IP_ADDR, "\xFF\xFF\xFF\x00", 0 /*dynamic_ip*/, 0 /*dhcp_state*/},
//	{&i8255x_eth_device, "\xC0\xA8\x2A\x33", "\xFF\xFF\xFF\x00", 0, 0}
#elif defined (evmdm6467)
	{&dm646x_emac_eth_device, DEF_IP_ADDR, "\xFF\xFF\x80\x00", 0 /*dynamic_ip*/, 0 /*dhcp_state*/},
#elif defined (versatile)
//	{&smsc91c111_eth_device, DEF_IP_ADDR, "\xFF\xFF\x80\x00", 0 /*dynamic_ip*/, 0 /*dhcp_state*/},
#endif
};

unsigned char	def_ip_addr[4] = DEF_IP_ADDR;

word	ip_id;
byte	ip_ttl = DEF_IP_TTL;
byte	ip_tos = 0;
word	ip_flags = 0;

struct ip_frag_rec
{
	in_addr_t	ip;
	word	id;
	word	total_size;
	word	arrived;
	unsigned char	payload[MAX_IP_TOTAL_SIZE];
};


struct ip_frag_rec	*ip_frag_tbl;

/*
 *	Find local IP address - return 0-based index of the address (may be several)
 *	-1 means no such address
 */
int	find_ip_addr(unsigned char *ip)
{
	int	if_idx;

	for (if_idx = 0; if_idx < MAX_NET_INTERFACES; ++if_idx)
	{
		serial_printf("%s(): if_idx=%d comparing %d.%d.%d.%d with %d.%d.%d.%d\n", __func__, if_idx, (int)net_interfaces[if_idx].ip_addr[0], (int)net_interfaces[if_idx].ip_addr[1], (int)net_interfaces[if_idx].ip_addr[2], (int)net_interfaces[if_idx].ip_addr[3],
			(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
		if (memcmp(net_interfaces[if_idx].ip_addr, ip, 4) == 0)
			return	if_idx;
	}

	return	-1;
}

/*
 *	Get network interface according to IP address (include subnet)
 *
 *	(!) This function may be used to look up appropriate interface not only by bound local address, but also by destination address, too
 */
struct net_if	*get_net_interface(unsigned char *ip_addr)
{
	in_addr_t	ip, net_if_ip, mask;
	int	i;

	ip = ntohl(*(in_addr_t*)ip_addr);
	for (i = 0; i < MAX_NET_INTERFACES; ++i)
	{
		net_if_ip = ntohl(*(in_addr_t*)net_interfaces[i].ip_addr);
		mask = ntohl(*(in_addr_t*)net_interfaces[i].mask);
		if ((ip & mask) == (net_if_ip & mask))
			return	&net_interfaces[i];
	}

	// TODO -- Here we will look for network interface that can send to default gateway (direct send is N/A)

	return	NULL;
}


/*
 *	Prepare IP header
 *
 *	(!) src_ip and dest_ip must be already in network order
 */
void	prep_ip_hdr(struct ip_hdr *ip_hdr, word packet_len, byte protocol, const dword *src_ip, const dword *dest_ip)
{
	if (NULL == ip_hdr)
		return;
		
	ip_hdr->ver_ihl = 0x45;				// Protocol version 4, header length 20 bytes
	ip_hdr->tos = ip_tos;
	ip_hdr->total_len = htons(packet_len);
	ip_hdr->id = htons(ip_id++);		// Complete datagram
	ip_hdr->frag_offs = htons(0);		// Fragment if necessary, here last fragment
	ip_hdr->ttl = ip_ttl;
	ip_hdr->protocol = protocol;		// protocol
	ip_hdr->hdr_checksum = 0;
	
	if (src_ip != NULL)
		memcpy(&ip_hdr->src_ip, src_ip, 4);

	if (dest_ip != NULL)
		memcpy(&ip_hdr->dest_ip, dest_ip, 4);
	
	// Calculate header's checksum
	ip_hdr->hdr_checksum = htons(calc_ip_checksum((char*)ip_hdr, 20));
}


/*
 *	Send IP packet
 *
 *	Returns: success indicator (0 means IP unreachable by ARP)
 *	Assumption: data are already in transmit_data; size is in ethernet card's native order
 *	TODO: determine if we really need `size' parameter (size is already in IP header)
 *
 *	NOTE: the function does't alter IP header (which includes destination IP address). IP address is necessary to look up a network interface that
 *	holds already prepared frame
 *	NOTE: this function assumes that the caller may know (and in most cases will know) appropriate network interface, so it will send directly to netif
 *	and not look it up again. If the caller doesn't know netif, it should set this parameter to NULL
 */
int	ip_send_packet(struct net_if *net_if, unsigned char *ip_addr, unsigned size)
{
	struct	arp_tbl_entry	*dest;
	int	i;
	in_addr_t	src_ip, dest_ip, mask;
	int	rv;

#ifdef DEBUG_IP_SEND	
	static int test_cnt;
#endif

	dest = find_arp_entry(ip_addr);
#ifdef DEBUG_IP_SEND	
	serial_printf("[%d] %s(): after 1st find_arp_entry(%d.%d.%d.%d) dest = %08X\r\n", 
		++test_cnt, __func__, (int)ip_addr[0], (int)ip_addr[1], (int)ip_addr[2], (int)ip_addr[3], dest);
#endif		
	if (NULL == dest)
	{
		arp_discover(ip_addr);
		dest = find_arp_entry(ip_addr);
	}
	
	if (!dest)
	{
		errno = EHOSTUNREACH;
		return	0;
	}

	if (!net_if)
		net_if = get_net_interface(ip_addr);
	rv = eth_send_packet(net_if, dest->eth_addr, htons(PROTO_TYPE_IP), size);
	return	(rv == 0);
}


/*
 *	Send a fragmented (possibly) IP packet
 *	TODO: determine if we really need `size' parameter (size is already in IP header)
 *	TODO: consider sending last fragment first: if succeed and it arrives first, the destination immediately
 *	knows the total datagram's size
 *
 *	(!) `src' contains IP header and possibly (always?) transport protocol's headers. Therefore, `headers_length' parameter was added
 */
int	ip_send_packet_frag(unsigned char *ip_addr, const unsigned char *src, size_t headers_length, const unsigned char *payload, unsigned size)
{
	struct ip_hdr	*ip_hdr; 
	struct net_if	*net_if;
	int	rv;
	word	offs, len;
	unsigned char	*transmit_data;
	unsigned char	*ip_payload;
	const unsigned char *src_payload;

	net_if = get_net_interface(ip_addr);
	if (NULL == net_if)
	{
		errno = EHOSTUNREACH;
		return	0;		// Error: no appropriate interface
	}

	// An unwise thing to do, but this function may be called with a buffer that fits into MTU, including unnecessary copy
	if (size <= ETH_MTU)
	{
		net_if->eth_dev->get_send_packet(&transmit_data);
		ip_hdr = (struct ip_hdr*)transmit_data;
		memcpy(ip_hdr, src, size);
		return	ip_send_packet(net_if, ip_addr, size);
	}

	// Send packet in fragments
	offs = 0;
	while (size > 0)
	{
		net_if->eth_dev->get_send_packet(&transmit_data);
		ip_hdr = (struct ip_hdr*)transmit_data;
		memcpy(ip_hdr, src, sizeof(struct ip_hdr));
		if (size > ETH_MTU)
		{
			len = ETH_MTU & ~7;			// Multiple of 8
			ip_hdr->total_len = htons(len);
			ip_hdr->frag_offs = htons(offs >> 3 | IP_FLAG_MORE_FRAGS);		// Fragment if necessary, more fragments
		}
		else
		{
			len = size;			// Multiple of 8
			ip_hdr->total_len = htons(len);
			ip_hdr->frag_offs = htons(offs >> 3);		// Fragment if necessary, here last fragment
			
		}
		ip_hdr->hdr_checksum = 0;
	
		// Calculate header's checksum
		ip_hdr->hdr_checksum = htons(calc_ip_checksum((char*)ip_hdr, 20));

		ip_payload = (unsigned char*)ip_hdr + sizeof(struct ip_hdr);
		src_payload = payload;
		memcpy(ip_payload, src_payload + offs, len - sizeof(struct ip_hdr));
		rv = ip_send_packet(net_if, ip_addr, size);
		if (0 == rv)
			return	rv;
		offs += len;
	}

	return	1;
}


struct ip_frag_rec	*ip_frag_hash_find(in_addr_t ip, word id)
{
	dword	hash = (ip + id) % IP_FRAG_TBL_SIZE;			// We don't really care if we get it in network or host order, as long as ip and id are unique
	int	i;

	i = hash;
	do
	{
		if (ip_frag_tbl[i].ip == ip && ip_frag_tbl[i].id == id)
			return	&ip_frag_tbl[i];
		i = (i + 1) % IP_FRAG_TBL_SIZE;
	} while (i != hash);

	return	NULL;
}

struct ip_frag_rec	*ip_frag_hash_insert(struct ip_hdr *ip_hdr)
{
	dword	hash = (ip_hdr->src_ip + ip_hdr->id) % IP_FRAG_TBL_SIZE;			// We don't really care if we get it in network or host order, as long as ip and id are unique
	int	i;
	struct	ip_frag_rec *ip_frag_rec;
	word	frag_offs;
	unsigned char	*payload;
	word	payload_len;

	ip_frag_rec = ip_frag_hash_find(ip_hdr->src_ip, ip_hdr->id);
	if (NULL == ip_frag_rec)
	{
		for (i = hash; !(0 == ip_frag_tbl[i].ip && 0 == ip_frag_tbl[i].id); i = (i+1) % IP_FRAG_TBL_SIZE)
			if (i == hash)
				break;
		ip_frag_rec = &ip_frag_tbl[i];
		ip_frag_rec->total_size = 0;
		ip_frag_rec->arrived = 0;
	}

	frag_offs = ip_hdr->frag_offs << 3 & 0xFFFF;				// Already in host order
	payload = (unsigned char*)ip_hdr + sizeof(struct ip_hdr);
	payload_len = ip_hdr->total_len - sizeof(struct ip_hdr);
	memcpy(ip_frag_rec->payload + frag_offs, payload, payload_len);
	ip_frag_rec->arrived += payload_len;
	if (0 == (ip_hdr->frag_offs & IP_FLAG_MORE_FRAGS))
		ip_frag_rec->total_size = frag_offs + payload_len;

	return	ip_frag_rec;
}

// Unlike the name sounds, this only sets ip, id, size and arrived to 0s, and does not delete payload
void	ip_frag_hash_delete(struct ip_frag_rec *ip_frag_rec)
{
	ip_frag_rec->ip = 0;
	ip_frag_rec->id = 0;
	ip_frag_rec->total_size = 0;
	ip_frag_rec->arrived = 0;
}


/*
 *	Parse IP protocol header and decide what to do.
 *
 *	Currently we don't check whether dest IP is ours...
 */
void	parse_ip(struct net_if *net_if, char *pdata)
{
	struct	ip_hdr	*ip_hdr;
	dword	checksum, got_checksum;
	int	i;
	word	frag_offs, total_length;
	unsigned char	*ip_payload;
#ifdef	DEBUG_IP
	static int	cnt;
#endif
	ip_hdr = (struct ip_hdr*)pdata;

	got_checksum = htons(ip_hdr->hdr_checksum);

	// Calculate our checksum to verify
	ip_hdr->hdr_checksum = 0;
	checksum = calc_ip_checksum(pdata, (ip_hdr->ver_ihl & 0xF) << 2);
#ifdef	DEBUG_IP
	serial_printf("IP: MY checksum=%04X, GOT checksum=%04X\r\n", checksum, got_checksum);
#endif

	ip_hdr->total_len = htons(ip_hdr->total_len);
	ip_hdr->id = htons(ip_hdr->id);
	ip_hdr->frag_offs = htons(ip_hdr->frag_offs);
	ip_hdr->hdr_checksum = htons(ip_hdr->hdr_checksum);

#ifdef	DEBUG_IP
	serial_printf("(%d) IP: ver_ihl=%02X, tos=%02X, total_len=%04X, id=%04X, frag_offs=%04X, ttl=%02X, protocol=%02X, hdr_checksum=%04X, src_ip=%08X, dest_ip=%08X\r\n", ++cnt,
		(unsigned)ip_hdr->ver_ihl, (unsigned)ip_hdr->tos, (unsigned)ip_hdr->total_len,
		(unsigned)ip_hdr->id, (unsigned)ip_hdr->frag_offs, (unsigned)ip_hdr->ttl,
		(unsigned)ip_hdr->protocol, (unsigned)ip_hdr->hdr_checksum, (unsigned)ip_hdr->src_ip,
		(unsigned)ip_hdr->dest_ip);
#endif

#if 0
	// Drop packets that are not designated for us and are not broadcast
	if ((ip_hdr->dest_ip & ~*(unsigned*)net_if->mask) != (INADDR_BROADCAST & ~*(unsigned*)net_if->mask) && ip_hdr->dest_ip != *(unsigned*)net_if->ip_addr)
	{
serial_printf("Dropping packet - not broadcast and not ours (%08X)\n", *(unsigned*)net_if->ip_addr);
		return;
	}
#endif

	// TODO: drop malformed packets (with bad checksum or unrecognized fields in headers)

	// TODO: add routing functions, which should be easy (no real checking, no reassembly, just forward according to rules and possibly fragment)
	// We will need, however, to support routing protocols (like BGP), advertisements and alike

	// TODO: reassemble IP packets if they arrived fragmented
	// Reassembly shall consider:
	//
	// 	1) source IP (dest is ours, when routing we won't care about reassemby even though we may fragment more during forwarding)
	// 	2) "More fragments" flag
	// 	3) Fragment offset field
	// 	4) Total length
	// 	5) (!) IP ID - we may receive many parts of different fragmented IP packets from the same host at the same time
	//
	// Reassembly will use a hash table of configurable size, keys in which will be source IP and IP ID (some combination)
	// Should be relatively easy
#if 0
	if ((ip_hdr->frag_offs & 0x1FFF) != 0 || (ip_hdr->frag_offs & IP_FLAG_MORE_FRAGS))
	{
		struct ip_frag_rec	*ip_frag_rec = ip_frag_hash_insert(ip_hdr);

		// Until total_size is arrived, nothing to do
		if (0 == ip_frag_rec->total_size || ip_frag_rec->total_size != ip_frag_rec->arrived)
			return;

		// Reconstruct original IP datagram - at least fields that higher-level protocols need (checksum, fragments etc. are already consumed), total_len and src_ip
		ip_hdr->total_len = ip_frag_rec->total_size + sizeof(struct ip_hdr);
		ip_payload = ip_frag_rec->payload;

		// Free entry for reuse while going on parsing
		ip_frag_hash_delete(ip_frag_rec);

	}
	else
#endif
	{
		ip_payload = pdata+((ip_hdr->ver_ihl & 0xF) << 2);
	}

	// Pass packet over to the correct transport layer protocol handler
	switch(ip_hdr->protocol)
	{
	default:
		break;
	case IPPROTO_ICMP:
		parse_icmp(net_if, ip_payload, ip_hdr);
		break;
	case IPPROTO_TCP:
		parse_tcp(net_if, ip_payload, ip_hdr);
		break;
	case IPPROTO_UDP:
		parse_udp(net_if, ip_payload, ip_hdr);
		break;
	}
}

/*
 *	Calculates IP checksum on pdata buffer. len is length in bytes (octets)
 */
word	calc_ip_checksum(char *pdata, int len)
{
	int	i;
	dword	checksum;
	int	remainder;

	remainder = len & 1;
	len >>= 1;
	checksum = 0;
	for (i = 0; i < len; ++i)
	{
		checksum += htons(((word*)pdata)[i]);
		checksum += (checksum & 0x10000) >> 16;
		checksum &= 0xFFFF;
	}

	if (remainder)
	{
		checksum += htons((word)pdata[(len << 1)]);
		checksum += (checksum & 0x10000) >> 16;
		checksum &= 0xFFFF;
	}

	return	~checksum;
}

void	init_ip(void)
{
	int	i;
	uint32_t	tmp;

	enable_irqs();
	ip_frag_tbl = calloc(IP_FRAG_TBL_SIZE, sizeof(struct ip_frag_rec));

	for (i = 0; i < MAX_NET_INTERFACES; ++i)
	{
		if (net_interfaces[i].eth_dev != NULL && net_interfaces[i].dynamic_ip)
		{
			serial_printf("%s(): configuring net_if #%d for dynamic IP (DHCP)...\n", __func__, i);
			dhcp_discover(&net_interfaces[i]);
			// Wait until timer_counter advances, since it's basis for DHCP transaction ID
			for (tmp = timer_counter; tmp == timer_counter; )
				;
		}
		else
		{
			if (!memcmp(net_interfaces[i].ip_addr, "\x0\x0\x0\x0", 4))
				serial_printf("%s(): net_if #%d is not configured\n", __func__, i);
			else
				serial_printf("%s(): net_if #%d is statically configured to ip %s\n", __func__, i, inet_ntoa(*(struct in_addr*)&net_interfaces[i].ip_addr));
		}
	}
}

