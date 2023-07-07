/*
 *	TCP implementation for September OS
 *
 *	TODO: support SO_LINGER option (which will allow graceful termination when close() is called on socket)
 *	TODO: allow "copy-saving" API (iTRON terminology) (the same for UDP) for clients - don't unconditionally copy data to socket buffer. LATER
 *	TODO: bug fixes!
 *
 *	TODO: limit amount of simultaneous retransmissions
 *	TODO: there's still some bug with removing timers (!)
 */
#include "inet.h"
#include "socket.h"
#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "timers.h"
#include "queues.h"

//#define DEBUG_TCP_STATES
//#define DEBUG_TCP_RECV
//#define DEBUG_TCP_SEND
//#define DEBUG_ACCEPT
//#define DEBUG_TCP_RST
//#define DEBUG_TCP_ACK
//#define DEBUG_SOCK_STATES
//#define DEBUG_TCP_RETRANSMIT

extern struct socket	*psockets;
//extern char	*transmit_data;
//extern char	ip_addr[4];
//extern char	eth_addr[6];
extern word	ip_id;
extern EVENTS_SEL_Q *sel_q_hd;
extern struct file	*files;

// We need also to keep retransmit structures in a list for reference
static list_t	*retrans_list_head, *retrans_list_tail;

struct tcp_retransmit_struct
{
	int	more_times;	
	timer_t	*ptimer;
	struct socket	*psock;
	unsigned char	data[SOCKBUF_LEN];
};


static long	cmp_retrans_struct(void *s1, void *s2)
{
	struct tcp_retransmit_struct *struct1 = s1, *struct2 = s2;
	struct tcp_hdr	*tcp_hdr1 = (struct tcp_hdr*)struct1->data + sizeof(struct ip_hdr);
	struct tcp_hdr	*tcp_hdr2 = (struct tcp_hdr*)struct2->data + sizeof(struct ip_hdr);

	return	!(struct1->psock == struct2->psock && tcp_hdr1->seq_num == tcp_hdr2->seq_num);
}

void	tcp_retransmit_timer_proc(void *arg)
{
	struct tcp_retransmit_struct	*retrans_struct = (struct tcp_retransmit_struct*)arg;
	struct ip_hdr	*ip_hdr = (struct ip_hdr*)retrans_struct->data;
	struct tcp_hdr	*tcp_hdr = (struct tcp_hdr*)((unsigned char*)ip_hdr + sizeof(struct ip_hdr));
	size_t	length = ntohs(ip_hdr->total_len);
	int	long_pkt = 0;
	size_t	headers_len;
	unsigned char	*buf;
	list_t	*l;
	struct net_if	*net_if;
	unsigned char	*transmit_data;

#ifdef DEBUG_TCP_RETRANSMIT
serial_printf("tcp_retransmit_timer_proc(): entered (length=%u more_times=%u seq_num=%08X:%08X timer=%08X)\n", length, retrans_struct->more_times,
	ntohl(tcp_hdr->seq_num), ntohl(tcp_hdr->seq_num) + length - sizeof(struct tcp_hdr), retrans_struct->ptimer);
#endif

	if (length > ETH_MTU)
		long_pkt = 1;
		
	// Send the TCP packet
	// Funny-munny, looks like retransmission never sent correct data
	if (!long_pkt)
	{
		net_if = get_net_interface((unsigned char*)&ip_hdr->dest_ip);
		if (!net_if)
			goto	del_this_timer;
		eth_get_send_packet(net_if, &transmit_data);
		// If can't get transmit data buffer, just return, we will
		// retransmit later.
		if (!transmit_data)
			return;
		memcpy(transmit_data, retrans_struct->data, length);
		ip_send_packet(net_if, (unsigned char*)&ip_hdr->dest_ip, length);
	}
	else
	{
		unsigned payload_len = length;
		headers_len = sizeof(struct ip_hdr) + sizeof(struct tcp_hdr);

		buf = retrans_struct->data + headers_len;
		ip_send_packet_frag((unsigned char*)&ip_hdr->dest_ip, (const unsigned char*)ip_hdr, headers_len, buf, length);
	}
	if (!--retrans_struct->more_times)
	{
#ifdef DEBUG_TCP_RETRANSMIT
serial_printf("removing timer %08X -- more_times = 0\n", retrans_struct->ptimer);
#endif
del_this_timer:
		remove_timer(retrans_struct->ptimer);
		free(retrans_struct->ptimer);
		free(retrans_struct);

		// Remove from list
		if (l = list_find(retrans_list_head, retrans_struct, cmp_list_entries))
		{
			list_delete(&retrans_list_head, &retrans_list_tail, l);
			free(l);
		}
	}
#ifdef DEBUG_TCP_RETRANSMIT
serial_printf("tcp_retransmit_timer_proc(): exitted\n");
#endif
}

void	kill_all_retransmit_timers(void)
{
	list_t  *l;

	for (l = retrans_list_head; l != NULL; l = l->next)
	{
		struct tcp_retransmit_struct	*retrans_struct = (struct tcp_retransmit_struct*)l->datum;
		timer_t	*retrans_timer = retrans_struct->ptimer;
#ifdef DEBUG_TCP_ACK
		// Below vars are needed only to print
		struct ip_hdr	*ip_hdr = (struct ip_hdr*)retrans_struct->data;	
		struct tcp_hdr	*tcp_hdr = (struct tcp_hdr*)(retrans_struct->data + sizeof(struct ip_hdr));
		size_t	payload_len = ntohs(ip_hdr->total_len) - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr));
		uint32_t	test_ack_num = ntohl(tcp_hdr->seq_num);
#endif

		// No need to care wrap-around
#ifdef DEBUG_TCP_ACK
serial_printf("%s(): removing retransmission timer for %08X:%08X\n", __func__, test_ack_num, test_ack_num + payload_len);
#endif
		remove_timer(retrans_timer);
		free(retrans_timer);
		free(retrans_struct);
		list_delete(&retrans_list_head, &retrans_list_tail, l);
		free(l);
	}
}

static void	copy_data_to_sock(struct socket *sock, char *data, size_t offs, ssize_t len, struct ip_hdr *remote_ip_hdr, word port)
{
	struct sockaddr_in	peer_addr;

	if (offs + len > sock->peer_win_size)
	{
		// TODO: Should this terminate/reset connection?
		return;
	}

	memcpy(sock->buf + offs, data, len);
}

/*
 *	Parse TCP and may be deliver to connected socket
 *
 *	(!) Called in IRQ context
 *	TODO: when sending or receiving a TCP packet with TCP_FLAG_RST set we need to notify waiting socket client (if any) on error
 */
void	parse_tcp(struct net_if *net_if, char *pdata, struct ip_hdr *remote_ip_hdr)
{
	int	i;
	word	port;
	struct	tcp_hdr	*tcp_hdr, *reply_tcp_hdr;
	char	*tcp_data;
	ssize_t	len;
	struct	udp_pseudo_hdr	pseudo_hdr;
	struct	ip_hdr *ip_hdr;
	struct	arp_tbl_entry *dest;
	unsigned ts;
	char	*tcp_opt;
	unsigned char	*transmit_data;
	
#ifdef	DEBUG_TCP_RECV
	static int	cnt;
#endif
	
	tcp_hdr = (struct tcp_hdr*)pdata;
	tcp_data = pdata + ((tcp_hdr->dataoffs_ctl[0] >> 4) << 2);
	len = remote_ip_hdr->total_len - sizeof(struct ip_hdr) - ((tcp_hdr->dataoffs_ctl[0] >> 4) << 2);

#ifdef	DEBUG_TCP_RECV
	serial_printf("%s(): entered\n", __func__);
	serial_printf("RECV TCP packet(%d): ts=%u, len=%d (remote total_len=%u, remote_ip_hdr=%08X)\r\n", ++cnt, ts, len, remote_ip_hdr->total_len, remote_ip_hdr);
	serial_printf("*******************************\n");
#endif
#if 0
	for (i = 0; i < 10; ++i)
	{
		// (!!!!!) - GCC has a weird bug - it mixes up sub-structures names! If the below two lines are unified, it mixes up '.peer.' with '.addr.' and prints everything pertaining to '.addr.' instead of '.peer.'
		// Probably nothing weird, that was a misconfigures/miscompiled GCC
		serial_printf("(%d) sock_idx=%d, protocol=%d, sock's addr=%s:%hu ", cnt, i, psockets[i].protocol, inet_ntoa(psockets[i].addr.sin_addr), psockets[i].addr.sin_port);
		serial_printf("sock's peer=%s:%hu tcp_state=%08X\r\n", inet_ntoa(psockets[i].peer.sin_addr), psockets[i].peer.sin_port, psockets[i].tcp_state);
	}
	serial_printf("*******************************\n");
#endif

	for (i = 0; i < MAX_SOCKETS; ++i)
	{
#ifdef	DEBUG_TCP_RECV
		if (IPPROTO_TCP == psockets[i].protocol)
		{
			serial_printf("(%d) sock_idx=%d, TCP, sock's port=%hu hdr's dest port=%hu tcp_state=%08X\r\n", cnt, i, psockets[i].addr.sin_port, tcp_hdr->dest_port, psockets[i].tcp_state);
		}
#endif		
		
		// Is this a packet for THIS socket (i)?
		if (IPPROTO_TCP == psockets[i].protocol && psockets[i].addr.sin_port == tcp_hdr->dest_port)
		{
#ifdef DEBUG_TCP_RECV
			serial_printf("%s(): sock=%d peer.sin_port=%04X tcp_hdr->src_port=%04X peer.sin_addr=%08X remote_ip_hdr->src_ip=%08X tcp_state=%08X\n",
			__func__, i, psockets[i].peer.sin_port, tcp_hdr->src_port, psockets[i].peer.sin_addr.s_addr, remote_ip_hdr->src_ip, psockets[i].tcp_state);
#endif
//			__asm__ __volatile__ ("rdtsc":"=a"(ts));

			// Handle RST flag. If received RST, we don't need to send it back
			if (tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_RST)
			{
				if ((TCP_STATE_CONNECTED | TCP_STATE_SYNSENT | TCP_STATE_FINSENT) & psockets[i].tcp_state)
				{
					// Test that SEQ number is valid! Don't allow spurious RST packets confuse us
					if (psockets[i].peer_seqnum != htonl(tcp_hdr->seq_num))
					{
#ifdef	DEBUG_TCP_RESET
						serial_printf("%s(): confusing RST\n", __func__);
#endif
						break;
					}
				}
				if (psockets[i].tcp_state & TCP_STATE_LISTEN)
					psockets[i].tcp_state = TCP_STATE_LISTEN;
				else
					psockets[i].tcp_state |= TCP_STATE_RESET;
#ifdef	DEBUG_TCP_RESET
				serial_printf("%s(): non-confusing RST\n", __func__);
#endif
				kill_all_retransmit_timers();
				break;
			}
			else if (TCP_STATE_CLOSED == psockets[i].tcp_state)
			{
				// TCP connection is in initial state - send back RST
				psockets[i].peer.sin_addr.s_addr = remote_ip_hdr->src_ip;
				psockets[i].peer.sin_port = tcp_hdr->src_port;
#ifdef DEBUG_TCP_RST
				serial_printf("Case for TCP RST is TCP_STATE_CLOSED\n");
#endif
				tcp_send_rst(&psockets[i]);
				kill_all_retransmit_timers();
				break;
			}
			else if (psockets[i].tcp_state != TCP_STATE_LISTEN && psockets[i].tcp_state != TCP_STATE_CONNECTED)
			{
				// Here we handle only peer's ISN and ISN+1. We don't need to check whether peer_seqnum doesn't exceed window size here.
				// Peer may send only packet with `peer_seqnum' or re-send packet with peer_isn.
				// We don't need here to check SN wrap-around also
				uint32_t	peer_seqnum;

				peer_seqnum = ntohl(tcp_hdr->seq_num);

#ifdef DEBUG_TCP_RECV
serial_printf("!TCP_STATE_LISTEN && !TCP_STATE_CONNECTED. peer_seqnum=%08X socket's peer_isn=%08X socket's peer_seqnum=%08X\n", peer_seqnum, psockets[i].peer_isn, psockets[i].peer_seqnum);
#endif
				if (psockets[i].peer_isn != 0)
					if (!(peer_seqnum == psockets[i].peer_isn || peer_seqnum == psockets[i].peer_seqnum))
						break;
#ifdef DEBUG_TCP_RECV
serial_printf("peer_seqnum is OK\n");
#endif
			}
			// If socket is NOT in listening state and peer's port or IP address doesn't match what appears in IP and TCP headers, then
			// it's not THIS socket's (i) packet
			else if (!(psockets[i].tcp_state & TCP_STATE_LISTEN) &&
				(psockets[i].peer.sin_port != tcp_hdr->src_port || psockets[i].peer.sin_addr.s_addr != remote_ip_hdr->src_ip))
					continue;

			if ((TCP_STATE_SYNSENT & psockets[i].tcp_state) && !(TCP_STATE_LISTEN & psockets[i].tcp_state))
			{
				// TCP connection is in connecting state - expecting to accept SYN/ACK packets
				// (!) This must be a non-listening socket
				// We consider the socket connected when we sent SYN, received SYN, received ACK and send ACK to SYN.
				// NOTE: although TCP spec mention "3-way handshake" (SYN-SYN+ACK-ACK), we actually allow response SYN and ACK
				// to be sent separately. Seems that it's not prohibited by TCP ("simultaneous" connection establishment by two clients).
				if ((tcp_hdr->dataoffs_ctl[1] & (TCP_FLAG_ACK | TCP_FLAG_SYN)) != 0)
				{
					if ((tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_ACK) == TCP_FLAG_ACK)
					{
						dword	ack_num;

						ack_num = htonl(tcp_hdr->ack_num);
						if (psockets[i].this_seqnum + 1 == ack_num)
						{
							psockets[i].tcp_state |= TCP_STATE_SYNACKRECVED;
						}
						else
						{
							// Connection failed (wrong ACK number). Don't send back TCP_FLAG_RST)
							psockets[i].tcp_state = TCP_STATE_CLOSED;
							psockets[i].peer.sin_addr.s_addr = remote_ip_hdr->src_ip;
							psockets[i].peer.sin_port = tcp_hdr->src_port;
							break;
						}
					}
					if ((tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_SYN) == TCP_FLAG_SYN)
					{
						// Send our ACK to SYN
						psockets[i].peer_seqnum = htonl(tcp_hdr->seq_num) + 1;
						psockets[i].this_seqnum = htonl(tcp_hdr->ack_num);
						psockets[i].this_acknowledged = htonl(tcp_hdr->ack_num);
						psockets[i].this_isn = htonl(tcp_hdr->ack_num);
						psockets[i].peer.sin_addr.s_addr = remote_ip_hdr->src_ip;
						psockets[i].peer.sin_port = tcp_hdr->src_port;
						psockets[i].peer_isn = ntohl(tcp_hdr->seq_num);
						psockets[i].peer_win_size = ntohs(tcp_hdr->window);
						tcp_send_ack(&psockets[i]);
					}
					if ((tcp_hdr->dataoffs_ctl[1] & (TCP_FLAG_ACK | TCP_FLAG_SYN)) == (TCP_FLAG_ACK | TCP_FLAG_SYN))
					{
						// If both SYN and ACK to our SYN were received, the socket is connected (from our POV)
						// From peer's POV, actually, this is not CONNECTED, but waiting for our ACK to be connected!
						psockets[i].tcp_state = TCP_STATE_CONNECTED;
						psockets[i].tcp_retransmit_interval = DEF_TCP_RETRANSMIT_INTERVAL;
						memcpy(&psockets[i].peer.sin_addr.s_addr, &remote_ip_hdr->src_ip, 4);
						psockets[i].peer.sin_port = tcp_hdr->src_port;
#ifdef	DEBUG_TCP_STATES
						serial_printf("     tcp_state <- TCP_STATE_CONNECTED: addr=%08X port=%04X   \r\n", (unsigned)psockets[i].peer.sin_addr.s_addr, (unsigned)psockets[i].peer.sin_port);
#endif
						psockets[i].status |= SOCK_STAT_MAYSEND;
						psockets[i].status |= SOCK_STAT_READY_WR;
						files[psockets[i].fd].status |= FD_STAT_MAYWRITE;
						// Write events (notified sockets) are the same as read events, but offset by MAX_FILES
						send_event_sel(&sel_q_hd, MAX_FILES * 2, psockets[i].fd + MAX_FILES);

						// Allocate acknowledgement bitmap
						psockets[i].ack_bitmap_size = psockets[i].peer_win_size / 8 + 1;
						psockets[i].ack_bitmap = calloc(1, psockets[i].ack_bitmap_size);
						if (psockets[i].ack_bitmap == NULL)
							tcp_send_rst(&psockets[i]);
						psockets[i].buf = malloc(psockets[i].peer_win_size);
						if (psockets[i].buf == NULL)
						{
							tcp_send_rst(&psockets[i]);
							free(psockets[i].ack_bitmap);
						}
					}
//					break;
				}
				else
				{
					// Packets that don't contain either SYN or ACK after initial SYN was sent and before we are connected, should be dropped.
					// TODO - Actually we may want to handle packets with TCP_FLAG_RST, to properly reset connection
					psockets[i].tcp_state = TCP_STATE_CLOSED;
					psockets[i].peer.sin_addr.s_addr = remote_ip_hdr->src_ip;
					psockets[i].peer.sin_port = tcp_hdr->src_port;
#ifdef DEBUG_TCP_RST
					serial_printf("Case for TCP RST is neither SYN nor ACK for socket in SYNSENT but not CONNECTED state\n");
#endif
					tcp_send_rst(&psockets[i]);
					kill_all_retransmit_timers();
					break;
				}
			}
			else if (TCP_STATE_LISTEN == psockets[i].tcp_state)
			{
#ifdef	DEBUG_TCP_STATES
					serial_printf("  socket %d is listening, peer=%s:%hu\n", i, inet_ntoa(psockets[i].peer.sin_addr), psockets[i].peer.sin_port); 
#endif
				// If queue of pending connections reached maximum, just don't respond and let the initiating
				// connection time out
				if (psockets[i].num_pending_conn == psockets[i].max_pending_conn)
				{
#ifdef	DEBUG_SOCK_STATES
					serial_printf("   max connection are already pending\n"); 
#endif
					return;
				}

				// TCP connection is in listening state - accept only SYN packets
				if ((tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_SYN) == TCP_FLAG_SYN)
				{
					psockets[i].tcp_state |= TCP_STATE_SYNRECVED;

#ifdef	DEBUG_TCP_STATES
					serial_printf("     tcp_state <- TCP_STATE_SYNRECVED   \r\n");
#endif

					// Send our SYN + SYNACK
					{
						struct sockaddr_in	remote_addr_in;

						remote_addr_in.sin_addr.s_addr = remote_ip_hdr->src_ip;
						remote_addr_in.sin_port = tcp_hdr->src_port;
						psockets[i].peer_seqnum = ntohl(tcp_hdr->seq_num) + 1;
						psockets[i].peer_isn = ntohl(tcp_hdr->seq_num);
						psockets[i].peer_win_size = ntohs(tcp_hdr->window);
#ifdef DEBUG_TCP_STATES
serial_printf("TCP_FLAG_SYN got on listening socket. peer_seqnum=%08X peer_isn=%08X peer_win_size=%hu\n", psockets[i].peer_seqnum, psockets[i].peer_isn, psockets[i].peer_win_size);
#endif

						// Send the TCP packet
						if (tcp_send_syn(&psockets[i], &remote_addr_in, TCP_FLAG_ACK) == 0)
							// Advance TCP state
							psockets[i].tcp_state |= TCP_STATE_SYNACKSENT;
					}

#ifdef	DEBUG_TCP_STATES
					serial_printf("     tcp_state <- TCP_STATE_SYNRECVED | TCP_STATE_SYNSENT | TCP_STATE_SYNACKSENT  (%08X) \r\n", psockets[i].tcp_state);
#endif
				}
				else
				{
					// Connection is reset. DROP
					// BUGBUGBUG -- this is a BUG: *listening* socket's state has nothing to do with wrong SYN sequence.
					// It should only drop the relevant pending connection
					// And may be send TCP_FLAG_RST (if not DDOS attack)
//#ifdef	DEBUG_TCP_STATES
//					serial_printf("     tcp_state <- TCP_STATE_CLOSED   \r\n");
//#endif
					//psockets[i].tcp_state = TCP_STATE_CLOSED;
				}
//				break;
			}
			else if ((TCP_STATE_LISTEN | TCP_STATE_SYNRECVED | TCP_STATE_SYNSENT | TCP_STATE_SYNACKSENT) == psockets[i].tcp_state)
			{
#ifdef	DEBUG_TCP_STATES
						serial_printf("     Received something   \r\n");
#endif			
				// Listening connection, received SYN, sent SYN+ACK. Expecting ACK to SYN, possibly with data
#ifdef DEBUG_TCP_STATES
serial_printf("dataoffs_ctl[1]=%08X TCP_FLAG_ACK=%08X\n", (unsigned)tcp_hdr->dataoffs_ctl[1], (unsigned)TCP_FLAG_ACK);
#endif
				if ((tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_ACK) == TCP_FLAG_ACK)
				{
					dword	ack_num;
					int	n;

					ack_num = htonl(tcp_hdr->ack_num);
#ifdef DEBUG_TCP_STATES
serial_printf("TCP_STATE_LISTEN | the whole list. ACK received. ack_num=%08X socket's this_seqnum=%08X\n", ack_num, psockets[i].this_seqnum);
#endif
					if (psockets[i].this_seqnum + 1 == ack_num)
					{
#ifdef	DEBUG_TCP_STATES
						serial_printf("     tcp_state <- TCP_STATE_CONNECTED   \r\n");
#endif
						//
						// Good, we think that we are connected. Now really it's new accept()ing socket
						// connected, this socket returns to listening state.
						// TODO:
						//		1) the current scheme allows only one connection to be handled at the same time. Any new
						//		connection will have to wait until negotiation on the previous finishes. This is OK for
						//		a simple device that accepts configuration connections, but too bad for a server of any kind.
						//
						//		2) The current scheme doesn't time out. Besides SYN flood attack vulnerability, if the
						//		connecting peer just fails, we don't receive any new connections since this one is in
						//		progress... too bad for anybody
						//
						//		3) Timeouts should be added for outgoing connections too.
						//
						//		4) Remove dead code
						//

						n = (psockets[i].first_pending_conn + psockets[i].num_pending_conn) % MAX_PENDING_CONN;
						//memmove(&psockets[i].pending_conn[n].peer.sin_addr.s_addr, &remote_ip_hdr->src_ip, 4);
						psockets[i].pending_conn[n].peer.sin_addr.s_addr = remote_ip_hdr->src_ip;
						//memmove(&psockets[i].pending_conn[n].peer.sin_port, &tcp_hdr->src_port, 2);
						psockets[i].pending_conn[n].peer.sin_port = tcp_hdr->src_port;
						psockets[i].pending_conn[n].peer.sin_family = AF_INET;
						psockets[i].pending_conn[n].peer_seqnum = htonl(tcp_hdr->seq_num);
						psockets[i].pending_conn[n].this_seqnum = psockets[i].this_seqnum + 1;
						psockets[i].pending_conn[n].peer_win_size = psockets[i].peer_win_size;
						++psockets[i].num_pending_conn;

						// Return the listening socket to initial state (no need any more, accept()ing socket
						// will go on)
						psockets[i].tcp_state = TCP_STATE_LISTEN;
						psockets[i].this_seqnum = 0;
						psockets[i].this_acknowledged = 0;
						psockets[i].peer_seqnum = 0;

						psockets[i].status |= SOCK_STAT_MAYACC;
						files[psockets[i].fd].status |= FD_STAT_MAYREAD;
						// Wake up task that sleeps on accept().
						psockets[i].status |= SOCK_STAT_READY_RD;
						send_event_sel(&sel_q_hd, MAX_FILES * 2, psockets[i].fd);

						// Data may follow. Or not?
						//if (len != 0)
						//	copy_data_to_sock(psockets+i, tcp_data, len, remote_ip_hdr, peer.sin_port);
					}
					else
					{
						// Connection is reset
						// BUGBUGBUG -- this is a BUG: *listening* socket's state has nothing to do with wrong SYN sequence.
						// It should only drop the relevant pending connection
						// And may be send TCP_FLAG_RST (if not DDOS attack)
//#ifdef	DEBUG_TCP_STATES
//						serial_printf(0, 23, "     tcp_state <- TCP_STATE_CLOSED   ");
//#endif
//						psockets[i].tcp_state = TCP_STATE_CLOSED;
					}
				}
				else
				{
					// It's a junk connection request, may be attack. Drop
					;
				}
//				break;
			}
			else if (TCP_STATE_CONNECTED == (TCP_STATE_CONNECTED & psockets[i].tcp_state) || psockets[i].tcp_state & TCP_STATE_FINISHED_MASK)
			{
				// It may happen that we think we are connected, but the peer didn't receive
				// our ACK to its SYN. 
				// NOTE: there may be several progressing connections on listen()ed socket at the same time

/*
				if ((tcp_hdr->dataoffs_ctl[1] & (TCP_FLAG_ACK | TCP_FLAG_SYN)) != 0)
				{
					psockets[i].tcp_state = TCP_STATE_SYNSENT;
					goto	handle_synack;
				}
*/
#ifdef	DEBUG_TCP_STATES
					serial_printf("  socket %d is connected, peer=%s:%hu\n", i, inet_ntoa(psockets[i].peer.sin_addr), psockets[i].peer.sin_port); 
#endif
				if (tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_ACK)
				{
					list_t  *l;
					uint32_t	ack_num;

					// We received an ACK
					ack_num = ntohl(tcp_hdr->ack_num);

#ifdef DEBUG_TCP_ACK
serial_printf("ACK: sock=%d received ack_num=%08X //// this_seqnum=%08X this_acknowledged=%08X this_isn=%08X\n", i, ack_num, psockets[i].this_seqnum, psockets[i].this_acknowledged, psockets[i].this_isn);
#endif
					// If ACK is for something not in this socket's window, skip it (it's probably an ACK for something already acknowledged?)
					if (ack_num - psockets[i].this_acknowledged > psockets[i].peer_win_size)
					{
#ifdef DEBUG_TCP_ACK
							serial_printf("ACK: for something already acknowledged\n");
#endif
							goto	acknowledged;
					}

					// ACK is legitimate, and it acknowledges something that is in retransmission queue
					// Look in rentransmit list, and remove all segments whose seq_num + tcp payload length is no greater than ACK num
					// We always assume that the data will include exacly IP header and TCP header (no options) because only regular
					// data segments are there
					for (l = retrans_list_head; l != NULL; l = l->next)
					{
						struct tcp_retransmit_struct	*retrans_struct = (struct tcp_retransmit_struct*)l->datum;
						timer_t	*retrans_timer = retrans_struct->ptimer;
						struct ip_hdr	*ip_hdr = (struct ip_hdr*)retrans_struct->data;	
						struct tcp_hdr	*tcp_hdr = (struct tcp_hdr*)(retrans_struct->data + sizeof(struct ip_hdr));
						size_t	payload_len = ntohs(ip_hdr->total_len) - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr));
						uint32_t	test_ack_num = ntohl(tcp_hdr->seq_num);

#ifdef DEBUG_TCP_ACK
serial_printf("ACK: testing against retransmission schedule for %08X:%08X (ack=%08X)\n", test_ack_num, test_ack_num + payload_len, ack_num);
#endif
						// No need to care wrap-around
						if (retrans_struct->psock == &psockets[i] && ack_num - (test_ack_num + payload_len) <= psockets[i].peer_win_size)
						{
#ifdef DEBUG_TCP_ACK
serial_printf("ACK: removing retransmission timer for %08X:%08X (ack=%08X)\n", test_ack_num, test_ack_num + payload_len, ack_num);
#endif
							remove_timer(retrans_timer);
							free(retrans_timer);
							free(retrans_struct);
							list_delete(&retrans_list_head, &retrans_list_tail, l);
							free(l);
						}

//						if (l == retrans_list_tail)
//							break;
					}
					psockets[i].this_acknowledged = ack_num;

					// If we already sent FIN, update our state
					if (psockets[i].tcp_state & TCP_STATE_FINSENT)
					{
						psockets[i].tcp_state |= TCP_STATE_FINACKRECVED;
					}
				} // if (ACK received)
acknowledged:
				// Consider receiving only if socket hasn't shut down read side and peer didn't shut down read side
				if (len && !(psockets[i].status & SOCK_STAT_SHUT_RD) && !(psockets[i].tcp_state & TCP_STATE_NO_RECV))
				{
					int	j, k;
					uint32_t	peer_seqnum, ack_num;
					uint32_t	src_skip = 0;				// Bytes to skip from tcp_data
#ifdef	DEBUG_TCP_RECV
					tcp_data[len] = '\0';
					serial_printf("Received: sock=%d len=%d, data=%s\r\n", i, len, tcp_data);
#endif

					// Mark received data in ack_bitmap in place appropriate to received TCP segment
					peer_seqnum = ntohl(tcp_hdr->seq_num);

					// Meanwhile the connection will fail when SN overflows, but for now it's a marginal acceptable bug
					if (peer_seqnum < psockets[i].peer_seqnum || peer_seqnum - psockets[i].peer_seqnum > psockets[i].this_win_size)
					{
						if (peer_seqnum + len < psockets[i].peer_seqnum || peer_seqnum > psockets[i].peer_seqnum && peer_seqnum - psockets[i].peer_seqnum > psockets[i].this_win_size)
						{
							serial_printf("Bad SN received: %lu, current peer_seqnum: %lu. Nothing to do with this, resetting connection\n", peer_seqnum, psockets[i].peer_seqnum);
							tcp_send_rst(&psockets[i]);
							break;
						}
						// Some segment is retransmitted (although already acknowleged, our ACK took longer to arrive than peer's timer, or our ACK was lost) along with another, which was not yet acknowledged.
						// Skip the already-acknowledged part
						src_skip = psockets[i].peer_seqnum - peer_seqnum;
					}

					copy_data_to_sock(psockets+i, tcp_data + src_skip, peer_seqnum + src_skip - psockets[i].peer_seqnum, len - src_skip, remote_ip_hdr, psockets[i].peer.sin_port);
#ifdef DEBUG_TCP_RECV
serial_printf("%s(): Tesing how copy_data_to_sock() performed: buf=<<%s>>, src_skip=%lu, copied to offs=%lu (%lu-%lu)\n", __func__,  psockets[i].buf, src_skip, peer_seqnum + src_skip - psockets[i].peer_seqnum, peer_seqnum, psockets[i].peer_seqnum);
#endif

					for (j = peer_seqnum - psockets[i].peer_seqnum; j < peer_seqnum + len - psockets[i].peer_seqnum && j < psockets[i].peer_win_size; ++j)
						psockets[i].ack_bitmap[j >> 3] |= 1 << (j & 7);

					// Now check what is the maximum contiguous number of bytes from the beginning of peer window that we have received
					// and may acknowledge and deliver to local socket. If there's anything, move "sliding window" forward by that received
					// number of bytes
					for (j = 0; j < psockets[i].peer_win_size; ++j)
						if (!(psockets[i].ack_bitmap[j >> 3] & 1 << (j & 7)))
							break;

#ifdef DEBUG_TCP_ACK
					serial_printf("Ack bitmap: ");
					{
						int	k;
						for (k = 0; k < (psockets[i].peer_win_size >> 3) + 1; ++k)
							serial_printf("%02X", psockets[i].ack_bitmap[k]);
						serial_printf("\n---------------------------------------\n");
					}
					serial_printf("We may acknowledge %d bytes\n", j);
#endif

					// If nothing contiguous was received that corresponds to the beginning of the peer's window, just return - may be
					// with the next segment we will have more luck
					if (j == 0)
						break;
//serial_printf("%s(): Tesing who stole our buffer[1]: buf=<<%s>>\n", __func__,  psockets[i].buf);

					// Move the sliding window (part I - move ack_bitmap. Part II- moving data in socket buffer will be done by recvfrom())
					memmove(psockets[i].ack_bitmap, psockets[i].ack_bitmap + (j >> 3), psockets[i].ack_bitmap_size - (j >> 3));
					for (k = 0; k < psockets[i].ack_bitmap_size - (j >> 3); ++k)
						psockets[i].ack_bitmap[k] >>= j & 7;
//serial_printf("%s(): Tesing who stole our buffer[2] [ack_bitmap=%08X, ack_bitmap_size=%u, j=%d, moved in ack_bitmap %lu bytes]: buf=<<%s>>\n", __func__, psockets[i].ack_bitmap, psockets[i].ack_bitmap_size, j, psockets[i].ack_bitmap_size - (j >> 3), psockets[i].buf);

					// Send our ACK to received data
					psockets[i].peer_seqnum = peer_seqnum + j;		// This must be updated before tcp_send_ack()
					tcp_send_ack(&psockets[i]);

					psockets[i].data_len = j;
					psockets[i].status |= SOCK_STAT_HASDATA;
					files[psockets[i].fd].status |= FD_STAT_MAYREAD;

#ifdef	DEBUG_TCP_RECV
					serial_printf("sock=%d sleeping=%08X\r\n", i, psockets[i].select_sleeping);
#endif

					psockets[i].status |= SOCK_STAT_READY_RD;
					send_event_sel(&sel_q_hd, MAX_FILES * 2, psockets[i].fd);

				} // if (len) - we received some data

				if ((tcp_hdr->dataoffs_ctl[1] & TCP_FLAG_FIN) == TCP_FLAG_FIN)
				{
					psockets[i].tcp_state |= TCP_STATE_FINRECVED;
					// The socket may not receive any more
					psockets[i].status &= ~SOCK_STAT_MAYRECV;

					// Send our FINACK
					psockets[i].peer_seqnum = ntohl(tcp_hdr->seq_num) + 1;

					// Send the TCP packet
					if (0 == tcp_send_ack(&psockets[i]))
					{
						// Advance TCP state
						psockets[i].tcp_state |= TCP_STATE_FINACKSENT;
					}
					else

					// NOTE: we don't automatically send back FIN. There are common frameworks (HTTP client/server exchange being a notable
					// example) where the communication explicitly includes ONE side shutting down connection, and the other side goes on.
					// In order to terminate its side of connection, the client explicitly must call shutdown()
					//
					// If we already had sent FIN, make the socket not connected
					if (psockets[i].tcp_state & TCP_STATE_FINSENT)
					{
						psockets[i].attrib &= ~SOCK_ATTR_CONN;
						psockets[i].tcp_state &= ~TCP_STATE_CONNECTED; 
					}

					// NOTE: TCP_STATE_CONNECTED actually indicates connection only to receive side.
					// Transmit side only checks for connection abort/reset and local shutdown
				}

				// Finally, if we reached finished state after receiving ACK/FIN+ACK, and socket is being closed, complete the closing
				if ((psockets[i].tcp_state & TCP_STATE_FINISHED_MASK) == TCP_STATE_FINISHED_MASK && psockets[i].status & SOCK_STAT_CLOSING)
				{
					close_socket(i + FIRST_SOCKET_DESCR);
				}

				break;
			} // if (TCP_STATE_CONNECTED)
		} // if (IPPROTO_TCP)
	} // for (all sockets) loop
#ifdef	DEBUG_TCP_RECV
	serial_printf("%s(): exitted\n", __func__);
#endif
}

/*
 *	Connect socket to remote
 */
int	tcp_connect(struct socket *psock, const struct sockaddr_in *address)
{
	int	rv = 0;
	fd_set	wfds;
	int	sock = psock - psockets;

	tcp_send_syn(psock, address, 0);	

//	if (psock->attrib & SOCK_ATTR_NONBLOCKING)
	if (files[psock->fd].status & FD_STAT_NONBLOCK)
	{
		errno = EAGAIN;
		return	-1;
	}
	// TODO: return and check for error conditions (while waking from nap())
	FD_ZERO(&wfds);
	FD_SET(psockets[sock].fd, &wfds);
	select(psockets[sock].fd+1, NULL, &wfds, NULL, NULL);

	return	0;
}

/*
 *	Generic routine to send TCP SYN
 *	tcp_flags parameter allows applying ACK
 */
int	tcp_send_syn(struct socket *psock, const struct sockaddr_in *address, unsigned char tcp_flags)
{
	word	port;
	struct	tcp_hdr	*tcp_hdr;
	char	*tcp_data;
	ssize_t	len;
	struct	udp_pseudo_hdr	pseudo_hdr;
	struct	ip_hdr *ip_hdr;
	struct	arp_tbl_entry	*dest;
	char	*tcp_opt;
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	size_t	headers_len;	
	timer_t	*retrans_timer;
	struct tcp_retransmit_struct	*retrans_struct;
	list_t	*new_retrans_list;

	net_if = get_net_interface((unsigned char*)&psock->addr.sin_addr.s_addr);
	if (NULL == net_if)
	{
		errno = EHOSTUNREACH;
		return	-1;		// Error: no appropriate interface
	}

	// Send our SYN. Randomize sequence number with the CPU time stamp counter
//	psockets[sock].this_seqnum = timer_counter;
//	__asm__ __volatile__ ("rdtsc" :"=a"(psock->this_seqnum));

	// Prepare IP header
	eth_get_send_packet(net_if, &transmit_data);
	if (!transmit_data)
	{
		errno = EBUSY;
		return	-1;
	}

	ip_hdr = (struct ip_hdr*)transmit_data;
	prep_ip_hdr(ip_hdr, 4 + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr), // No data, 4 bytes of TCP options
		IPPROTO_TCP, &psock->addr.sin_addr.s_addr, &address->sin_addr.s_addr);

	// Prepare TCP header
	tcp_hdr = (struct tcp_hdr*)(transmit_data + sizeof(struct ip_hdr));
	tcp_hdr->src_port = psock->addr.sin_port;
	tcp_hdr->dest_port = address->sin_port;
	tcp_hdr->seq_num = htonl(psock->this_seqnum);	// ISN
	psock->this_isn = tcp_hdr->seq_num;
	if (tcp_flags & TCP_FLAG_ACK)
		tcp_hdr->ack_num = htonl(psock->peer_seqnum);			// ACK to peer's ISN: connect, nothing to acknowledge yet

	tcp_hdr->dataoffs_ctl[0] = 0x60;		// header = 6 dwords
	tcp_hdr->dataoffs_ctl[1] = TCP_FLAG_MASK & (TCP_FLAG_SYN | tcp_flags);
	psock->this_win_size = SOCKBUF_LEN;
	tcp_hdr->window = htons(SOCKBUF_LEN);
	tcp_hdr->urgent_ptr = htons(0);
	tcp_hdr->checksum = 0;

	// Append MSS option
	tcp_opt = (char*)tcp_hdr + sizeof(struct tcp_hdr);
	tcp_opt[0] = TCP_OPT_MSS;
	tcp_opt[1] = 4;
	*(word*)(tcp_opt+2) = htons(ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr)));

	// Prepare pseudo-header
	memmove(&pseudo_hdr.src_ip, &ip_hdr->src_ip, 4);
	memmove(&pseudo_hdr.dest_ip, &ip_hdr->dest_ip, 4);
	pseudo_hdr.zero = 0;
	pseudo_hdr.protocol = ip_hdr->protocol;
	pseudo_hdr.total_length = htons(ip_hdr->total_len) - sizeof(struct ip_hdr);
	pseudo_hdr.total_length = htons(pseudo_hdr.total_length);

	tcp_hdr->checksum = calc_udp_checksum((char*)&pseudo_hdr, (char*)tcp_hdr, sizeof(struct tcp_hdr) + 4);
	if (tcp_hdr->checksum == 0)
		tcp_hdr->checksum = 0xFFFF;
	tcp_hdr->checksum = htons(tcp_hdr->checksum);

	// Send the TCP packet
	if (ip_send_packet(net_if, (unsigned char*)&address->sin_addr.s_addr, htons(ip_hdr->total_len)) != 0)
		// Advance TCP state
		psock->tcp_state |= TCP_STATE_SYNSENT;

// "Forget" to set  up retransmission
//goto	ret;

	// Copy data to retransmit structure
	retrans_struct = malloc(sizeof(struct tcp_retransmit_struct));
	if (NULL == retrans_struct)
		goto	ret;

	retrans_struct->more_times = MAX_TCP_RETRANSMISSIONS;
	retrans_struct->psock = psock;
	headers_len = sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + 8;
	memcpy(retrans_struct->data, ip_hdr, headers_len);

	// Allocate and install retransmission timer
	// NOTE: we install periodic timer, that is for the same segment retransmission will not be adjustable (at least, now for simplicity)
	retrans_timer = calloc(1, sizeof(timer_t));
	if (NULL == retrans_timer)
	{
		free(retrans_struct);
		goto	ret;
	}

	retrans_struct->ptimer = retrans_timer;				// Needed so that common callback will be able to remove and free timer

	retrans_timer->timeout = psock->tcp_retransmit_interval;
	retrans_timer->resolution = TICKS_PER_SEC;
	retrans_timer->flags = TF_PERIODIC;
	retrans_timer->callback = tcp_retransmit_timer_proc; 
	retrans_timer->prm = retrans_struct;

	new_retrans_list = malloc(sizeof(list_t));
	if (NULL == new_retrans_list)
	{
		free(retrans_timer);
		free(retrans_struct);
		goto	ret;
	}
	new_retrans_list->datum = retrans_struct;
	list_insert(&retrans_list_head, &retrans_list_tail, new_retrans_list);
	install_timer(retrans_timer);

ret:
	return	0;
}

/*
 *	Send data to remote peer
 */
unsigned tcp_send(struct socket *psock, const void *message, size_t length, unsigned flags)
{
	struct	tcp_hdr	*tcp_hdr;
	struct	ip_hdr	*ip_hdr;
	char	*tcp_data;
	struct	udp_pseudo_hdr	pseudo_hdr;
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	unsigned	rv;


	rv = tcp_send_packet(psock, message, length, TCP_FLAG_ACK | TCP_FLAG_PUSH);
	return	rv;


}

/*
 *	Send FIN - start graceful connection termination
 */
int	tcp_send_fin(struct socket *psock)
{
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	struct	tcp_hdr	*tcp_hdr;
	struct	ip_hdr	*ip_hdr;
	struct	udp_pseudo_hdr	pseudo_hdr;
	int	rv = -1;
	
	if ((rv = tcp_send_packet(psock, NULL, 0, TCP_FLAG_FIN | TCP_FLAG_ACK)) >= 0)
	{
                // Advance TCP state
                psock->tcp_state |= TCP_STATE_FINSENT;
		rv = 0;
	}

	return	rv;

}


/*
 *	Send RST - signal the other side to abort the connection 
 */
int	tcp_send_rst(struct socket *psock)
{
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	struct	tcp_hdr	*tcp_hdr;
	struct	ip_hdr	*ip_hdr;
	struct	udp_pseudo_hdr	pseudo_hdr;
	int	rv = -1;

	if (tcp_send_packet(psock, NULL, 0, TCP_FLAG_RST | TCP_FLAG_ACK) >= 0)
		rv = 0;

#ifdef	DEBUG_TCP_RST
	serial_printf("%s(): sock_idx=%d, tcp_state=%08X rv=%d\n", __func__, psock - psockets, psock->tcp_state, rv);
#endif
	// Advance TCP state. Listen sockets will return to LISTEN state from anything. Other (connected) sockets will immediately return to initial (CLOSED) state
	// TODO: decide whether we need to keep an indication that socket was reset (in order to return ECONNRESET on any attempt to transfer data)
	if (psock->tcp_state & TCP_STATE_LISTEN)
		psock->tcp_state = TCP_STATE_LISTEN;
	else
		psock->tcp_state |= TCP_STATE_ABORTED;

	return	rv;
}


/*
 *	Send empty ACK - useful for connection establishment and termination and also when nothing to send within ACK timeframe
 */
int	tcp_send_ack(struct socket *psock)
{
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	struct	tcp_hdr	*tcp_hdr;
	struct	ip_hdr	*ip_hdr;
	struct	udp_pseudo_hdr	pseudo_hdr;
	int	rv = -1;

	if (tcp_send_packet(psock, NULL, 0, TCP_FLAG_ACK) >= 0)
                // Advance TCP state
		rv = 0;

	return	rv;


}


/*
 *	Generic TCP send packet (data segment) procedure. We need it in order to remove duplicate sending code
 *
 *	NOTE: returns length sent (may be 0!) for success, -1 for failure. This changed at some time, watch for callers to this to call correctly
 */
int	tcp_send_packet(struct socket *psock, const void *buf, size_t length, unsigned char tcp_flags)
{
	unsigned char	*transmit_data;
	struct net_if	*net_if;
	struct tcp_hdr	*tcp_hdr;
	struct ip_hdr	*ip_hdr;
	struct udp_pseudo_hdr	pseudo_hdr;
	unsigned char	*tcp_data;
	int	long_pkt = 0;
	unsigned char	transport_headers[sizeof(struct  ip_hdr) + sizeof(struct  tcp_hdr)];
	size_t	headers_len;	
	timer_t	*retrans_timer;
	struct tcp_retransmit_struct	*retrans_struct;
	list_t	*new_retrans_list;
	size_t	allowed_send;

	if (NULL == buf)
		length = 0;

	net_if = get_net_interface((unsigned char*)&psock->addr.sin_addr.s_addr);
	if (NULL == net_if)
	{
		errno = ENODEV;
		return	-1;		// Error: no appropriate interface
	}

	// If send would exceed peer's window size (not enought unacknowledged bytes left), send up to maximum.
	if (psock->tcp_state & TCP_STATE_CONNECTED)
	{ 
		allowed_send = psock->this_acknowledged + psock->peer_win_size - psock->this_seqnum;
#ifdef DEBUG_TCP_SEND
serial_printf("%s(): this_seqnum=%08X this_acknowledged=%08X length=%d allowed_send=%d\n", __func__, psock->this_seqnum, psock->this_acknowledged, length, allowed_send);
#endif
		if (allowed_send < length)
			length = allowed_send;
	}

	// Send TCP packet.
	// Only general packet send function needs to care about long IP datagrams - TCP control packets are much shorter
	if (length > ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr)))
	{
		//length = ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr));
		long_pkt = 1;
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
	prep_ip_hdr(ip_hdr, length + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr), // data
		IPPROTO_TCP, &psock->addr.sin_addr.s_addr, &psock->peer.sin_addr.s_addr);

	// Prepare TCP header
	tcp_hdr = (struct tcp_hdr*)((unsigned char*)ip_hdr + sizeof(struct ip_hdr));
	tcp_hdr->src_port = psock->addr.sin_port;
	tcp_hdr->dest_port = psock->peer.sin_port;

	tcp_hdr->seq_num = htonl(psock->this_seqnum);	// SN
	psock->this_seqnum += length;

	if (tcp_flags & TCP_FLAG_ACK)
		tcp_hdr->ack_num = htonl(psock->peer_seqnum);	// ACK to peer's SN
	tcp_hdr->dataoffs_ctl[0] = 0x50;		// header = 5 dwords
	tcp_hdr->dataoffs_ctl[1] = TCP_FLAG_MASK & tcp_flags;
	tcp_hdr->window = htons(SOCKBUF_LEN);
	tcp_hdr->urgent_ptr = htons(0);
	tcp_hdr->checksum = 0;

	// Copy data
	if (length > 0 && !long_pkt)
	{
		tcp_data = (char*)tcp_hdr + ((tcp_hdr->dataoffs_ctl[0] >> 4) << 2);
		memcpy(tcp_data, buf, length);
	}

	// Prepare pseudo-header
	memmove(&pseudo_hdr.src_ip, &ip_hdr->src_ip, 4);
	memmove(&pseudo_hdr.dest_ip, &ip_hdr->dest_ip, 4);
	pseudo_hdr.zero = 0;
	pseudo_hdr.protocol = ip_hdr->protocol;
	pseudo_hdr.total_length = ntohs(ip_hdr->total_len) - sizeof(struct ip_hdr);
	pseudo_hdr.total_length = htons(pseudo_hdr.total_length);

	tcp_hdr->checksum = calc_udp_checksum((char*)&pseudo_hdr, (char*)tcp_hdr, sizeof(struct tcp_hdr) + length);
	if (tcp_hdr->checksum == 0)
		tcp_hdr->checksum = 0xFFFF;
	tcp_hdr->checksum = htons(tcp_hdr->checksum);

#ifdef	DEBUG_TCP_SEND
	serial_printf("SND TCP seg: port=%04X, checksum=%04X\r\n", tcp_hdr->dest_port, htons(tcp_hdr->checksum)); 
#endif
	// Send the TCP packet
	if (!long_pkt)
		ip_send_packet(net_if, (unsigned char*)&ip_hdr->dest_ip, ntohs(ip_hdr->total_len));
	else
	{
		headers_len = sizeof(struct ip_hdr) + sizeof(struct tcp_hdr);
		unsigned payload_len = length;

		ip_send_packet_frag((unsigned char*)&ip_hdr->dest_ip, (unsigned char*)ip_hdr, headers_len, buf, length);
	}

	// ACKs without data are not retransmitted
	if (tcp_flags & TCP_FLAG_ACK && length == 0)
		goto	ret;

// "Forget" to set  up retransmission
//goto	ret;

	// Copy data to retransmit structure
	retrans_struct = malloc(sizeof(struct tcp_retransmit_struct));
	if (NULL == retrans_struct)
		goto	ret;
	retrans_struct->more_times = MAX_TCP_RETRANSMISSIONS;
	retrans_struct->psock = psock;
	headers_len = sizeof(struct ip_hdr) + sizeof(struct tcp_hdr);
	memcpy(retrans_struct->data, ip_hdr, headers_len);
	memcpy(retrans_struct->data + headers_len, buf, length);

	// Allocate and install retransmission timer
	// NOTE: we install periodic timer, that is for the same segment retransmission will not be adjustable (at least, now for simplicity)
	retrans_timer = calloc(1, sizeof(timer_t));
	if (NULL == retrans_timer)
	{
		free(retrans_struct);
		goto	ret;
	}

	retrans_struct->ptimer = retrans_timer;				// Needed so that common callback will be able to remove and free timer

#ifdef DEBUG_TCP_RETRANSMIT
serial_printf("%s(): setting timer with timeout=%ld\n", __func__, psock->tcp_retransmit_interval);
#endif
	retrans_timer->timeout = psock->tcp_retransmit_interval;
	retrans_timer->resolution = TICKS_PER_SEC;
	retrans_timer->flags = TF_PERIODIC;
	retrans_timer->callback = tcp_retransmit_timer_proc; 
	retrans_timer->prm = retrans_struct;

	new_retrans_list = malloc(sizeof(list_t));
	if (NULL == new_retrans_list)
	{
		free(retrans_timer);
		free(retrans_struct);
		goto	ret;
	}
	new_retrans_list->datum = retrans_struct;
	list_insert(&retrans_list_head, &retrans_list_tail, new_retrans_list);
	install_timer(retrans_timer);

ret:
	return	length;
}

