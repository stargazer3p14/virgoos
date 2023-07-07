/*
 *	dhcp.c
 *
 *	DHCP protocol implementation for SeptemberOS. Client and server (client is the first stage)
 */

#include "config.h"
#include "sosdef.h"
#include "inet.h"
#include "socket.h"
#include "queues.h"

// Our UDP/IP implementations don't do any checks on whether source IP is 0.0.0.0 or dest IP is 255.255.255.255
// UDP sockets don't have to be bound to send data, but in our case it's still not true. udp_send() looks up appropriate network interface according to its bound IP address.
// This may be changed so that if the socket is not bound, then dest. address lookup is performed (the function get_net_interface() is suitable for that).
// We need to add a special handling for UDP/IP broadcasts (probably on IP level), so that they won't look up ARP table - it's useless and wrong
// We will set the interface's being configured IP address to 0.0.0.0, then create a UDP socket, bind to this address and do communication. (?) How can we dynamically configure several interfaces at the same time?
// Looks like we will have to make a really "creative use" of TCP/IP stack, as DHCP spec says. We will have to use the supplied net_if parameter particularly

// Builds DISCOVER message in local buffer and sends it
// We might have got send buffer and construct message there. However, this way it's simpler to use existing code, and DHCP messages are very rare - a couple of
// messages per days or months, so performance will not matter here
//
//

#define	MAX_DHCP_RETRIES	5

struct dhcp_timer_prm
{
	int	more_times;
	unsigned long	xid;
	timer_t	*timer;
	void *msg;
	size_t	msg_len;
	struct sockaddr_in	src_addr;
	struct sockaddr_in	dest_addr;
	struct net_if	*net_if;
};

extern dword	timer_counter;
extern byte	ip_tos;

// We need also to keep retransmit structures in a list for reference
static list_t	*retrans_list_head, *retrans_list_tail;

static long     cmp_retrans_struct(void *s1, void *s2)
{
	struct dhcp_msg	*m1 = s1, *m2 = s2;

	return	m1->xid - m2->xid;
}

// Periodic UDP timer just re-sends message, until removed
static  void    dhcp_timer_callback(void *prm)
{
	struct dhcp_timer_prm	*p = prm;
	unsigned char   *discover_msg = p->msg;
	list_t	*l;

	udp_send_to_netif(p->net_if, discover_msg, p->msg_len, &p->src_addr, &p->dest_addr , 0);
	if (!--p->more_times)
	{
		remove_timer(p->timer);
		free(p->timer);
		free(p->msg);
		free(p);

		// Remove from list
		if (l = list_find(retrans_list_head, p, cmp_list_entries))
		{
			list_delete(&retrans_list_head, &retrans_list_tail, l);
			free(l);
		}
	}
}

int	dhcp_discover(struct net_if *net_if)
{
	unsigned char	*discover_msg;
	struct dhcp_msg	*msg;
	uint32_t	*magic_cookie;
	struct dhcp_option	*first_option;
	struct dhcp_option	*option;
	unsigned char prm_req_list[] = {0x1, 0x1C, 0x2, 0x3, 0xF, 0x6, 0xC, 0x28, 0x29, 0x2A};
	int	rv;
	struct sockaddr_in	src_addr, dest_addr;
	timer_t	*discover_timer;	
	struct dhcp_timer_prm	*prm;
	list_t	*new_retrans_list;
	uint8_t	temp_tos;
	
	net_if->dhcp_state = DHCP_STATE_INIT;

	discover_msg = calloc(1, 1024);
	if (discover_msg == NULL)
		return	-1;

	magic_cookie = (uint32_t*)(discover_msg + sizeof(struct dhcp_msg));
	first_option = (struct dhcp_option*)((unsigned char*)magic_cookie + 4);

	msg = (struct dhcp_msg*)discover_msg;
	msg->op = DHCP_OP_REQUEST;
	msg->htype = HTYPE_ETHERNET;
	msg->hlen = ETH_ADDR_SIZE;
	msg->xid = htonl(timer_counter);
	memcpy(msg->chaddr, net_if->eth_dev->addr, ETH_ADDR_SIZE);
	*magic_cookie = htonl(DHCP_MAGIC_COOKIE);
	first_option->code = DHCP_OPT_MSG_TYPE;
	first_option->len = 1;
	first_option->data[0] = DHCPDISCOVER;

	// Additional options -- taken from Linux dump saved with sniffer
	option = (struct dhcp_option*)((unsigned char*)first_option + 3);
	option->code = DHCP_OPT_PRM_REQUEST_LIST;
	option->len = sizeof(prm_req_list);
	memcpy(option->data, prm_req_list, sizeof(prm_req_list));

	// Last option
	option = (struct dhcp_option*)((unsigned char*)option + 2 + option->len);
	option->code = 0xFF;
	src_addr.sin_addr.s_addr = INADDR_ANY;
	src_addr.sin_port = htons(DHCP_CLIENT_PORT);
	dest_addr.sin_addr.s_addr = INADDR_BROADCAST;
	dest_addr.sin_port = htons(DHCP_SERVER_PORT);

	// Differentiated Services Code point setting is not necessary; we keep them as a use of mechanism of setting IP TOS field for now
	// (probably has to be a parameter to send packet function)
	temp_tos = ip_tos;
	ip_tos = 0x10;			// Set DSCP to codepoint 4 (following Linux dump)
	rv = udp_send_to_netif(net_if, discover_msg, 300, &src_addr, &dest_addr , 0);
	ip_tos = temp_tos;

	// If rv >= 0 (success), then set timeout and wait for reply
	if (rv >= 0)
	{
		net_if->dhcp_state = DHCP_STATE_SELECTING;

		discover_timer = calloc(1, sizeof(timer_t));
		if (discover_timer == NULL)
			return	0;
		discover_timer->timeout = TICKS_PER_SEC * 4 + timer_counter % TICKS_PER_SEC;
		discover_timer->resolution = TICKS_PER_SEC;
		discover_timer->callback = dhcp_timer_callback;

		prm = malloc(sizeof(struct dhcp_timer_prm));
		if (prm == NULL)
		{
			free(discover_timer);
			free(discover_msg);
			return	0;
		}
		prm->more_times = MAX_DHCP_RETRIES;
		prm->timer = discover_timer;
		prm->msg = discover_msg;
		prm->msg_len = 300;
		prm->src_addr = src_addr;
		prm->dest_addr = dest_addr;
		prm->net_if = net_if;

		discover_timer->prm = prm;
		discover_timer->flags = TF_PERIODIC;

		new_retrans_list = malloc(sizeof(list_t));
		if (NULL == new_retrans_list)
		{
			free(discover_timer);
			free(discover_msg);
			free(prm);
			return	0;
		}
		new_retrans_list->datum = prm;
		list_insert(&retrans_list_head, &retrans_list_tail, new_retrans_list);

		install_timer(discover_timer);
		return	0;
	}

	return	-1;
}

int	dhcp_request(struct net_if *net_if, uint32_t xid)
{
	unsigned char	*request_msg;
	struct dhcp_msg	*msg;
	uint32_t	*magic_cookie;
	struct dhcp_option	*first_option;
	struct dhcp_option	*option;
	unsigned char prm_req_list[] = {0x1, 0x1C, 0x2, 0x3, 0xF, 0x6, 0xC, 0x28, 0x29, 0x2A};
	int	rv;
	struct sockaddr_in	src_addr, dest_addr;
	timer_t	*request_timer;	
	struct dhcp_timer_prm	*prm;
	list_t	*new_retrans_list;
	uint8_t	temp_tos;
	
	request_msg = calloc(1, 1024);
	if (request_msg == NULL)
		return	-1;

	magic_cookie = (uint32_t*)(request_msg + sizeof(struct dhcp_msg));
	first_option = (struct dhcp_option*)((unsigned char*)magic_cookie + 4);

	msg = (struct dhcp_msg*)request_msg;
	msg->op = DHCP_OP_REQUEST;
	msg->htype = HTYPE_ETHERNET;
	msg->hlen = ETH_ADDR_SIZE;
	msg->xid = xid;
	memcpy(msg->chaddr, net_if->eth_dev->addr, ETH_ADDR_SIZE);
	*magic_cookie = htonl(DHCP_MAGIC_COOKIE);
	first_option->code = DHCP_OPT_MSG_TYPE;
	first_option->len = 1;
	first_option->data[0] = DHCPREQUEST;

	// Server ID as recorded in DHCPOFFER
	option = (struct dhcp_option*)((unsigned char*)first_option + 3);
	option->code = DHCP_OPT_SERVER_ID;
	option->len = 4;
	*(uint32_t*)option->data = *(uint32_t*)net_if->dhcp_srv_id;
	option = (struct dhcp_option*)((unsigned char*)option + 2 + option->len);

	// Requested IP address a recorded in DHCPOFFER
	option->code = DHCP_OPT_REQ_IP_ADDR;
	option->len = 4;
	*(uint32_t*)option->data = *(uint32_t*)net_if->ip_addr;
	option = (struct dhcp_option*)((unsigned char*)option + 2 + option->len);

	// Additional options -- taken from Linux dump saved with sniffer
	option->code = DHCP_OPT_PRM_REQUEST_LIST;
	option->len = sizeof(prm_req_list);
	memcpy(option->data, prm_req_list, sizeof(prm_req_list));

	// Last option
	option = (struct dhcp_option*)((unsigned char*)option + 2 + option->len);
	option->code = 0xFF;
	src_addr.sin_addr.s_addr = INADDR_ANY;
	src_addr.sin_port = htons(DHCP_CLIENT_PORT);
	dest_addr.sin_addr.s_addr = INADDR_BROADCAST;
	dest_addr.sin_port = htons(DHCP_SERVER_PORT);
	temp_tos = ip_tos;
	ip_tos = 0x10;			// Set DSCP to codepoint 4 (following Linux dump)
	rv = udp_send_to_netif(net_if, request_msg, 300, &src_addr, &dest_addr , 0);
	ip_tos = temp_tos;

	// If rv >= 0 (success), then set timeout and wait for reply
	if (rv >= 0)
	{
		request_timer = calloc(1, sizeof(timer_t));
		if (request_timer == NULL)
			return	0;
		request_timer->timeout = TICKS_PER_SEC * 4 + timer_counter % TICKS_PER_SEC;
		request_timer->resolution = TICKS_PER_SEC;
		request_timer->callback = dhcp_timer_callback;

		prm = malloc(sizeof(struct dhcp_timer_prm));
		if (prm == NULL)
		{
			free(request_msg);
			free(request_timer);
			return	0;
		}
		prm->more_times = MAX_DHCP_RETRIES;
		prm->timer = request_timer;
		prm->msg = request_msg;
		prm->msg_len = 300;
		prm->src_addr = src_addr;
		prm->dest_addr = dest_addr;
		prm->net_if = net_if;

		request_timer->prm = prm;
		request_timer->flags = TF_PERIODIC;

		new_retrans_list = malloc(sizeof(list_t));
		if (NULL == new_retrans_list)
		{
			free(request_timer);
			free(request_msg);
			free(prm);
			return	0;
		}
		new_retrans_list->datum = prm;
		list_insert(&retrans_list_head, &retrans_list_tail, new_retrans_list);

		install_timer(request_timer);
		return	0;
	}

	return	-1;
}


int	parse_dhcp_client(struct net_if *net_if, char *pdata, size_t size)
{
	list_t	*l;
	struct dhcp_msg	*msg = (struct dhcp_msg*)pdata, *lmsg;
	struct dhcp_timer_prm	*prm;
	uint32_t	*magic_cookie;
	struct dhcp_option	*first_option, *option;
	size_t	n;
	int	send_dhcp_request = 0;
	int	send_dhcp_discover = 0;
	int	got_dhcp_ack = 0;
	int	kill_retrans = 0;
	int	i;

	if (msg->op != DHCP_OP_REPLY)
		return	-1;

	magic_cookie = (uint32_t*)(pdata + sizeof(struct dhcp_msg));
	first_option = (struct dhcp_option*)((unsigned char*)magic_cookie + 4);

	if (*magic_cookie != htonl(DHCP_MAGIC_COOKIE))
		return	-1;

	n = sizeof(struct dhcp_msg) + 4;
	for (l = retrans_list_head; l != NULL; l = l->next)
	{
		prm = l->datum;
		lmsg = prm->msg;
		if (msg->xid == lmsg->xid)
		{
			option = first_option;
			while (n < size)
			{
				switch (option->code)
				{
				default:
					break;
				case DHCP_OPT_MSG_TYPE:
					switch (net_if->dhcp_state)
					{
					default:
						break;
					case DHCP_OPT_PAD:
						++n;
						option = (struct dhcp_option*)(pdata + n);
						continue;
					case DHCP_OPT_END:
						n = size;
						break;
					case DHCP_STATE_SELECTING:
						if (option->data[0] == DHCPOFFER)
						{
							send_dhcp_request = 1;
							net_if->dhcp_state = DHCP_STATE_REQUESTING;
							*(uint32_t*)net_if->ip_addr = msg->yiaddr;
							kill_retrans = 1;
						}
						else
						{
							return	-1;
						}
						break;
					case DHCP_STATE_REQUESTING:
						if (option->data[0] == DHCPACK)
						{
							*(uint32_t*)net_if->ip_addr = msg->yiaddr;
							net_if->dhcp_state = DHCP_STATE_BOUND;
							got_dhcp_ack = 1;
							kill_retrans = 1;
						}
						else if (option->data[0] == DHCPNAK)
						{
							net_if->dhcp_state = DHCP_STATE_INIT;
							send_dhcp_discover = 1;
							kill_retrans = 1;
						}
						else
						{
							return	-1;
						}
						break;
					}
					break;
				case DHCP_OPT_SERVER_ID:
					*(uint32_t*)net_if->dhcp_srv_id = *(uint32_t*)option->data;
					break;
				case DHCP_OPT_IP_ADDR_LEASE_TIME:
					net_if->dhcp_lease_sec = ntohl(*(uint32_t*)option->data);
					break;
				case DHCP_OPT_SUBNET_MASK:
					*(uint32_t*)net_if->mask = *(uint32_t*)option->data;
					break;
				case DHCP_OPT_ROUTER:
					*(uint32_t*)net_if->router = *(uint32_t*)option->data;
					break;
				case DHCP_OPT_DNS_SERVER:
					for (i = 0; i < MAX_DNS_SERVERS && i * 4 < option->len; ++i)
						*(uint32_t*)net_if->dns_srv[i] = *(uint32_t*)option->data;
					break;
				case DHCP_OPT_DOMAIN_NAME:
					// Do we really need this?
					memcpy(net_if->domain_name, option->data, option->len);
					break;
				}
				n += 2 + option->len;
				option = (struct dhcp_option*)(pdata + n);
			} // while (n < option->len)

			if (send_dhcp_request)
			{
				dhcp_request(net_if, msg->xid);
			}
			else if (send_dhcp_discover)
			{
				dhcp_discover(net_if);
			}
			else if (got_dhcp_ack)
			{
			}

			if (kill_retrans)
			{
				remove_timer(prm->timer);
				free(prm->timer);
				free(prm->msg);
				free(prm);
				list_delete(&retrans_list_head, &retrans_list_tail, l);
			}
			if (net_if->dhcp_state == DHCP_STATE_BOUND)
			{
				serial_printf("%s(): configured net_if=%08X: ip = %d.%d.%d.%d, netmask = %d.%d.%d.%d, DHCP server= %d.%d.%d.%d DHCP lease time = %lu(s), router = %d.%d.%d.%d, DNS servers = %d.%d.%d.%d, %d.%d.%d.%d, %d.%d.%d.%d, %d.%d.%d.%d, my domain name = '%s'\n", __func__, net_if, net_if->ip_addr[0], net_if->ip_addr[1], net_if->ip_addr[2], net_if->ip_addr[3], net_if->mask[0], net_if->mask[1], net_if->mask[2], net_if->mask[3], net_if->dhcp_srv_id[0], net_if->dhcp_srv_id[1], net_if->dhcp_srv_id[2], net_if->dhcp_srv_id[3], net_if->dhcp_lease_sec, net_if->router[0], net_if->router[1], net_if->router[2], net_if->router[3], net_if->dns_srv[0][0], net_if->dns_srv[0][1], net_if->dns_srv[0][2], net_if->dns_srv[0][3], net_if->dns_srv[1][0], net_if->dns_srv[1][1], net_if->dns_srv[1][2], net_if->dns_srv[1][3], net_if->dns_srv[2][0], net_if->dns_srv[2][1], net_if->dns_srv[2][2], net_if->dns_srv[2][3], net_if->dns_srv[3][0], net_if->dns_srv[3][1], net_if->dns_srv[3][2], net_if->dns_srv[3][3], net_if->domain_name);
			}

			return	0;
		} // if (xid matches)
	} // Loop for all list entries

	return	-1;
}

int	parse_dhcp_server(struct net_if *net_if, char *pdata, size_t len)
{
	return	-1;
}

