/*
 *	udp.c
 *
 *	Implementation of UDP protocol for SeptemberOS
 */

#include "socket.h"
#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "inet.h"

//#define	DEBUG_UDP	1
//#define	DEBUG_UDP_SEND	1
//#define	DEBUG_CHECKSUM	1

extern struct socket	*psockets;
//extern char	*transmit_data;
//extern char	ip_addr[4];
//extern char	eth_addr[6];
extern word	ip_id;
extern EVENTS_SEL_Q *sel_q_hd;
extern struct file	*files;

/*
 *	UDP datagrams are copied to socket buffer in the following format:
 *		ssize_t	len
 *		struct sockaddr_in      peer_addr
 *		char	data[len]
 *		
 *		sock->data_len includes all fields. First field (len) contains length of only datagram (not including peer address and itself)
 */
static void	copy_data_to_sock(struct socket *sock, char *data, ssize_t len, struct ip_hdr *remote_ip_hdr, word port)
{
	struct sockaddr_in	peer_addr;

	if (sock->data_len+len+sizeof(len)+sizeof(peer_addr) > SOCKBUF_LEN)
		return;

	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = port;
	peer_addr.sin_addr.s_addr = remote_ip_hdr->src_ip;
	memset(&peer_addr.sin_zero, 0, 8);

	memcpy(sock->buf+sock->data_len, &len, sizeof(len));
	memcpy(sock->buf+sock->data_len+sizeof(len), &peer_addr, sizeof(peer_addr));
	memcpy(sock->buf+sock->data_len+sizeof(len)+sizeof(peer_addr), data, len);
	sock->data_len += len+sizeof(len)+sizeof(peer_addr);
}


/*
 *	Calculates UDP header checksum
 *
 *	In:	pseudo_hdr - pointer to UDP pseudo header with information from IP header
 *		pdata - pointer to UDP header, followed by data
 *		len - total length of UDP header + data
 *	Out: checksum
 */
word	calc_udp_checksum(char *pseudo_hdr, char *pdata, unsigned len)
{
	int	i;
	dword	checksum;
	int	remainder;
	dword	limit;

#ifdef	DEBUG_CHECKSUM
	printfxy(0, 2, "pseudo_hdr: ");
	for (i = 0; i < sizeof(struct udp_pseudo_hdr); ++i)
		printfxy(12+i*3, 2, "%02X", (unsigned)(unsigned char)pseudo_hdr[i]);

	printfxy(0, 3, "pdata: ");
	for (i = 0; i < len; ++i)
		printfxy(8+i*3, 3, "%02X", (unsigned)(unsigned char)pdata[i]);
#endif

	limit = sizeof(struct udp_pseudo_hdr) / 2;
	checksum = 0;
	for (i = 0; i < limit; ++i)
	{
		checksum += htons(((word*)pseudo_hdr)[i]);
		checksum += (checksum & 0x10000) >> 16;
		checksum &= 0xFFFF;
	}

	remainder = len & 1;
	len >>= 1;
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


/*
 *	Parse UDP and may be deliver to binded socket
 *
 *	(!) Called in IRQ context
 */
void	parse_udp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr)
{
	int	i;
	word	port;
	struct	udp_hdr	*udp_hdr;
	char	*udp_data;
	ssize_t	len;
	struct	udp_pseudo_hdr	pseudo_hdr;
	word	checksum;
	word	received_checksum;
	
	udp_hdr = (struct udp_hdr*)pdata;
	udp_data = pdata + 8;
	len = ntohs(udp_hdr->length) - 8;

	// Check UDP checksum (just make sure we do it right)
	pseudo_hdr.zero = 0;
	pseudo_hdr.protocol = remote_ip_hdr->protocol;
	pseudo_hdr.total_length = remote_ip_hdr->total_len - (remote_ip_hdr->ver_ihl & 0xF) * 4;
	pseudo_hdr.total_length = htons(pseudo_hdr.total_length);
	memcpy(&pseudo_hdr.src_ip, &remote_ip_hdr->src_ip, 4);
	memcpy(&pseudo_hdr.dest_ip, &remote_ip_hdr->dest_ip, 4);

	received_checksum = htons(udp_hdr->checksum);
	udp_hdr->checksum = 0;

	checksum = calc_udp_checksum((char*)&pseudo_hdr, (char*)udp_hdr, remote_ip_hdr->total_len - (remote_ip_hdr->ver_ihl & 0xF) * 4);

#ifdef	DEBUG_UDP_RECV
	printfxy(0, 15, "Recv UDP packet: total_len=%hu length=%hu (%hu %hu), checksum=%04X, received checksum=%04X",
		remote_ip_hdr->total_len,
		remote_ip_hdr->total_len - (remote_ip_hdr->ver_ihl & 0xF) * 4 - sizeof(struct udp_hdr), 
		(remote_ip_hdr->ver_ihl & 0xF) * 4, sizeof(struct udp_hdr),
		checksum, received_checksum);
#endif

	// Handle properly "connected" UDP sockets - drop anything the arrived not from "connected" peer
	for (i = 0; i < MAX_SOCKETS; ++i)
	{
		if (IPPROTO_UDP == psockets[i].protocol &&
			psockets[i].addr.sin_port == udp_hdr->dest_port &&
			!((psockets[i].attrib & SOCK_ATTR_CONN) && remote_ip_hdr->src_ip != psockets[i].peer.sin_addr.s_addr))
		{
			copy_data_to_sock(psockets+i, udp_data, len, remote_ip_hdr, udp_hdr->src_port);
			// Wake-up waiting app.
			// BUGBUGBUG: this is buggy behavior - see tcp_parse(). Needs resolution
			psockets[i].status |= SOCK_STAT_HASDATA;
			files[psockets[i].fd].status |= FD_STAT_MAYREAD;
#if 0
			if (psockets[i].select_sleeping != NULL)
			{
				if (psockets[i].select_events_mask & SOCK_STAT_MAYRECV)
				{
					wake(&psockets[i].select_sleeping);
				}
			}
#endif	
			psockets[i].status |= SOCK_STAT_READY_RD;
			send_event_sel(&sel_q_hd, MAX_FILES * 2, psockets[i].fd);
			return;
		}
	}

#ifdef DEBUG_UDP
	serial_printf("%s(): no socket consumed packet, src_port=%u dest_port=%u\n", __func__, ntohs(udp_hdr->src_port), ntohs(udp_hdr->dest_port));
#endif
	// If DHCP handles it, it returns 0
	if (ntohs(udp_hdr->src_port) == DHCP_SERVER_PORT && ntohs(udp_hdr->dest_port) == DHCP_CLIENT_PORT)
	{
		if (parse_dhcp_client(net_if, udp_data, len) == 0)
			return;
	}
	else if (ntohs(udp_hdr->src_port) == DHCP_CLIENT_PORT && ntohs(udp_hdr->dest_port) == DHCP_SERVER_PORT)
	{
		if (parse_dhcp_server(net_if, udp_data, len) == 0)
			return;
	}

	// Otherwise, call other callbacks (?) - protocols encapsulated in UDP?
}

unsigned udp_send_to_netif(struct net_if *net_if,  const void *message, size_t length, const struct sockaddr_in *src_addr, const struct sockaddr_in *dest_addr, unsigned flags)
{
	struct	udp_hdr	*udp_hdr;
	struct	ip_hdr	*ip_hdr;
	struct	udp_pseudo_hdr	pseudo_hdr;
	unsigned char	*transmit_data;
	int	long_pkt = 0;
	unsigned char	transport_headers[sizeof(struct  ip_hdr) + sizeof(struct  udp_hdr)];
	unsigned char	*udp_data;
	extern byte	ip_tos;
	struct	arp_tbl_entry	*dest;
	unsigned char	*ip_addr = (unsigned char*)&dest_addr->sin_addr.s_addr;

	// Make sure that dest address is known to ARP
	dest = find_arp_entry((unsigned char*)&dest_addr->sin_addr.s_addr);
	if (NULL == dest)
	{
		arp_discover((unsigned char*)&dest_addr->sin_addr.s_addr);
		dest = find_arp_entry((unsigned char*)&dest_addr->sin_addr.s_addr);
		if (dest == NULL)
		{
			errno = EHOSTUNREACH;
			return	-1;
		}
	}
	
	// Send UDP packet
	if (length > ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct udp_hdr)))
	{
		long_pkt = 1;
		//length = ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct udp_hdr));
	}

	// Prepare IP header
	eth_get_send_packet(net_if, &transmit_data);
	if (!transmit_data)
	{
		errno = EBUSY;
		return	-1;
	}

	if (!long_pkt)
		ip_hdr = (struct ip_hdr*)transmit_data;
	else
		ip_hdr = (struct ip_hdr*)&transport_headers;
	prep_ip_hdr(ip_hdr, length + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), // data
		IPPROTO_UDP, &src_addr->sin_addr.s_addr, &dest_addr->sin_addr.s_addr);

	// Prepare UDP header
	udp_hdr = (struct udp_hdr*)(transmit_data + sizeof(struct ip_hdr));
	udp_hdr->src_port = src_addr->sin_port;
	udp_hdr->dest_port = dest_addr->sin_port;
	udp_hdr->length = sizeof(struct udp_hdr) + length;

	// Copy data
	if (length > 0 && !long_pkt)
	{
		udp_data = (char*)udp_hdr + sizeof(struct udp_hdr);
		memcpy(udp_data, message, length);
	}

	// Prepare pseudo-header
	memmove(&pseudo_hdr.src_ip, &ip_hdr->src_ip, 4);
	memmove(&pseudo_hdr.dest_ip, &ip_hdr->dest_ip, 4);
	pseudo_hdr.zero = 0;
	pseudo_hdr.protocol = ip_hdr->protocol;
	pseudo_hdr.total_length = udp_hdr->length;
	pseudo_hdr.total_length =  htons(pseudo_hdr.total_length);

	udp_hdr->length = htons(udp_hdr->length);
	udp_hdr->checksum = 0;
	udp_hdr->checksum = calc_udp_checksum((char*)&pseudo_hdr, (char*)udp_hdr, sizeof(struct udp_hdr) + length);
	if (udp_hdr->checksum == 0)
		udp_hdr->checksum = 0xFFFF;

#ifdef	DEBUG_UDP_SEND
	printfxy(0, 11, "Send checksum=%04X", udp_hdr->checksum);
#endif

	udp_hdr->checksum = htons(udp_hdr->checksum);

	if (dest_addr->sin_addr.s_addr == INADDR_BROADCAST)
	{
		int	rv;
		rv = eth_send_packet(net_if, ETH_ADDR_BROADCAST, htons(PROTO_TYPE_IP), length + sizeof(struct ip_hdr) + sizeof(struct udp_hdr));
		if (rv != 0)
			return	-1;
		return	length;
	}

	// Send the UDP packet
	if (!long_pkt)
		ip_send_packet(net_if, (unsigned char*)&ip_hdr->dest_ip, length + sizeof(struct ip_hdr) + sizeof(struct udp_hdr));
	else
	{
		size_t	headers_len = sizeof(struct ip_hdr) + sizeof(struct udp_hdr);
		unsigned payload_len = length;

		ip_send_packet_frag((unsigned char*)&ip_hdr->dest_ip, (unsigned char*)ip_hdr, headers_len, message, length);
	}

	return	length;	
}

unsigned udp_send(struct socket *psock, const void *message, size_t length, const struct sockaddr_in *dest_addr, unsigned flags)
{
	struct net_if	*net_if;
	
	// Get interface according to destination IP address, not source IP
	//net_if = get_net_interface((unsigned char*)&psock->addr.sin_addr.s_addr);
	net_if = get_net_interface((unsigned char*)&dest_addr->sin_addr.s_addr);
	if (NULL == net_if)
	{
		errno = ENETUNREACH;
		return	-1;		// Error: no appropriate interface
	}

	// If dest_addr is NULL --and-- the socket is connected, send to default address
	if (NULL == dest_addr)
	{
		if (psock->attrib & SOCK_ATTR_CONN)
			dest_addr = &psock->peer;
		else
			return	0;
	}

	return	udp_send_to_netif(net_if, message, length, &psock->addr, dest_addr, flags);
}

