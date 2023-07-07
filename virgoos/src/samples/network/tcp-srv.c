/*
 *	tcp-srv.c
 *
 *	Source code of TCP messaging sample SOS application (telnet server).
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

byte *timer_ptr = ( byte* )0xB8000;

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
const unsigned char	peer_ip_addr[4];

int	opt_xmit_bin;
int	opt_suppress_goahead;
int	opt_linemode;

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


char	*commands[] = {"ver", "dump"};


void	task1(void *unused)
{
	int	i, j, k, l;
	int	ch = 0;
	char	ascii;
	int	msg_count = 0;
	int	cmd_count = 0;
	char	septos_cmd[256];
	int	len = 0;

	int	rv;

	start_task(idle_task, NUM_PRIORITY_LEVELS - 1, 0, NULL);
// Single-task is OK
/*
	if ( !task2_started )
	{
		start_task( task2, 1, OPT_TIMESHARE );
		++task2_started;
	}
*/
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Should be added functions inet_ntoa() and inet_addr()

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(23);		// Port 23 - telnet default port
	memcpy(&my_addr.sin_addr.s_addr, my_ip_addr, 4);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));

//	peer_addr.sin_family = AF_INET;
//	peer_addr.sin_port = htons(10004);		// Port 10004
//	memcpy(&peer_addr.sin_addr.s_addr, peer_ip_addr, 4);

	rv = listen(sock, 2);
	if (rv < 0)
	{
		serial_printf("    listen() error: %d\r\n     ", errno);
		terminate();
	}

	serial_printf("     listen() succeeded     \r\n");

	from_addr_len = sizeof(struct sockaddr_in);
	
	acc_sock = accept(sock, (struct sockaddr*)&peer_addr, &from_addr_len);
	if (acc_sock < 0)
	{
		serial_printf("    accept() error: %d\r\n     ", errno);
		terminate();
	}

#if 0
	// Send my options
	sprintf(msg, "\xFF\xFB\x03\xFF\xFB\x22\xFF\xFD\x03\xFF\xFD\x22");
	msg_len = 12;
	send(acc_sock, msg, msg_len, 0);

	udelay(1000 / 4);

	// Send prompt
	sprintf(msg,	"\r\n" 
				"SeptOS:/>");
	msg_len = strlen(msg);
	send(acc_sock, msg, msg_len, 0);
#endif

	serial_printf("     accept() succeeded, acc_sock=%d     \r\n", acc_sock);

#if 0
	// At the beginning, receive a bunch of options
	// TODO: set socket as non-blocking and wait for options no more than about 1-2 (s)
	from_msg_len = sizeof(from_msg);
	rv = recv(acc_sock, from_msg, from_msg_len, 0);
#endif

	// Send prompt
	sprintf(msg,	"\r\n" 
				"SeptOS:/>");
	msg_len = strlen(msg);
	send(acc_sock, msg, msg_len, 0);

//	printf("Connected: %s\n", inet_ntoa(peer_addr.sin_addr));

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
							"September OS version 1.0. Copyright (c) Daniel Drubin, 2007\r\n");
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

	// Never reached
	terminate();
}


void	app_entry()
{
	unsigned char	*p = ( unsigned char *) 0xB8000;
	int	i;

	char	*p1;
	long	*p2;
	char	buf[ 256 ];

//	start_task(task1, 1, OPT_TIMESHARE, NULL);
	task1(NULL);

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

