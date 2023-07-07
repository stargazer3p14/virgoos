/*
 *	socket.h
 *
 *	Header file for sockets interface
 */

#ifndef	SOCKET__H
#define	SOCKET__H

#include "inet.h"
#include "sosdef.h"
#include "io.h"

// Socket's attributes
#define	SOCK_ATTR_LISTEN	0x1		// Listening
#define	SOCK_ATTR_CONN		0x2		// Connected
#define	SOCK_ATTR_NONBLOCKING	0x4		// Blocking / non-blocking sockets (by default sockets are non-blocking)

#define	FIRST_SOCKET_DESCR	0 /*100*/		// First socket's descriptor
#define	MAX_SOCKETS	20				// Maximum sockets
#define	MAX_PENDING_CONN	4		// Maximum pending connections

// Socket status flags
#define	SOCK_STAT_MAYSEND	0x1
#define	SOCK_STAT_MAYRECV	0x2
#define	SOCK_STAT_RECVSTARTED	0x4
#define	SOCK_STAT_MAYACC	0x8
#define	SOCK_STAT_ACCSTARTED	0x10
#define	SOCK_STAT_PEERRESET	0x20
#define	SOCK_STAT_SHUT_RD	0x40		// We need those separately from SOCK_STAT_MAYRECV and SOCK_STAT_MAYSEND because they may alter return codes
#define	SOCK_STAT_SHUT_WR	0x80		// State xxx_SHUT_xxx will also cause appropriate !xxx_STAT_MAYxxx
#define	SOCK_STAT_CLOSING	0x100		// close() was issued, but connection termination work still needs to be done
#define	SOCK_STAT_HASDATA	0x200		// socket has data received in buffer, waiting for recv()
#define	SOCK_STAT_READY_RD	0x400		// read() / recv() / accept() from socket won't block
						// We need both "HASDATA" and READY_RD separately, because "READY_RD" may refer to listening socket as well
#define	SOCK_STAT_READY_WR	0x800		// write() / send() to socket won't block
#define	SOCK_STAT_ERR		0x80000000	// Error indicator

// Socket event flags
#define	SOCK_EVENT_READ_TIMEOUT	0x1
#define	SOCK_EVENT_WRITE_TIMEOUT	0x2

// fcntl() function codes and flags
#define	F_GETFL		0x1
#define	F_SETFL		0x2

// shutdown() flag
enum {SHUT_RD, SHUT_WR, SHUT_RDWR};

#define	SOCKBUF_LEN	4096
//
// Address family
enum	{AF_UNSPEC = 0, AF_INET = 1};
enum	{PF_UNSPEC = 0, PF_INET = 1};

// Socket types
enum	{SOCK_STREAM, SOCK_DGRAM, SOCK_RAW};

// Socket options levels
#define	SOL_SOCKET	0x8000

// Socket options
enum	{SO_DEBUG, SO_BROADCAST, SO_REUSEADDR, SO_KEEPALIVE, SO_LINGER, SO_OOBINLINE, SO_SNDBUF, SO_RCVBUF,
	SO_DONTROUTE, SO_RCVLOWAT, SO_RCVTIMEO, SO_SNDLOWAT, SO_SNDTIMEO};

// Socket options values (mask)
#define	SO_DEBUG_MASK		0x1
#define	SO_BROADCAST_MASK	0x2
#define	SO_REUSEADDR_MASK	0x4
#define	SO_KEEPALIVE_MASK	0x8
#define	SO_OOBONLINE_MASK	0x10
#define	SO_DONTROUTE_MASK	0x20

struct sockaddr
{
	unsigned short  sa_family;
	char            sa_data[14];
} __attribute__ ((packed));

struct	in_addr
{
    unsigned long s_addr;
};

struct sockaddr_in
{
	unsigned short	sin_family;   // AF_INET is supported
	unsigned short	sin_port;
	struct in_addr	sin_addr; 
	char		sin_zero[8];
} __attribute__ ((packed));


// Connections waiting for accept() to be called on a listening socket
struct	pending_conn
{
	struct	sockaddr_in	peer;		// IP address that connected
	uint32_t	this_seqnum;
	uint32_t	peer_seqnum;		// Peer's ISN + 1
	unsigned	peer_win_size;
};


struct	socket
{
	int	protocol;
	int	sock_type;
	int	bound;						// Index of IP address, to which the socket is bound (1-based, 0 means not bound)
	struct sockaddr_in	addr;		// IP address and port (needed address!)
	struct sockaddr_in	peer;		// For connected sockets
	
// Socket options
	unsigned char	bool_opts;		// Boolean options
	struct	timeval	recv_timeout;	// Receive timeout (0 means don't time out; timeouts less than 1 tick return immediately)

// Receive data buffer. send() transmits data immediately - no buffering.
	int	data_len;
	unsigned char	*buf;
	
//	struct task_q	*read_sleeping;		// Task(s) sleeping on this socket waiting for read event
//	struct task_q	*write_sleeping;	// Task(s) sleeping on this socket waiting for write event
//	struct task_q	*acc_sleeping;		// Task(s) sleeping on this socket waiting for accept event
	struct task_q	*select_sleeping;	// Task(s) sleeping on this socket waiting for select events
	struct events_sel_q *sel_q;		// Task(s) sleeping on this socket waiting for ALL events
	unsigned	attrib;			// Socket attributes
	unsigned	status;			// May send, may receive, may accept, ... etc.
	int	last_err;
	unsigned	select_events_mask;	// Mask of select()'s events of interest: may send, may receive, may accept etc.
	
// TCP states (TCP sockets-specific)
	unsigned	tcp_state;
	uint32_t	this_seqnum;		// This side's sequence number
	uint32_t	this_acknowledged;	// This side's last sequence number that received acknowledgement (`this_seqnum' may be up to `peer_win_size' ahead)
	uint32_t	peer_seqnum;		// Peer's sequence number (last acknowledged). ACKs to peer SNs are handled with `ack_bitmap'
//	uint32_t	peer_acknowledged;	// Peer's last sequence number, for which we sent an ACK. (`peer_seqnum' may be up to `this_win_size' ahead - packets with "bigger distance" we will drop)
	uint32_t	this_isn;		// This side's ISN
	uint32_t	peer_isn;		// Peer's ISN
	unsigned	this_win_size;		// This side's window size
	unsigned	peer_win_size;		// Peer's window size
	unsigned	tcp_retransmit_interval;		// In timer ticks
	unsigned char	*ack_bitmap;		// Acknowledgement bitmap
	int	ack_bitmap_size;		// Size of ack_bitmap

// Pending connections waiting for accept()	
	int	max_pending_conn;			// Maximum connections waiting for accept()
	int	first_pending_conn, num_pending_conn;	// First pending connection and number of pending connections.
									// Connections queue wrap to index 0 after reaching MAX_PENDING_CONN
	struct	pending_conn	pending_conn[MAX_PENDING_CONN];	// Pending connections (for listening sockets)
	int	fd;
};

typedef	unsigned	socklen_t;

struct msghdr;

// Sockets functions
int socket(int domain, int type, int protocol);
int	close_socket(int sock);
int bind(int socket, const struct sockaddr *address, socklen_t address_len);

int accept(int socket, struct sockaddr *address,
             socklen_t *address_len);
int bind(int socket, const struct sockaddr *address,
             socklen_t address_len);
int connect(int socket, const struct sockaddr *address,
             socklen_t address_len);
int getpeername(int socket, struct sockaddr *address,
             socklen_t *address_len);
int getsockname(int socket, struct sockaddr *address,
             socklen_t *address_len);
int getsockopt(int socket, int level, int option_name,
             void *option_value, socklen_t *option_len);
int listen(int socket, int backlog);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t length,
             int flags, struct sockaddr *address, socklen_t *address_len);
ssize_t recvmsg(int socket, struct msghdr *message, int flags);
ssize_t send(int socket, const void *message, size_t length, int flags);
ssize_t sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags,
             const struct sockaddr *dest_addr, socklen_t dest_len);
int setsockopt(int socket, int level, int option_name,
             const void *option_value, socklen_t option_len);
int shutdown(int socket, int how);
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol,
             int socket_vector[2]);

int inet_aton(const char *src, struct in_addr *dst);
in_addr_t inet_addr(const char *s);
char *inet_ntoa(struct in_addr inaddr);

#if 0
#define	FD_ZERO(set)	\
	do	\
	{	\
		int	i;	\
\
		for (i = 0; i < FD_SETSIZE; ++i)	\
			((fd_set*)set)->desc[i] = 0;	\
	}	\
	while (0)

#define	FD_CLR(fd, set)	(((fd_set*)set)->desc[fd>>5] &= ~(1<<fd))
#define	FD_SET(fd, set)	(((fd_set*)set)->desc[fd>>5] |= 1<<fd)
#define	FD_ISSET(fd, set)	(((fd_set*)set)->desc[fd>>5] & 1<<fd)
#endif // 0

#endif	// SOCKET__H
