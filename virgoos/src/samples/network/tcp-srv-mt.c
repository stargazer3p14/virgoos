/*
 *	tcp-srv-mt.c
 *
 *	Source code of TCP messaging sample September OS application (telnet server) with multi-tasking.
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "socket.h"
#include "errno.h"

#define	TELNET_CMD_IAC		255

#define	TELNET_CMD_WILL		251
#define	TELNET_CMD_WONT		252
#define	TELNET_CMD_DO		253
#define	TELNET_CMD_DONT		254

#define	TELNET_OPT_XMIT_BIN	0
#define	TELNET_OPT_SUPPRESS_GOAHEAD	3
#define	TELNET_OPT_LINEMODE	34

byte *timer_ptr = (byte*)0xB8000;

int	task1_attr, task2_attr;
char task1_msg[] = "Task1";
char task2_msg[] = "Task2";
int	task2_started = 0;

int	sock;
int	acc_sock;
struct	sockaddr_in	my_addr;
struct	sockaddr_in	peer_addr;
struct	sockaddr_in	from_addr;

socklen_t	from_addr_len;

unsigned char	msg[256];
int	msg_len;

unsigned char	ui_msg[256];
int	ui_msg_len;

unsigned char	from_msg[256];
int	from_msg_len;

extern unsigned char	def_ip_addr[4];
const unsigned char	*my_ip_addr = def_ip_addr;
const char	peer_ip_addr[4];

int	opt_xmit_bin;
int	opt_suppress_goahead;
int	opt_linemode;

char	ip_addr[64] = DEF_IP_ADDR_STR;
unsigned short	my_port = 23;				// Use another port than standard 23 if interoperability with native telnet server is required
int	max_backlog = 20;
char	greeting[] = "Welcome to Daniel Drubin's Testing Telnet Server";
char	prompt[256] = "telnet-srv:\\>";

int	char_mode_only = 0;
int	char_mode_echo_default = 1;

struct conn_prm
{
	int	sock;
};

// For testing and debugging purposes
extern struct socket   *psockets;
extern struct file	*files;

/*
void	task2( void )
{
	int	i;
	int	j = 0;
	char	buf[ 256 ];

	while ( 1 )
	{
		for ( i = 0; i < strlen( task1_msg ); ++i )
		{
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 30 ) * 2 ) ) = task2_msg[ i ];
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 30 ) * 2 + 1 ) ) = task2_attr++;
		}

		open_drv( DEV_ID( TERM_DEV_ID, 0 ) );
		sprintf( buf, "Task2: %d\r\n", ++j );
		write_drv( DEV_ID( TERM_DEV_ID, 0 ), buf, strlen( buf ) );
	}
}
*/

static void	sock_report_error(char *msg, int code)
{
	serial_printf("%s(): %s (%d)\r\n", __func__, msg, code);
}


void	conn_thread(void *prm)
{
	struct conn_prm	*p = (struct conn_prm*)prm;
	int	i, j, k, l;
	char	*commands[] = {"ver", "dump"};
	int	sock = p->sock;
	int	ch = 0;
	char	ascii;
	int	msg_count = 0;
	int	cmd_count = 0;
	char	septos_cmd[256];
	int	len = 0;
	int	rv;
	int	acc_sock = sock;
	char	buf[1024];
	static int	count = 1;

serial_printf("%s(): entered\n", __func__);
// There is some nasty bug. When the new task is created and passed a small structures containing only socket descriptor, the socket structure itself (!) is garbled; in particular, protocol becomes 0.
// The line below fixes it, but it shouldn't be  here. Also, the whole behavior is weird (how task creation can access sockets structure and wipe a single field in it?????
// As it appears, other fields remain correct - addr, peer etc, only protocol is damaged
{
 psockets[p->sock-FIRST_SOCKET_DESCR].protocol = IPPROTO_TCP;
serial_printf("%s(): p=%08X (p->sock=%d) &socket=%08X new socket's protocol received in conn_thread() is %d (addr=%s:%hu \n", __func__, p, p->sock, &psockets[p->sock-FIRST_SOCKET_DESCR], psockets[p->sock-FIRST_SOCKET_DESCR].protocol, inet_ntoa(psockets[p->sock-FIRST_SOCKET_DESCR].addr.sin_addr), psockets[p->sock-FIRST_SOCKET_DESCR].addr.sin_port);
serial_printf("peer=%s:%hu)\n", inet_ntoa(psockets[p->sock-FIRST_SOCKET_DESCR].peer.sin_addr), psockets[p->sock-FIRST_SOCKET_DESCR].peer.sin_port);
serial_printf("%s(): ack_bitmap=%08X\n", __func__, psockets[p->sock-FIRST_SOCKET_DESCR].ack_bitmap);
}

#if 0
	// Send prompt
	sprintf(msg,	"\r\n" 
				"SeptOS:/>");
	msg_len = strlen(msg);
	send(acc_sock, msg, msg_len, 0);
#endif
	do
	{
		// Send test string
		sprintf(buf, "Test TCP string: hello, world! (%d)\n", count++);
		
		rv = send(acc_sock, buf, strlen(buf)+1, 0);
		if (rv < 0)
		{
			sock_report_error("Error sending data", errno);
			return;
		}
		serial_printf("Test data sent, rv=%d\n", rv);
		memset(buf, sizeof(buf), 0);
		rv = recv(acc_sock, buf, sizeof(buf), 0);
		if (rv < 0)
		{
			sock_report_error("Error receiving data", errno);
			return;
		}
		serial_printf("%s(): Test data received: %s, rv=%d\n", __func__, buf, rv);
	} while(strcmp(buf, "exit") != 0);

#if 0
	while ( 1 )
	{
		ch = 0;

		for ( i = 0; i < strlen( task2_msg ); ++i )
		{
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 50 ) * 2 ) ) = task1_msg[ i ];
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 50 ) * 2 + 1 ) ) = task1_attr++;
		}

		from_addr_len = sizeof(struct sockaddr_in);
		from_msg_len = sizeof(from_msg);
		rv = recv(acc_sock, from_msg, from_msg_len, 0);
		if (rv < 0)
		{
			serial_printf("     recv() error: %d\r\n", errno);
			continue;
		}
		from_msg[rv] = '\0';
		serial_printf("     %d): Received %d bytes <<%s>>     \r\n", msg_count++, rv, from_msg);
/*
		sprintf(msg, "<<%s>>\r\n"
					"SeptOS:/>", from_msg);
		msg_len = strlen(msg) + 1;
		send(acc_sock, msg, msg_len, 0);
*/


		// Parse what we've got: command, data...
		i = 0;
		j = 0;
		while (i < rv)
		{
			if (TELNET_CMD_IAC == from_msg[i])
			{
				switch (from_msg[++i])
				{
				case TELNET_CMD_WILL:
					msg[j++] = TELNET_CMD_IAC;
					++i;
					if (TELNET_OPT_XMIT_BIN == from_msg[i] || TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i])
					{
						if (TELNET_OPT_XMIT_BIN == from_msg[i])
							opt_xmit_bin = 1;
						else
							opt_suppress_goahead = 1;
						msg[j++] = TELNET_CMD_DO;
					}
					else
						msg[j++] = TELNET_CMD_DONT;
					msg[j++] = from_msg[i++];
					break;
				case TELNET_CMD_WONT:
					msg[j++] = TELNET_CMD_IAC;
					msg[j++] = TELNET_CMD_DONT;
					++i;
					msg[j++] = from_msg[i++];
					break;
				case TELNET_CMD_DO:
					msg[j++] = TELNET_CMD_IAC;
					++i;
					if (TELNET_OPT_XMIT_BIN == from_msg[i] || TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i] || TELNET_OPT_LINEMODE == from_msg[i])
					{
						if (TELNET_OPT_XMIT_BIN == from_msg[i])
							opt_xmit_bin = 1;
						else if (TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i])
							opt_suppress_goahead = 1;
						else
							opt_linemode = 1;
						msg[j++] = TELNET_CMD_WILL;
					}
					else
						msg[j++] = TELNET_CMD_WONT;
					msg[j++] = from_msg[i++];
					break;
				case TELNET_CMD_DONT:
					msg[j++] = TELNET_CMD_IAC;
					msg[j++] = TELNET_CMD_WONT;
					++i;
					msg[j++] = from_msg[i++];
					break;
				default:
					++i;
					break;
				}
			}
			else
			{
				// Skip '\n' after '\r'
				if ('\n' == from_msg[i] || '\0' == from_msg[i])
				{
					++i;
					continue;
				}
				septos_cmd[len++] = from_msg[i++];

				if ('\r' == septos_cmd[len-1])
				{
					// We've got a complete command
					septos_cmd[len-1] = '\0';

					++cmd_count;
					// Check what we've got
					for (k = 0; isspace(septos_cmd[k]); ++k)
						;
					if (strncmp(septos_cmd+k, "ver", 3) == 0)
					{
						sprintf(ui_msg, "\r\n"
							"September OS version 0.1. Copyright (c) Daniel Drubin, 2007\r\n");
						ui_msg_len = strlen(ui_msg);
						send(acc_sock, ui_msg, ui_msg_len, 0);
						// Let the frame be sent
						//udelay(1000);
					}
					else if (strncmp(septos_cmd+k, "dump", 4) == 0)
					{
						dword	address;
						unsigned char *p;

						k += 4;
						for ( ;isspace(septos_cmd[k]); ++k)
							;

						address = 0;

						while (septos_cmd[k] >= '0' && septos_cmd[k] <= '9' || 
							septos_cmd[k] >= 'A' && septos_cmd[k] <= 'F' ||
							septos_cmd[k] >= 'a' && septos_cmd[k] <= 'f')
						{
							address <<= 4;
							if (septos_cmd[k] <= '9')
								address += (dword)septos_cmd[k] - '0';
							else if (septos_cmd[k] <= 'F')
								address += (dword)septos_cmd[k] - 'A' + 0xA;
							else if (septos_cmd[k] <= 'f')
								address += (dword)septos_cmd[k] - 'a' + 0xA;

							++k;
						}
						p = (unsigned char*)address;

						sprintf(ui_msg, "\r\n"
							"*%08X = ", address);

						for (l = 0; l < 16; ++l)
							sprintf(ui_msg + strlen(ui_msg), "%02X ", p[l]);
						sprintf(ui_msg + strlen(ui_msg), "\r\n");
						ui_msg_len = strlen(ui_msg);
						send(acc_sock, ui_msg, ui_msg_len, 0);
						// Let the frame be sent
						//udelay(1000);
					}
					// Report unknown command
					else if (len > 1)
					{
						sprintf(ui_msg, "\r\n"
							"Unknown command: <<%s>>\r\n", septos_cmd);
						ui_msg_len = strlen(ui_msg);
						send(acc_sock, ui_msg, ui_msg_len, 0);
						// Let the frame be sent
						//udelay(1000);
					}

					// Send prompt
					sprintf(ui_msg, "\r\n"
						"SeptOS:/>");
					ui_msg_len = strlen(ui_msg);
serial_printf("(%d)Sending prompt: '%s'(%d)\r\n", cmd_count, ui_msg, ui_msg_len);
					send(acc_sock, ui_msg, ui_msg_len, 0);
					// Let the frame be sent
					udelay(1000);

					len = 0;
					septos_cmd[0] = '$'/*'\0'*/;
				}

				// If not line mode, send echo
/*
				else if (!opt_linemode)
				{
					ui_msg[0] = septos_cmd[len-1];
					ui_msg_len = 1;
					send(acc_sock, ui_msg, ui_msg_len, 0);
				}
*/
			}
		}

		// If we got options, respond to them.
		if (j > 0)
		{
			msg_len = j;
			serial_printf("Sending: msg_len=%d msg=", msg_len);
			for (i = 0; i < msg_len; ++i)
				serial_printf("%02X ", msg[i]);
			serial_printf("\r\n");

			send(acc_sock, msg, msg_len, 0);
			// Let the frame be sent
			udelay(1000);
		}

serial_printf("Continue; len=%d septos_cmd[len]=%02X i=%d rv=%d from_msg[i]=%02X\r\n", len, septos_cmd[len], i, rv, from_msg[i]);
		continue;

	} // while (1)
#endif

//	terminate();
}


void	telnet_srv_main(void *unused)
{
	struct sockaddr_in	my_addr;
	int	list_sock;		// Listening socket
	int	acc_sock;		// Accepted socket
	struct sockaddr_in	remote_addr;
	int	remote_addr_len;
	int	allow = 1;
	int	err;
	int	i = 1;
#if 0
	FILE	*f;
	char	fbuf[256];
#endif

	// Since we will sleep, we need an idle task in order not to confuse the task manager
//	start_task(idle_task, NUM_PRIORITY_LEVELS - 1, 0, NULL);
	
#if 0
	// Get default IP address
	system("/sbin/ifconfig eth0 | grep \"inet addr:\" > /tmp/tmp1");
	f = fopen("/tmp/tmp1", "rt");
	fgets(fbuf, 256, f);
	fclose(f);
	unlink("/tmp/tmp1");
	sscanf(strstr(fbuf, "inet addr:") + strlen("inet addr:"), "%s", ip_addr);
	printf("Default IP address/port: %s:%hu\n", ip_addr, my_port);

	while (i < argc && '-' == argv[i][0])
	{
		if (strcmp(argv[i], "-c") == 0)
			char_mode_only = 1;
		else if (strcmp(argv[i], "-e1") == 0)
			char_mode_echo_default = 1;

		// More options here
		//	...
		else
			printf("	Unrecognized option '%s'\n", argv[i]);
		++i;
	}

	// Get IP address
	if (i < argc)
	{
		strcpy(ip_addr, argv[i++]);
		sscanf(argv[i], "%hu", &my_port);
	}
#endif

	// Create socket for listening
	list_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == list_sock)
	{
		printf("Error in socket(): %s\r\n", strerror(errno));
		return;
	}
	setsockopt(list_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&allow, sizeof(int));

	// Bind socket to local address
	my_addr.sin_addr.s_addr = inet_addr(ip_addr);
	my_addr.sin_port = htons(my_port);
	my_addr.sin_family = AF_INET;

	if (bind(list_sock, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Error in bind(): %s\r\n", strerror(errno));
		close(list_sock);
		return;
	}

	// Make socket listen
	if (listen(list_sock, max_backlog) == -1)
	{
		printf("Error in listen(): %d\r\n", strerror(errno));
		close(list_sock);
		return;
	}

	while (1)
	{
#if 0
		pthread_t	tid;
#endif
		struct conn_prm	*p;

		// Accept incomming connection
		remote_addr_len = sizeof(struct sockaddr_in);
		if ((acc_sock = accept(list_sock, (struct sockaddr*)&remote_addr, &remote_addr_len)) == -1)
		{
			printf("Error in accept(): %d\r\n", strerror(errno));
			close(list_sock);
			return;
		}
		printf("Connected from: %s:%hu\r\n", inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

		p = calloc(1, sizeof(*p));
		p->sock = acc_sock;

#if 1 
		// Create thread for handling accepted connection
		//err = pthread_create(&tid, NULL, conn_thread, p);
{
extern struct socket   *psockets;
serial_printf("%s(): p=%08X, new socket's protocol passed to conn_thread() is %d\n", __func__, p, psockets[((struct sock_file*)(files[p->sock].file_struct))->sock].protocol);
}
		err = start_task(conn_thread, DEF_PRIORITY_LEVEL, 0, p);
		if (err != 0)
		{
			printf("Error in pthread_create(): %d(%s) - connection not established\r\n", err, strerror(errno));
			close(acc_sock);
			free(p);
			continue;
		}
#else
		conn_thread(p);
#endif
	}
}


void	app_entry()
{

//	start_task(telnet_srv_main, 1, OPT_TIMESHARE, NULL);
	telnet_srv_main(NULL);

//	After start_task() this code will never be reached.
	for (;;)
		;
/*
label:
	_asm	mov eax, offset label
	_asm	nop
	_asm	hlt
	_asm	jmp	label
*/
}

