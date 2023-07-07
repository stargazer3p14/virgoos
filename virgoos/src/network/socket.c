/*
 *	Implementation of sockets interface for September OS
 *
 *	Limitation: we don't allow to change dynamically send and receive buffers' sizes, but allow to retrieve them.
 *
 * 	TODO: Implement raw IP sockets
 * 	TODO: implement missing functions (sendmsg() / recvmsg(), poll())
 * 	TODO: test, in particular select() and poll()
 */

#include "socket.h"
#include "errno.h"
#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "io.h"

// Debug switch definitions
//#define	DEBUG_UDP_SEND
//#define	DEBUG_UDP_RECV
//#define	DEBUG_ARP
//#define	DEBUG_CHECKSUM
//#define	DEBUG_TCP_STATES
//#define	DEBUG_TCP_RECV
//#define	DEBUG_TCP_SEND
//#define	DEBUG_ACCEPT
//#define	DEBUG_SOCKETS_INIT

// Protocol = 0 means that socket is not allocated
//static struct socket	sockets[MAX_SOCKETS];
struct socket	*psockets;

// Pointer to transmit data buffer. Size = 1500 (Ethernet MTU), including all headers
//extern char	*transmit_data;
//extern char	ip_addr[4];
//extern char	eth_addr[6];
extern word	ip_id;

extern dword	timer_counter;

extern int	arp_replied;

extern	TASK_Q	*running_task;
extern	TASK_Q	*task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];

extern struct file     *files;

int	init_sockets(void)
{
	//psockets = sockets;
	psockets = calloc(1, sizeof(struct socket) * MAX_SOCKETS);
#ifdef DEBUG_SOCKETS_INIT
	serial_printf("%s(): psockets=%08X\n", __func__, psockets);
#endif
	return	1;
}

/*
 *	Send ping request
 *
 *	Later...
 */
int	ping(char *ip_addr)
{
	struct	ip_hdr	*ip_hdr;
	struct	icmp_hdr	*icmp_hdr;
}


static int	valid_domain(int domain)
{
	if (AF_INET == domain)
		return	1;

	return	0;
}

static int	valid_type(int type)
{
	return	(SOCK_STREAM == type || SOCK_DGRAM == type || SOCK_RAW == type);
}

static int	good_protocol(int type, int protocol)
{
	return	(SOCK_STREAM == type && IPPROTO_TCP == protocol ||
				SOCK_DGRAM == type && IPPROTO_UDP == protocol ||
				SOCK_RAW == type && IPPROTO_IP == protocol);
}

static int	assign_protocol(int type)
{
	if (SOCK_STREAM == type)
		return	IPPROTO_TCP;
	if (SOCK_DGRAM == type)
		return	IPPROTO_UDP;
	if (SOCK_RAW == type)
		return	IPPROTO_IP;
}

static int	find_avail_socket(void)
{
	int	i;

	for (i = 0; i < MAX_SOCKETS; ++i)
		if (0 == psockets[i].protocol)
			return	i;

	return	-1;
}

static int	check_bound(int sock, int protocol, struct sockaddr_in *addr)
{
	int	i;

	for (i = 0; i < MAX_SOCKETS; ++i)
		if (psockets[i].protocol != 0 && psockets[i].protocol == protocol && 
			psockets[i].bound != 0 && psockets[i].addr.sin_port == addr->sin_port &&
			(psockets[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr ||
			 INADDR_ANY == psockets[i].addr.sin_addr.s_addr) && i != sock)
			return	1;

	return	0;
}

// Check if there is a listening socket on requested address
static int	check_listen(int sock, int protocol, struct sockaddr_in *addr)
{
	int	i;

	for (i = 0; i < MAX_SOCKETS; ++i)
	{
		if (psockets[i].protocol != 0 && psockets[i].protocol == protocol && 
			psockets[i].bound != 0 && psockets[i].addr.sin_port == addr->sin_port &&
			(psockets[i].attrib & SOCK_ATTR_LISTEN) != 0 &&
			(psockets[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr ||
			 INADDR_ANY == psockets[i].addr.sin_addr.s_addr) && i != sock)
				return	1;
	}

	return	0;
}

/*
 *	Create and initialize socket structure
 */
int open_socket(int domain, int type, int protocol)
{
	int	sock;

	if (!valid_domain(domain))
	{
		errno = EINVAL;
		return	-1;
	}
	if (!valid_type(type))
	{
		errno = EINVAL;
		return	-1;
	}
	if (0 == protocol)
	{
		protocol = assign_protocol(type);
	}
	else
	{
		if (!good_protocol(type, protocol))
		{
			errno = EINVAL;
			return	-1;
		}
	}

	sock = find_avail_socket();
	if (-1 == sock)
		errno = ENOMEM;					// MAX_SOCKETS exhausted
	else
	{
		// Init socket
		psockets[sock].protocol = protocol;
		psockets[sock].sock_type = type;
		psockets[sock].data_len = 0;
		psockets[sock].attrib = 0;
		memset(&psockets[sock].recv_timeout, 0, sizeof(struct timeval));
		psockets[sock].bound = 0;
		psockets[sock].status = 0;
		if (IPPROTO_TCP == protocol)
		{
			// TCP-specific init: TCP states
			psockets[sock].tcp_state = TCP_STATE_CLOSED;	
			psockets[sock].first_pending_conn = 0;
			psockets[sock].num_pending_conn = 0;
			
			// Bind to INADDR_ANY:0 (possible for non-listening sockets, we will bind to appropriate IP:first available socket upon connect())
			psockets[sock].addr.sin_addr.s_addr = INADDR_ANY;
			psockets[sock].addr.sin_port = 0;
		}
		else if (IPPROTO_UDP == protocol)
		{
			// UDP-specific init
			// bind to INADDR_ANY:0 (possible for UDP sockets, we will bind to appropriate IP:first available socket upon send())
			psockets[sock].addr.sin_addr.s_addr = INADDR_ANY;
			psockets[sock].addr.sin_port = 0;
			psockets[sock].status |= SOCK_STAT_MAYSEND;

			// UDP sockets are available for immediate send, so allocate immediately buffer of size SOCKBUF_LEN
			psockets[sock].buf = malloc(SOCKBUF_LEN);
			if (psockets[sock].buf == NULL)
			{
				errno = ENOMEM;
				return	-1;
			}
		}
		//memset(&psockets[sock].peer, 0, sizeof(struct sockaddr_in)); 
		errno = 0;
		sock += FIRST_SOCKET_DESCR;
	};

	return	sock;
}


/*
 *	Close socket - will probably be needed to multiplex with file descriptors
 */
int	close_socket(int sock)
{
	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}
	if (0 == psockets[sock].protocol)
	{
		errno = EBADF;
		return	-1;
	}

	// An application may request close() on a connected TCP socket without calling shutdown() before; we will do it on its behalf in such a case
	if (SOCK_STREAM == psockets[sock].sock_type /* TCP socket */ &&
		psockets[sock].tcp_state & TCP_STATE_ESTABLISHED_MASK /* socket is in somewhat-established state */ &&
		(psockets[sock].tcp_state & TCP_STATE_FINISHED_MASK) != TCP_STATE_FINISHED_MASK /* socket is in finished state already */)
			shutdown(sock + FIRST_SOCKET_DESCR, SHUT_RDWR);

	// (!) Important handling: an application may request TCP socket close() after just sending shutdown(); if we close socket immediately,
	// we will not be able to correctly handle peer's FIN request.
	if (SOCK_STREAM == psockets[sock].sock_type /* TCP socket */ && (psockets[sock].tcp_state & TCP_STATE_FINISHED_MASK) != TCP_STATE_FINISHED_MASK)
	{
		psockets[sock].status |= SOCK_STAT_CLOSING;
		return	0;
	}

	errno = 0;
	if (psockets[sock].ack_bitmap != NULL)
	{
		free(psockets[sock].ack_bitmap);
		psockets[sock].ack_bitmap = NULL;
	}
	if (psockets[sock].buf != NULL)
	{
		free(psockets[sock].buf);
		psockets[sock].buf = NULL;
	}
	memset(&psockets[sock], 0, sizeof(struct  socket));
	return	0;
}


/*
 *	Bind socket to an address
 */
int bind(int sock, const struct sockaddr *addr, socklen_t addr_len)
{
	struct sockaddr_in	*paddr;
	int	if_idx;
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

//	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_FILES)
	{
		errno = EINVAL;
		return	-1;
	}
	if (0 == psockets[sock].protocol)
	{
		errno = EBADF;
		return	-1;
	}
	if (addr_len < sizeof(struct sockaddr_in))
	{
		errno = EINVAL;
		return	-1;
	}
	paddr = (struct sockaddr_in*)addr;
	if (check_bound(sock, psockets[sock].protocol, (struct sockaddr_in*)addr))
	{
		errno = EADDRINUSE;
		return	-1;
	}
	if ((if_idx = find_ip_addr((unsigned char*)&paddr->sin_addr)) < 0)
	{
		errno = EADDRNOTAVAIL;
		return	-1;
	}

	memcpy(&psockets[sock].addr, addr, sizeof(struct sockaddr_in));
	psockets[sock].bound = 1;
	if (psockets[sock].protocol == IPPROTO_UDP)
		psockets[sock].status |= SOCK_STAT_MAYRECV;
	errno = 0;

	return	0;
}


/*
 *	Put socket in listening mode
 *
 *	listen() is a strange beast. It applies only to TCP sockets, it requires to formally bind() to an address (being the only
 *	type of sockets that requires explicit bind), any yet it logically is NOT bound to any particular address, by definition!
 *	In other words, listening sockets don't participate in communication in any way.
 *
 *	listen() interface is awkward, but it's too late to redefine it. Too much code is already written using sockets as they are
 */
int listen(int sock, int backlog)
{
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}
	if (psockets[sock].protocol != IPPROTO_TCP)
	{
		errno = EINVAL;
		return	-1;
	}
	if (!psockets[sock].bound)
	{
		// Socket that tries to listen() must be bound
		errno = EADDRNOTAVAIL;
		return	-1;
	}
	if (psockets[sock].tcp_state != TCP_STATE_CLOSED)
	{
		// Socket that tries to listen() must be in initial ("closed") TCP state
		errno = EADDRINUSE;
		return	-1;
	}
	psockets[sock].attrib |= SOCK_ATTR_LISTEN;
	psockets[sock].tcp_state = TCP_STATE_LISTEN;
	psockets[sock].tcp_retransmit_interval = DEF_TCP_RETRANSMIT_INTERVAL;

#ifdef	DEBUG_TCP_STATES
	serial_printf("     sock_idx=%d tcp_state <- TCP_STATE_LISTEN\n", sock);
#endif
	psockets[sock].max_pending_conn = backlog;
	errno = 0;
	serial_printf("%s(): sock_idx=%d, protocol=%d, sock's addr=%s:%hu sock's peer=%s:%hu tcp_state=%08X attrib=%08X status=%08X\n", __func__, sock, psockets[sock].protocol, inet_ntoa(psockets[sock].addr.sin_addr), psockets[sock].addr.sin_port, inet_ntoa(psockets[sock].peer.sin_addr), psockets[sock].peer.sin_port, psockets[sock].tcp_state, psockets[sock].attrib, psockets[sock].status);
	return	0;
}

/*
 *	recvfrom() always returns something when something is available, even less than buffer's length
 *	If supplied buffer is big enough, a packet is copied
 *	If it is not big enough, for SOCK_STREAM sockets up to buffer length is copied to the buffer and
 *	the rest remains in socket's buffer
 *	For SOCK_DGRAM and SOCK_RAW sockets up to buffer length is copied to the buffer and the rest
 *	is dropped
 */
ssize_t recvfrom(int sock, void *buffer, size_t length,
             int flags, struct sockaddr *address, socklen_t *address_len)
{
	ssize_t	len;
	ssize_t	data_len;
	fd_set	rfds;
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

//	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}

	if (psockets[sock].data_len > 0)
	{
recv_data:
		// TODO: disable network interrupt while copying
		// TODO: use circular queue for socket buffers (it will avoid constant copying)
		if (SOCK_STREAM == psockets[sock].sock_type)
		{
			if (psockets[sock].data_len < length)
				length = psockets[sock].data_len;
			memcpy(buffer, psockets[sock].buf, length);
			if (address != NULL)
				memcpy(address, &psockets[sock].peer, sizeof(struct sockaddr_in));
			// Move peer's "sliding window" part II - move data in socket buffer
			memmove(psockets[sock].buf, psockets[sock].buf + length, psockets[sock].peer_win_size - length);
			psockets[sock].data_len -= length;
			if (psockets[sock].data_len == 0)
			{
				psockets[sock].status &= ~SOCK_STAT_HASDATA;
				files[psockets[sock].fd].status &= ~FD_STAT_MAYREAD;
			}
			// TODO: enable network interrupt
			return	length;
		}
		else
		{
			// dgram socket, copy one datagram.
			memcpy(&len, psockets[sock].buf, sizeof(len));
			if (address != NULL)
				memcpy(address, psockets[sock].buf+sizeof(len), sizeof(struct sockaddr_in));
			if (len < length)
				length = len;
			memcpy(buffer, psockets[sock].buf+sizeof(struct sockaddr_in)+sizeof(len), length);
			memmove(psockets[sock].buf, psockets[sock].buf+sizeof(len)+sizeof(struct sockaddr_in)+len,
				SOCKBUF_LEN-(sizeof(len)+sizeof(struct sockaddr_in)+len));
			psockets[sock].data_len -= len + sizeof(len) + sizeof(struct sockaddr_in);
			if (psockets[sock].data_len == 0)
			{
				psockets[sock].status &= ~SOCK_STAT_HASDATA;
				files[psockets[sock].fd].status &= ~FD_STAT_MAYREAD;
			}
			return	length;
		}
	}
	else if (psockets[sock].tcp_state & TCP_STATE_NO_RECV || psockets[sock].status & SOCK_STAT_SHUT_RD)
	{
		// Connection errors checking
		if (psockets[sock].tcp_state & TCP_STATE_RESET)
		{
			// Connection reset by peer (RST flag received)
			errno = ECONNRESET;
			return	-1;
		}
		if (psockets[sock].tcp_state & TCP_STATE_ABORTED)
		{
			// Connection was aborted by local stack, due to hopeless  miscommunication
			errno = ECONNABORTED;
			return	-1;
		}
		if (psockets[sock].status & SOCK_STAT_SHUT_RD || psockets[sock].tcp_state & TCP_STATE_FINRECVED)
		{
			// Connection send-side was terminated with shutdown()
			errno = ESHUTDOWN;
			return	-1;
		}
	}
	else
	{
//		if (psockets[sock].attrib & SOCK_ATTR_NONBLOCKING)
		if (files[psockets[sock].fd].status & FD_STAT_NONBLOCK)
		{
			errno = EAGAIN;
			return	-1;
		}
		FD_ZERO(&rfds);
		FD_SET(psockets[sock].fd, &rfds);
		select(psockets[sock].fd+1, &rfds, NULL, NULL, NULL);

		goto recv_data;
	}
}


ssize_t recv(int sock, void *buffer, size_t length, int flags)
{
	return	recvfrom(sock, buffer, length, flags, NULL, NULL);
}


ssize_t send(int sock, const void *message, size_t length, int flags)
{
	return	sendto(sock, message, length, flags, NULL, 0);
}


ssize_t sendto(int sock, const void *message, size_t length, int flags,
             const struct sockaddr *dest_addr, socklen_t dest_len)
{
	size_t	count;
	struct sock_file	*sf;
	size_t	size_to_send, sent, total_sent;

	sf = files[sock].file_struct;
	sock = sf->sock;

//	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}

	if (SOCK_STREAM == psockets[sock].sock_type)
	{
		size_t	max_size_to_send = ETH_MTU - (sizeof(struct ip_hdr) + sizeof(struct tcp_hdr));

		// Connection errors checking
		if (psockets[sock].tcp_state & TCP_STATE_RESET)
		{
			// Connection reset by peer (RST flag received)
			errno = ECONNRESET;
			return	-1;
		}
		if (psockets[sock].tcp_state & TCP_STATE_ABORTED)
		{
			// Connection was aborted by local stack, due to hopeless  miscommunication
			errno = ECONNABORTED;
			return	-1;
		}
		if (psockets[sock].status & SOCK_STAT_SHUT_WR)
		{
			// Connection send-side was terminated with shutdown()
			errno = ESHUTDOWN;
			return	-1;
		}

		// Send larger than Ethernet MTU amounts in up to MTU chunks (we will test sending IP fragmented frames later)
		for (total_sent = 0, count = length; count > 0; )
		{
			size_to_send = count > max_size_to_send ? max_size_to_send : count;

			// TCP sockets sendto() ignores "to" address
			sent = tcp_send(psockets + sock, (unsigned char*)message + total_sent, size_to_send, flags);
			serial_printf("%s(): tcp_send() returned %u\n", sent);
			if (sent < 0)
				break;
			if (sent == 0)
			{
				serial_printf("%s(): Sleeping 1 (s) in order to allow acknowledgedment to arrive (probably we run on s/w emulator)\n", __func__);
				sleep(1);
			}
			count -= sent;
			total_sent += sent;
		}
	}
	else
	{
		// TODO: will UDP allow sending datagrams > Ethernet MTU?
		length = udp_send(psockets + sock, message, length, (struct sockaddr_in*)dest_addr, flags);
	}

	return	length;
}


/*
 * Connect to a remote server
 * Send SYN TCP packet to peer and set TCP state
 */
int connect(int sock, const struct sockaddr *address, socklen_t address_len)
{
	fd_set	wfds;
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

//	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}
	if (psockets[sock].protocol != IPPROTO_TCP)
	{
		errno = EINVAL;
		return	-1;
	}
	if (address_len < sizeof(struct sockaddr_in))
	{
		errno = EINVAL;
		return	-1;
	}
	
    // Create/remove association for UDP sockets
	if (IPPROTO_UDP == psockets[sock].protocol)
	{
		memcpy(&psockets[sock].peer, address, sizeof(struct sockaddr_in));
		if (psockets[sock].addr.sin_family == AF_INET)
			psockets[sock].attrib |= SOCK_ATTR_CONN;
		else if (psockets[sock].addr.sin_family == AF_UNSPEC)
			psockets[sock].attrib &= ~SOCK_ATTR_CONN;
		else
		{
			errno = EAFNOSUPPORT;
			return	-1;
		}
		errno = 0;
		return	0;
	}

	// TCP sockets that are trying to connect() must be of AF_INET and must be not connected
	if (psockets[sock].addr.sin_family != AF_INET)
	{
		errno = EAFNOSUPPORT;
		return	-1;
	}
	if (psockets[sock].attrib & SOCK_ATTR_CONN)
	{
		errno = EISCONN;
		return	-1;
	}

	psockets[sock].tcp_retransmit_interval = DEF_TCP_RETRANSMIT_INTERVAL;
	return	tcp_connect(psockets+sock, (struct sockaddr_in*)address);
}

/*
 *	Accept an incomming socket.
 *	Check that the socket is listening and put the caller to sleep. Will be waking up by the incomming
 *	connection
 */
int accept(int sock, struct sockaddr *address, socklen_t *address_len)
{
	int	new_sock;
	int	n;
	fd_set	rfds;
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

//	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}
	if (psockets[sock].protocol != IPPROTO_TCP)
	{
		errno = EINVAL;
		return	-1;
	}
	if (psockets[sock].tcp_state != TCP_STATE_LISTEN)
	{
		errno = EINVAL;
		return	-1;
	}
	if (address != NULL && *address_len < sizeof(struct sockaddr_in))
	{
	  	errno = EINVAL;
		return	-1;
	}	

	while (0 == psockets[sock].num_pending_conn)
	{
//		if (psockets[sock].attrib & SOCK_ATTR_NONBLOCKING)
		if (files[psockets[sock].fd].status & FD_STAT_NONBLOCK)
		{
			errno = EAGAIN;
			return	-1;
		}
		FD_ZERO(&rfds);
		FD_SET(psockets[sock].fd, &rfds);
		select(psockets[sock].fd+1, &rfds, NULL, NULL, NULL);
		// Handle any error that may occur and that woke us up
	}
	
#ifdef	DEBUG_ACCEPT
	serial_printf("%s(): after nap(), num_pending_conn=%d\n", __func__, psockets[sock].num_pending_conn);
#endif

/*
	new_sock = find_avail_socket();
	if (-1 == new_sock)
		errno = ENOMEM;					// MAX_SOCKETS exhausted
*/
	n = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (n < 0)
	{
		// errno is already set
		new_sock = -1;
	}
	else
	{
		struct sock_file	*sf = files[n].file_struct;
		new_sock = sf->sock;

		// Init socket
		psockets[new_sock].protocol = IPPROTO_TCP;
		psockets[new_sock].sock_type = SOCK_STREAM;
		psockets[new_sock].data_len = 0;
		psockets[new_sock].attrib = 0;
		memset(&psockets[new_sock].recv_timeout, 0, sizeof(struct timeval));
		psockets[new_sock].bound = 0;
		psockets[new_sock].tcp_state = TCP_STATE_CONNECTED;
		psockets[new_sock].tcp_retransmit_interval = DEF_TCP_RETRANSMIT_INTERVAL;
		psockets[new_sock].num_pending_conn = 0;
		psockets[new_sock].first_pending_conn = 0;
		psockets[new_sock].status |= SOCK_STAT_MAYSEND | SOCK_STAT_MAYRECV;
		files[n].status |= FD_STAT_MAYWRITE;

		// Peek the first connection of list of pending connections.
		// TODO: protect this from preemption by network IRQ!
#ifdef	DEBUG_ACCEPT
	serial_printf("%s(): first_pending_conn=%08X num_pending_conn=%08X new_sock=%d protocol=%d\r\n", __func__,
		psockets[sock].first_pending_conn, psockets[sock].num_pending_conn, new_sock, psockets[new_sock].protocol);
#endif

		memcpy(&psockets[new_sock].addr, &psockets[sock].addr, sizeof(struct sockaddr_in));
		memcpy(&psockets[new_sock].peer, &psockets[sock].pending_conn[psockets[sock].first_pending_conn].peer, sizeof(struct sockaddr_in));
		psockets[new_sock].this_seqnum = psockets[sock].pending_conn[psockets[sock].first_pending_conn].this_seqnum;
		psockets[new_sock].this_acknowledged = psockets[sock].pending_conn[psockets[sock].first_pending_conn].this_seqnum;
		psockets[new_sock].peer_seqnum = psockets[sock].pending_conn[psockets[sock].first_pending_conn].peer_seqnum;
		psockets[new_sock].peer_isn = psockets[new_sock].peer_seqnum - 1;
		psockets[new_sock].this_isn = psockets[new_sock].this_seqnum;
		psockets[new_sock].peer_win_size = psockets[sock].pending_conn[psockets[sock].first_pending_conn].peer_win_size;
		psockets[new_sock].this_win_size = SOCKBUF_LEN;

		// Allocate acknowledgement bitmap
		psockets[new_sock].ack_bitmap_size = psockets[new_sock].peer_win_size / 8 + 1;
		psockets[new_sock].ack_bitmap = calloc(1, psockets[new_sock].ack_bitmap_size);
		if (psockets[new_sock].ack_bitmap == NULL)
		{
err_ret:
			errno = ENOMEM;
			tcp_send_rst(&psockets[new_sock]);
			close(n);
			return	-1;
		}
		// Allocate socket's buffer
		psockets[new_sock].buf = malloc(psockets[new_sock].peer_win_size);
		if (psockets[new_sock].buf == NULL)
		{
			free(psockets[new_sock].ack_bitmap);
			goto	err_ret;
		}

		// Remove pending connection
		psockets[sock].first_pending_conn = (psockets[sock].first_pending_conn + 1) % MAX_PENDING_CONN;
		if (!--psockets[sock].num_pending_conn)
		{
			psockets[sock].status &= ~SOCK_STAT_MAYACC;
			files[psockets[sock].fd].status &= ~FD_STAT_MAYREAD;
		}

		// Record peer (client's) address for the caller
		memcpy(address, &psockets[new_sock].peer, sizeof(struct sockaddr_in));
		*address_len = sizeof(struct sockaddr_in);
		errno = 0;
		new_sock += FIRST_SOCKET_DESCR;

		new_sock = n;
	};

	return	new_sock;
}


/*
 *	Shutdown a connected socket
 */
int shutdown(int sock, int how)
{
	struct sock_file	*sf;
	struct file	*f;

	f = &files[sock];
	sf = files[sock].file_struct;
	sock = sf->sock;

	serial_printf("%s(): entered\r\n", __func__);

	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EBADF;
		return	-1;
	}
	if (!(SOCK_DGRAM == psockets[sock].sock_type || SOCK_STREAM == psockets[sock].sock_type))
	{
		errno = EINVAL;
		return	-1;
	}
	if (SOCK_DGRAM == psockets[sock].sock_type && !(psockets[sock].attrib & SOCK_ATTR_CONN) || 
		SOCK_STREAM == psockets[sock].sock_type && !(psockets[sock].tcp_state & TCP_STATE_CONNECTED || psockets[sock].tcp_state & TCP_STATE_FINSENT))
	{
		// We return ENOTCONN for TCP sockets that already sent SYN in order to avoid SYN flood
		errno = ENOTCONN;
		return	-1;
	}

	serial_printf("%s(): switch(how)\r\n", __func__);
	switch (how)
	{
	default:
		errno = EINVAL;			// Invalid "how"
		return	-1;
	case SHUT_RD:
	case SHUT_RDWR:
		psockets[sock].status |= SOCK_STAT_SHUT_RD;	
		psockets[sock].status &= ~SOCK_STAT_MAYRECV;
		if (SHUT_RD == how)
			break;
		// Fall through. 
	case SHUT_WR:
		psockets[sock].status |= SOCK_STAT_SHUT_WR;
		psockets[sock].status &= ~SOCK_STAT_MAYSEND;
		f->status &= ~ FD_STAT_MAYWRITE;
		
		if (SOCK_STREAM == psockets[sock].sock_type)
		{
			// We need not allow FIN flood
			if (!(psockets[sock].tcp_state & TCP_STATE_FINSENT))
				tcp_send_fin(psockets+sock);
		}
		else	// SOCK_DGRAM
			psockets[sock].attrib &= ~SOCK_ATTR_CONN;
		break;
	}

	return	0;
}


/*
 *	Set socket options
 */
int setsockopt(int sock, int level, int option_name, const void *option_value, socklen_t option_len)
{
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}

	if (SOL_SOCKET == level)
	{
		switch (option_name)
		{
		case SO_DEBUG:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			psockets[sock].bool_opts &= ~SO_DEBUG_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) & SO_DEBUG_MASK;
			break;
		case SO_BROADCAST:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}			psockets[sock].bool_opts &= ~SO_BROADCAST_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) << 1 & SO_BROADCAST_MASK;
			break;
		case SO_REUSEADDR:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}			psockets[sock].bool_opts &= ~SO_REUSEADDR_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) << 2 & SO_REUSEADDR_MASK;
			break;
		case SO_KEEPALIVE:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}			psockets[sock].bool_opts &= ~SO_KEEPALIVE_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) << 3 & SO_KEEPALIVE_MASK;
			break;
		case SO_LINGER:
			break;
		case SO_OOBINLINE:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}			psockets[sock].bool_opts &= ~SO_OOBONLINE_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) << 4 & SO_OOBONLINE_MASK;
			break;
		case SO_SNDBUF:
			break;
		case SO_RCVBUF:
			break;
		case SO_DONTROUTE:
			if (option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}			psockets[sock].bool_opts &= ~SO_DONTROUTE_MASK;
			psockets[sock].bool_opts |= (*(int*)option_value != 0) << 5 & SO_DONTROUTE_MASK;
			break;
		case SO_RCVLOWAT:
			break;
		case SO_RCVTIMEO:
			if (option_len < sizeof(struct timeval))
			{
				errno = EINVAL;
				return	-1;
			}
			memmove(&psockets[sock].recv_timeout, option_value, sizeof(struct timeval));
			break;
		case SO_SNDLOWAT:
			break;
		case SO_SNDTIMEO:
			break;
		}
	}

	return	0;
}


/*
 *	Get socket options
 */
int getsockopt(int sock, int level, int option_name, void *option_value, socklen_t *option_len)
{
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EINVAL;
		return	-1;
	}

	if (SOL_SOCKET == level)
	{
		switch (option_name)
		{
		case SO_DEBUG:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_DEBUG_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_BROADCAST:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_BROADCAST_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_REUSEADDR:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_REUSEADDR_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_KEEPALIVE:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_KEEPALIVE_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_LINGER:
			break;
		case SO_OOBINLINE:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_OOBONLINE_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_SNDBUF:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ETH_MTU;
			*option_len = sizeof(int);
			break;
		case SO_RCVBUF:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = SOCKBUF_LEN;
			*option_len = sizeof(int);
			break;
		case SO_DONTROUTE:
			if (*option_len < sizeof(int))
			{
				errno = EINVAL;
				return	-1;
			}
			*(int*)option_value = ((psockets[sock].bool_opts & SO_DONTROUTE_MASK) != 0);
			*option_len = sizeof(int);
			break;
		case SO_RCVLOWAT:
			break;
		case SO_RCVTIMEO:
			if (*option_len < sizeof(struct timeval))
			{
				errno = EINVAL;
				return	-1;
			}
			memmove(option_value, &psockets[sock].recv_timeout, sizeof(struct timeval));
			break;
		case SO_SNDLOWAT:
			break;
		case SO_SNDTIMEO:
			break;
		}
	}

	return	0;
}

/*
 *  getsockname()
 *      Retrieve local address associated with the socket (by bind())
 *
 *  Parameters:
 *      sock - socket's descriptor
 *      name - socket's address (output parameter)
 *      namelen - length of "name" (output parameter)
 *
 *  Return value:
 *      0: Success
 *      -1: error (errno is set appropriately)
 */
int getsockname(int sock, struct sockaddr *name, socklen_t *namelen)
{
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

    // Parameters validation
	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EBADF;
		return	-1;
	}
	if (0 == psockets[sock].protocol)
	{
		errno = EBADF;
		return	-1;
	}
	if (NULL == name || NULL == namelen || *namelen < sizeof(struct sockaddr_in))
	{
		errno = EINVAL;
		return	-1;
	}
	
	memcpy(name, &psockets[sock].addr, sizeof(struct sockaddr_in));
	*namelen = sizeof(struct sockaddr_in);
	
	return	0;
}

/*
 *  getpeername()
 *      Retrieve remote address connected to the socket (or associated with UDP connect())
 *
 *  Parameters:
 *      sock - socket's descriptor
 *      name - peer's address (output parameter)
 *      namelen - length of "name" (output parameter)
 *
 *  Return value:
 *      0: Success
 *      -1: error (errno is set appropriately)
 */
int getpeername(int sock, struct sockaddr *name, socklen_t *namelen)
{
	struct sock_file	*sf;

	sf = files[sock].file_struct;
	sock = sf->sock;

    // Parameters validation
	sock -= FIRST_SOCKET_DESCR;
	if (sock < 0 || sock >= MAX_SOCKETS)
	{
		errno = EBADF;
		return	-1;
	}
	if (0 == psockets[sock].protocol)
	{
		errno = EBADF;
		return	-1;
	}

	if (NULL == name || NULL == namelen || *namelen < sizeof(struct sockaddr_in))
	{
		errno = EINVAL;
		return	-1;
	}
	if (!psockets[sock].attrib & SOCK_ATTR_CONN)
	{
		errno = ENOTCONN;
		return	-1;
	}
	
	memcpy(name, &psockets[sock].peer, sizeof(struct sockaddr_in));
	*namelen = sizeof(struct sockaddr_in);
	
	return	0;
}

/***********************************
*	Address conversion functions
************************************/
int inet_aton(const char *src, struct in_addr *dst)
{
    const char *p, *p1;
	char	stmp[16];
    int cnt;
    unsigned long temp_addr = 0;
	unsigned long utmp;

    /*************** Parameter Validation*********** */

    /* Valid string */
	if(NULL == src)
{
serial_printf("%s(): error 1\n", __func__);
		return 0;
}
	if (strlen(src) > 15)
{
serial_printf("%s(): error 2\n", __func__);
		return	0;
}

	for (cnt = 0, p = src; cnt < 4; ++cnt)
	{
		p1 = strchr(p, '.');
serial_printf("%s(): p1=%08X\n", __func__, p1);
		if (p1 != NULL)
		{
			if (3 == cnt)
{
serial_printf("%s(): error 3\n", __func__);
				return	0;
}
serial_printf("%s(): p=%08X p1-p=%d\n", __func__, p, p1-p);
			strncpy(stmp, p, p1-p);
serial_printf("``%s(): p=%08X p1-p=%d\n", __func__, p, p1-p);
			stmp[p1-p] = '\0';
		}
		else
		{
			if (3 != cnt)
{
serial_printf("%s(): error 4\n", __func__);
				return	0;
}
			strcpy(stmp, p);
		}
serial_printf("%s(): stmp=%s\n", __func__, stmp);

		if (strlen(stmp) > 3)
{
serial_printf("%s(): error 5 stmp=%s\n", __func__, stmp);
			return	0;
}
		if (sscanf(stmp, "%u", &utmp) != 1)
{
serial_printf("%s(): error 6\n", __func__);
			return	0;
}
		if (utmp > 255)
{
serial_printf("%s(): error 7\n", __func__);
			return	0;
}
		temp_addr = temp_addr << 8 | utmp;
		p = p1 + 1;
	}

	temp_addr = htonl(temp_addr);
	dst->s_addr = temp_addr;
	return 1;
}

in_addr_t inet_addr(const char *s)
{
	struct in_addr temp_addr = {0};
	
serial_printf("%s(): s=%s\n", __func__, s);
	if (inet_aton(s, &temp_addr) == 0)
		return	INADDR_NONE;
	return	temp_addr.s_addr;
}

char *inet_ntoa(struct in_addr inaddr)
{
	unsigned long	temp_addr;
	static char	ip_addr[16];
	
	temp_addr = inaddr.s_addr;
	temp_addr = ntohl(temp_addr);
	sprintf(ip_addr, "%lu.%lu.%lu.%lu", temp_addr >> 24, temp_addr >> 16 & 0xFF, temp_addr >> 8 & 0xFF, temp_addr & 0xFF);
	return ip_addr;
}

