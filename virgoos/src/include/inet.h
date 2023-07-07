/*
 *	Header file for internet protocols
 */

#ifndef	INET__H
 #define	INET__H

#include "config.h"
#include "sosdef.h"

#define	ETH_MTU	1500

#define	ETH_ADDR_SIZE	6
#define	IP_ADDR_SIZE	4

#define	MIN_ETH_PACKET_SIZE	64

#define	MAX_DNS_SERVERS	4
#define	MAX_DOMAIN_NAME	256

struct sockaddr_in;
struct socket;

typedef	unsigned long	in_addr_t;

enum	{ETH_DEV_TYPE_LOOP, ETH_DEV_TYPE_NIC};

// Special ethernet addresses
#define	ETH_ADDR_BROADCAST	"\xFF\xFF\xFF\xFF\xFF\xFF"

struct eth_tbl_entry
{
	unsigned char	addr[6];
};

// Ethernet device structure (convenience)
struct eth_device
{
	int	type;				// Device type (loop interface, NIC)
	unsigned char	addr[6];		// Ethernet address
	void	(*get_send_packet)(unsigned char **payload);		// For sockets/IP stack layer, to get ethernet buffer to store everything there.
	int	(*send_packet)(unsigned char *dest_addr, word protocol, unsigned size);
	dword	remote_ip_addr;			// Temporary holder for remote IP address during packet parsing
	unsigned char	remote_eth_addr[6];	// Temporary holder for remote ethernet address during packet parsing
	struct eth_tbl_entry	eth_tbl[ETH_TBL_SIZE];	// Hash table of ethernet addresses that were met on this LAN (switch table)

#define	ETH_DEV_SEND_LOCKED	0x1
	unsigned	flags;
} __attribute__ ((packed));

// Fields related to dynamic IP management (leasing etc.): whether the interface uses dynamic IP at all and if yes, in what DHCP state it is
enum {DHCP_STATE_INIT, DHCP_STATE_SELECTING, DHCP_STATE_REQUESTING, DHCP_STATE_INIT_REBOOT, DHCP_STATE_REBOOTING, DHCP_STATE_BOUND, DHCP_STATE_RENEWING, DHCP_STATE_REBINDING};

// Network interface - binds IP address to ethernet device.
// There may be several IP addresses per network card
struct net_if
{
	struct eth_device	*eth_dev;
	unsigned char	ip_addr[4];
	unsigned char	mask[4];
	int	dynamic_ip;
	unsigned	dhcp_state;
	unsigned char	dhcp_srv_id[4];
	uint32_t	dhcp_lease_sec;
	unsigned char	router[4];
	unsigned char	dns_srv[MAX_DNS_SERVERS][4];
	char	domain_name[MAX_DOMAIN_NAME];
} __attribute__ ((packed));


// Ethernet header
struct eth_frame_hdr
{
	unsigned char dest_addr[6];
	unsigned char src_addr[6];
	word	frame_size;				/* Frame size (802.3 frames) or protocol type (Ethernet-II frames) */
} __attribute__ ((packed));

//////////////////////////////////////////////
// These will move to higher protocol levels
//////////////////////////////////////////////

struct	llc_hdr
{
	unsigned char	dsap;
	unsigned char	ssap;
	unsigned char	control;
} __attribute__ ((packed));

struct	snap_hdr
{
	unsigned char	oui[3];
	word	type;
} __attribute__ ((packed));


#define	PROTO_TYPE_ARP	0x0806
#define	PROTO_TYPE_IP	0x0800

#define	HW_TYPE_ETHERNET	1

#define	ARP_OPER_REQUEST	1
#define	ARP_OPER_REPLY		2

// ARP definitions
struct arp_packet
{
	word	hw_type;
	word	proto_type;
	byte	hw_len;
	byte	proto_len;
	word	oper;
	unsigned char	sender_hw_addr[6];
	unsigned char	sender_proto_addr[4];
	unsigned char	target_hw_addr[6];
	unsigned char	target_proto_addr[4];
} __attribute__ ((packed));
	
struct	arp_tbl_entry
{
	unsigned char	ip_addr[4];
	unsigned char	eth_addr[6];
};

// Special IP addresses
#define	INADDR_ANY	0x00000000UL				// "Any" address (to bind on specific port/any address)
#define	INADDR_BROADCAST	0xFFFFFFFFUL		// Broadcast address
#define	INADDR_NONE	(unsigned long)-1			// Error address (ambiguous by definition, but it is required by some old functions)

// IP header's protocol fields
#define	IPPROTO_ICMP	1
#define	IPPROTO_TCP		6
#define	IPPROTO_UDP		17
#define	IPPROTO_IP		PROTO_TYPE_IP
// More to come...

// IP-related definitions
#define	MAX_IP_TOTAL_SIZE	0xFFFF
#define	DEF_IP_TTL		0xFF

#define	IP_FLAG_DONT_FRAG	0x4000
#define	IP_FLAG_MORE_FRAGS	0x2000

struct	ip_hdr
{
	byte	ver_ihl;		// Version and header length (in 32-bit words)
	byte	tos;			// TYPE of service
	word	total_len;		// Datagram's length (in octets)
	word	id;
	word	frag_offs;		// +flags
	byte	ttl;
	byte	protocol;
	word	hdr_checksum;
	dword	src_ip;
	dword	dest_ip;
	// Options - if low nibble of ver_ihl is more than offset here
} __attribute__ ((packed));


// ICMP types
#define	ICMP_TYPE_ECHOREPLY	0
#define	ICMP_TYPE_ECHOREQUEST	8
// More to come

struct	icmp_hdr
{
	byte	type;
	byte	code;
	word	hdr_checksum;
} __attribute__ ((packed));


struct	icmp_echo_msg
{
	struct	icmp_hdr	hdr;
	word	id;
	word	seq_num;
} __attribute__ ((packed));


// UDP
struct	udp_hdr
{
	word	src_port;
	word	dest_port;
	word	length;
	word	checksum;
} __attribute__ ((packed));

// For UDP checksums
struct	udp_pseudo_hdr
{
	dword	src_ip;
	dword	dest_ip;
	byte	zero;
	byte	protocol;
//	byte	protocol;
//	byte	zero;
	word	total_length;
} __attribute__ ((packed));


// TCP
struct	tcp_hdr
{
	word	src_port;
	word	dest_port;
	dword	seq_num;
	dword	ack_num;
	byte	dataoffs_ctl[2];
	word	window;
	word	checksum;
	word	urgent_ptr;
	// Padding may be here (according to data offset field)
} __attribute__ ((packed));

// TCP flags
#define	TCP_FLAG_MASK	0x3F

#define	TCP_FLAG_URGENT	0x20
#define	TCP_FLAG_ACK	0x10
#define	TCP_FLAG_PUSH	0x8
#define	TCP_FLAG_RST	0x4
#define	TCP_FLAG_SYN	0x2
#define	TCP_FLAG_FIN	0x1

// TCP options
#define	TCP_OPT_ENDOFLIST	0
#define	TCP_OPT_NOOP	1
#define	TCP_OPT_MSS		2
#define	TCP_OPT_WINSCALE	3
#define	TCP_OPT_SACKPERM	4
#define	TCP_OPT_SACK	5
#define	TCP_OPT_ALTCSUMREQ	6
#define	TCP_OPT_ALTCSUM	7

// TCP states
#define	TCP_STATE_CLOSED	0x0
#define	TCP_STATE_LISTEN	0x1
#define	TCP_STATE_SYNSENT	0x2
#define	TCP_STATE_SYNACKRECVED	0x4
#define	TCP_STATE_SYNRECVED	0x8
#define	TCP_STATE_SYNACKSENT	0x10
#define	TCP_STATE_CONNECTED	0x20
#define	TCP_STATE_FINRECVED	0x40
#define	TCP_STATE_FINSENT	0x80
#define	TCP_STATE_FINACKRECVED	0x100
#define	TCP_STATE_FINACKSENT	0x200
#define	TCP_STATE_TIMEWAIT	0x400
#define	TCP_STATE_RESET		0x800
#define	TCP_STATE_ABORTED	0x1000

// Aggregate values
#define	TCP_STATE_ESTABLISHED_MASK	(TCP_STATE_SYNSENT | TCP_STATE_SYNACKRECVED | TCP_STATE_SYNRECVED | TCP_STATE_SYNACKSENT | TCP_STATE_CONNECTED | \
	TCP_STATE_FINRECVED | TCP_STATE_FINSENT | TCP_STATE_FINACKRECVED | TCP_STATE_FINACKSENT)
#define	TCP_STATE_CONNECTED_MASK (TCP_STATE_SYNSENT | TCP_STATE_SYNACKRECVED | TCP_STATE_SYNRECVED | TCP_STATE_SYNACKSENT)
#define	TCP_STATE_FINISHED_MASK	(TCP_STATE_FINRECVED | TCP_STATE_FINSENT | TCP_STATE_FINACKRECVED | TCP_STATE_FINACKSENT)
#define	TCP_STATE_NO_RECV	(TCP_STATE_FINRECVED | TCP_STATE_RESET | TCP_STATE_ABORTED)

// DHCP (extended BOOTP) general message
struct dhcp_msg
{
	uint8_t	op;
// Specific requests are defined for options
#define	DHCP_OP_REQUEST	1
#define	DHCP_OP_REPLY	2
	uint8_t	htype;
// HType definitions
#define	HTYPE_ETHERNET	1
#define	HTYPE_IEEE_802	6
#define	HTYPE_ARCNET	7
#define	HTYPE_LOCALTALK	11
#define	HTYPE_LOCALNET	12
#define	HTYPE_SMDS	14
#define	HTYPE_FRAME_RELAY	15
#define	HTYPE_ATM	16
#define	HTYPE_HDLC	17
#define	HTYPE_FIBRE_CHANNEL	18
#define	HTYPE_ATM_2	19
#define	HTYPE_SERIAL_LINE	20
	uint8_t	hlen;
	uint8_t	hops;
	uint32_t	xid;
	uint16_t	secs;
	uint16_t	flags;
	uint32_t	ciaddr;
	uint32_t	yiaddr;
	uint32_t	siaddr;
	uint32_t	giaddr;
	uint8_t	chaddr[16];
	uint8_t	sname[64];
	uint8_t	file[128];
} __attribute__ ((packed));

// DHCP options (some of them are not quite optional)
struct dhcp_option
{
	uint8_t	code;
	uint8_t	len;
	uint8_t	data[1];			// Size according to 'len' field above, may be 0
} __attribute__ ((packed));

// DHCP option codes definitions
#define	DHCP_OPT_PAD	0
#define	DHCP_OPT_SUBNET_MASK	1
#define DHCP_OPT_TIME_OFFSET	2
#define DHCP_OPT_ROUTER	3
#define DHCP_OPT_TIME_SERVER	4
#define DHCP_OPT_IEN116_NAME_SERVER	5
#define DHCP_OPT_DNS_SERVER	6
#define DHCP_OPT_LOG_SERVER	7
#define DHCP_OPT_COOKIE_SERVER	8
#define DHCP_OPT_LPR_SERVER	9
#define DHCP_OPT_IMPRESS_SERVER	10
#define DHCP_OPT_RES_LOC_SERVER	11
#define DHCP_OPT_HOST_NAME	12
#define DHCP_OPT_BOOT_FILE_SIZE	13
#define DHCP_OPT_MERIT_DUMP_FILE	14
#define DHCP_OPT_DOMAIN_NAME	15
#define DHCP_OPT_SWAP_SERVER	16
#define DHCP_OPT_ROOT_PATH	17
#define DHCP_OPT_EXT_PATH	18
#define DHCP_OPT_IP_FWD_EN	19
#define DHCP_OPT_NON_LOCAL_SRCRT_EN	20
#define DHCP_OPT_POLICY_FILTER	21
#define DHCP_OPT_MAX_DGRAM_REASM_SIZE	22
#define DHCP_OPT_DEF_IP_TTL	23
#define DHCP_OPT_PATH_MTU_AGING_TIMEOUT	24
#define DHCP_OPT_PATH_MTU_PLATEAU_TABLE	25
#define DHCP_OPT_INTERFACE_MTU	26
#define DHCP_OPT_ALL_SUBNETS_ARE_LOCAL	27
#define DHCP_OPT_BROADCAST_ADDR	28
#define DHCP_OPT_PERF_MASK_DISCOVERY	29
#define DHCP_OPT_MASK_SUPPLIER	30
#define DHCP_OPT_PERF_ROUTER_DISCOVERY	31
#define DHCP_OPT_ROUTER_SOL_ADDRESS	32
#define DHCP_OPT_STATIC_ROUTE	33
#define DHCP_OPT_TRAILER_ENCAPS	34
#define DHCP_OPT_ARP_CACHE_TIMEOUT	35
#define DHCP_OPT_ETHER_ENCAPS	36
#define DHCP_OPT_DEF_TTL	37
#define DHCP_OPT_TCP_KEEPALIVE_INTERVAL	38
#define DHCP_OPT_TCP_KEEPALILVE_GARBAGE	39
#define DHCP_OPT_NET_INFO_SERVICE_DOMAIN	40
#define DHCP_OPT_NET_INFO_SERVERS	41
#define DHCP_OPT_NET_TIME_PROTO_SERVERS	42
#define DHCP_OPT_VENDOR_SPECIFIC	43
#define DHCP_OPT_NETBIOS_OVER_TCPIP_NAME_SERVERS	44
#define DHCP_OPT_NETBIOS_OVER_TCPIP_DGRAM_DISTRIB_SERVERS	45
#define DHCP_OPT_NETBIOS_OVER_TCPIP_NODE_TYPE	46
#define DHCP_OPT_NETBIOS_OVER_TCPIP_SCOPE	47
#define DHCP_OPT_X_WINDOWS_FONT_SERVERS	48
#define DHCP_OPT_X_WINDOWS_SYS_DISPLAY_MANAGER	49
#define DHCP_OPT_NET_INFO_SERVICE_PLUS_DOMAIN	64
#define DHCP_OPT_NET_INDO_SERVICE_PLUS_SERVERS	65
#define DHCP_OPT_MOBILE_IP_HOME_CLIENT	68
#define DHCP_OPT_SMTP_SERVERS	69
#define DHCP_OPT_POP3_SERVERS	70
#define DHCP_OPT_NNTP_SERVERS	71
#define DHCP_OPT_DEF_WWW_SERVERS	72
#define DHCP_OPT_DEF_FINGER_SERVERS	73
#define DHCP_OPT_DEF_IRC_SERVERS	74
#define DHCP_OPT_STREETTALK_SERVERS	75
#define DHCP_OPT_STREETTALK_DA_SERVERS	76
#define DHCP_OPT_REQ_IP_ADDR	50
#define DHCP_OPT_IP_ADDR_LEASE_TIME	51
#define DHCP_OPT_OVERLOAD	52
#define DHCP_OPT_MSG_TYPE	53
#define DHCP_OPT_SERVER_ID	54
#define DHCP_OPT_PRM_REQUEST_LIST	55
#define DHCP_OPT_MESSAGE	56
#define DHCP_OPT_MAX_MSG_SIZE	57
#define DHCP_OPT_RENEWAL_TIME_VAL	58
#define DHCP_OPT_REBINDING_TIME_VAL	59
#define DHCP_OPT_VENDOR_CLASS_ID	60
#define DHCP_OPT_CLIENT_ID	61
#define DHCP_OPT_TFTP_SERVER_NAME	62
#define DHCP_OPT_BOOTFILE_NAME	67

#define DHCP_OPT_END	255

// DHCP message types
#define	DHCPDISCOVER	1
#define	DHCPOFFER	2
#define	DHCPREQUEST	3
#define	DHCPDECLINE	4
#define	DHCPACK	5
#define	DHCPNAK	6
#define	DHCPRELEASE	7
#define	DHCPINFORM	8

// DHCP ports (UDP)
#define	DHCP_CLIENT_PORT	68
#define	DHCP_SERVER_PORT	67

#define	DHCP_MAGIC_COOKIE	0x63825363

// Functions prototypes
int	find_ip_addr(unsigned char *ip);
void	parse_tcp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr);
int	tcp_send_packet(struct socket *psock, const void *buf, size_t length, unsigned char tcp_flags);
unsigned tcp_send(struct socket *psock, const void *message, size_t length, unsigned flags);
int	tcp_connect(struct socket *psock, const struct sockaddr_in *address);
int	tcp_send_syn(struct socket *psock, const struct sockaddr_in *address, unsigned char tcp_flags);
int	tcp_send_fin(struct socket *psock);
int	tcp_send_ack(struct socket *psock);
int     tcp_send_rst(struct socket *psock);
void	parse_udp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr);
unsigned udp_send(struct socket *psock, const void *message, size_t length, const struct sockaddr_in *dest_addr, unsigned flags);
unsigned udp_send_to_netif(struct net_if *net_if,  const void *message, size_t length, const struct sockaddr_in *src_addr, const struct sockaddr_in *dest_addr, unsigned flags);
void	parse_icmp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr);
void	parse_ip(struct net_if *net_if, char *pdata);
void    parse_arp(struct net_if *net_if, char *pdata);
void	add_arp_entry(struct arp_tbl_entry *entry);
struct	arp_tbl_entry	*find_arp_entry(unsigned char *ip_addr);
int	arp_discover(char *addr);
void	update_arp_tbl(unsigned char *ip_addr, struct eth_frame_hdr *frame_hdr);
void	send_grat_arp(struct net_if *net_if);
int	ip_send_packet(struct net_if *net_if, unsigned char *ip_addr, unsigned size);
int	ip_send_packet_frag(unsigned char *ip_addr, const unsigned char *src, size_t headers_length, const unsigned char *payload, unsigned size);
void	init_ip(void);
//void	eth_send_packet(char *dest_addr, word protocol, unsigned char *data, unsigned size);
void	eth_parse_packet(struct net_if *this, void *pkt);
int	eth_send_packet(struct net_if *this, unsigned char *dest_addr, word protocol, unsigned size);
word	calc_ip_checksum(char *pdata, int len);
word	calc_udp_checksum(char *pseudo_hdr, char *pdata, unsigned len);
void	prep_ip_hdr(struct ip_hdr *ip_hdr, word packet_len, byte protocol, const dword *src_ip, const dword *dest_ip);
struct net_if	*get_net_interface(unsigned char *ip_addr);
void	add_eth_entry(struct eth_device *eth_dev, unsigned char *addr);
struct	eth_tbl_entry	*find_eth_entry(struct eth_device *eth_dev, unsigned char *addr);
void    eth_get_send_packet(struct net_if *this, unsigned char **payload);
int	parse_dhcp_client(struct net_if *net_if, char *pdata, size_t size);
int	parse_dhcp_server(struct net_if *net_if, char *pdata, size_t size);
int	dhcp_discover(struct net_if *net_if);
int	dhcp_request(struct net_if *net_if, uint32_t xid);
int	dhcp_get_lease(struct net_if *net_if);
int	dhcp_renew_lease(struct net_if *net_if);
int	dhcp_release_lease(struct net_if *net_if);



/////////////////////////////////////////////
// netdb
/////////////////////////////////////////////

struct hostent
{
	char	*h_name;                 /* Official name of host.  */
	char	**h_aliases;             /* Alias list.  */
	int	h_addrtype;               /* Host address type.  */
	int	h_length;                 /* Length of address.  */
	char	**h_addr_list;           /* List of addresses from name server.  */
#define        h_addr  h_addr_list[0] /* Address, for backward compatibility.*/
};


int	gethostname(char *name, size_t len);
struct hostent *gethostbyname(const char *name);

#endif // INET__H

