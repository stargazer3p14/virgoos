/*
 *	telnet-srv.c
 *
 *	Source code of simple TELNET server for SeptemberOS.
 *
 *	RFCs: 854, 855, 856, 857, 858, 1184
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "socket.h"
#include "errno.h"
#include "pthread.h"

//#define	printf	serial_printf

#define	TELNET_CMD_IAC		255

#define	TELNET_CMD_WILL		251
#define	TELNET_CMD_WONT		252
#define	TELNET_CMD_DO		253
#define	TELNET_CMD_DONT		254

#define	TELNET_CMD_SB		250
#define	TELNET_CMD_SE		240

// Options
#define	TELNET_OPT_XMIT_BIN	0
#define	TELNET_OPT_ECHO		1
#define	TELNET_OPT_SUPPRESS_GOAHEAD	3
#define	TELNET_OPT_LINEMODE	34

// Suboptions
#define	SUBOPT_MODE			1
#define	SUBOPT_FORWARDMASK	2
#define	SUBOPT_SLC			3

// Modes (bitmask)
#define	MODE_EDIT             1
#define	MODE_TRAPSIG          2
#define	MODE_ACK         4
#define	MODE_SOFT_TAB         8
#define	MODE_LIT_ECHO        16


// SLC levels
#define	SLC_DEFAULT      3
#define	SLC_VALUE        2
#define	SLC_CANTCHANGE   1
#define	SLC_NOSUPPORT    0

#define	SLC_LEVELBITS    3
#define	SLC_ACK        128
#define	SLC_FLUSHIN     64
#define	SLC_FLUSHOUT    32


// SLC functions
#define	SLC_SYNCH        1
#define	SLC_BRK          2
#define	SLC_IP           3
#define	SLC_AO           4
#define	SLC_AYT          5
#define	SLC_EOR          6
#define	SLC_ABORT        7
#define	SLC_EOF          8
#define	SLC_SUSP         9
#define	SLC_EC          10
#define	SLC_EL          11
#define	SLC_EW          12
#define	SLC_RP          13
#define	SLC_LNEXT       14
#define	SLC_XON         15
#define	SLC_XOFF        16
#define	SLC_FORW1       17
#define	SLC_FORW2       18
#define	SLC_MCL         19
#define	SLC_MCR         20
#define	SLC_MCWL        21
#define	SLC_MCWR        22
#define	SLC_MCBOL       23
#define	SLC_MCEOL       24
#define	SLC_INSRT       25
#define	SLC_OVER        26
#define	SLC_ECR         27
#define	SLC_EWR         28
#define	SLC_EBOL        29
#define	SLC_EEOL        30

#define	MAX_MONITOR_CMD_SIZE	256
#define	MAX_MONITOR_LINE	1024

static char monitor_prompt[] = "SeptOS:\\>";

void	run_cmd(char *cmdline);


char	ip_addr[64] = DEF_IP_ADDR_STR;
unsigned short	my_port = 23;				// Use another port than standard 23 if interoperability with native telnet server is required
int	max_backlog = 20;
char	greeting[] = "Welcome to Daniel Drubin's Testing Telnet Server\r\n";
//char	prompt[256] = "telnet-srv:\\>";
char	*prompt = monitor_prompt;

int	char_mode_only = 0;				// Accept only char mode (not line mode)
int	char_mode_echo_default = 1;			// Default mode is echo for each char
int	ignore_options = 1;				// Don't negotiate options; behave as "dump" TCP server not aware of TELNET protocol

struct conn_prm
{
	int	sock;
};

int	curr_sock;


void	*conn_thread(void *prm)
//void	conn_thread(void *prm)
{
	struct conn_prm	*p = (struct conn_prm*)prm;
	int	i, j, k, l;
	int	ch = 0;
	char	ascii;
	int	msg_count = 0;
	int	cmd_count = 0;
	char	test_cmd[256];
	int	len = 0;
	int	rv;
	unsigned char	msg[256];
	int	msg_len;
	unsigned char	ui_msg[256];
	int	ui_msg_len;
	unsigned char	from_msg[256];
	int	from_msg_len;
	int	opt_xmit_bin = 0;
	int	opt_suppress_goahead = 0;
	int	opt_linemode = 0;
	int	opt_echo = 0;

	int	mode_edit = 0;

	printf("*********** %s(): p=%08X sock=%d **************\n", __func__, p, p->sock);
	curr_sock = p->sock;

	// Send greeting
	printf("Sending greeting: %s(%d)\n", greeting, strlen(greeting));
	send(p->sock, greeting, strlen(greeting), 0);

	// Send prompt
	sprintf(ui_msg, "\r\n%s", prompt);
	ui_msg_len = strlen(ui_msg);
	printf("(%d)Sending prompt: '%s'(%d)\r\n", cmd_count, ui_msg, ui_msg_len);
	send(p->sock, ui_msg, ui_msg_len, 0);

	if (!ignore_options)
	{
		// Initially send our options set; provoke negotiation of LINEMODE
		msg[0] = TELNET_CMD_IAC;
		msg[1] = TELNET_CMD_DO;
		msg[2] = TELNET_OPT_LINEMODE;
		msg[3] = TELNET_CMD_IAC;
		msg[4] = TELNET_CMD_DO;
		msg[5] = TELNET_OPT_SUPPRESS_GOAHEAD;
		msg[6] = TELNET_CMD_IAC;
		msg[7] = TELNET_CMD_WILL;
		msg[8] = TELNET_OPT_SUPPRESS_GOAHEAD;
		msg_len = 9;
		printf("Sending initial options\r\n");
		send(p->sock, msg, msg_len, 0);
		opt_linemode = 1;
		opt_suppress_goahead = 1;
	}

//	if (ignore_options)
//		opt_echo = 1;

	while ( 1 )
	{
		ch = 0;

		// Receive data
		from_msg_len = sizeof(from_msg);
		rv = recv(p->sock, from_msg, from_msg_len, 0);
		if (rv < 0)
		{
			printf("recv() error: %s\n", strerror(errno));
			continue;
		}

		from_msg[rv] = '\0';
		printf("     Received %d bytes <<%s>>\n", rv, from_msg);
		for (i = 0; i < rv; ++i)
		{
			printf("%02X ", (unsigned)from_msg[i]);
			if (i % 8 == 7)
				printf("\n");
		}
		printf("\n\n");

		// Parse what we've got: command, data...
		i = 0;
		j = 0;

		while (i < rv)
		{
			// If command
			if (!ignore_options && TELNET_CMD_IAC == from_msg[i])
			{
				switch (from_msg[++i])
				{
				case TELNET_CMD_WILL:
					++i;
					if (TELNET_OPT_XMIT_BIN == from_msg[i])
					{
						if (1 == opt_xmit_bin)
						{
							++i;
							break;
						}
						opt_xmit_bin = 1;
					}
					else if (TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i])
					{
						if (1 == opt_suppress_goahead)
						{
							++i;
							break;
						}
						opt_suppress_goahead = 1;
					}
					else if (TELNET_OPT_LINEMODE == from_msg[i])
					{
						if (1 == opt_linemode)
						{
							++i;
							break;
						}
						opt_linemode = 1;
					}
					else if (TELNET_OPT_ECHO == from_msg[i])
					{
						if (1 == opt_echo)
						{
							++i;
							break;
						}
						opt_echo = 1;
					}
					else
					{
						// Irrelevant options are just ignored
						break;
					}
					msg[j++] = TELNET_CMD_IAC;
					msg[j++] = TELNET_CMD_DO;
					msg[j++] = from_msg[i++];
					break;

				case TELNET_CMD_WONT:
					++i;
					if (TELNET_OPT_XMIT_BIN == from_msg[i])
						opt_xmit_bin = 0;
					else if (TELNET_OPT_LINEMODE == from_msg[i])
					{
						opt_linemode = 0;
						if (char_mode_echo_default)
							opt_echo = 1;
					}
					else if (TELNET_OPT_ECHO == from_msg[i])
						opt_echo = 0;
					++i;
					break;

				case TELNET_CMD_DO:
					++i;
					if (TELNET_OPT_LINEMODE == from_msg[i])
					{
						// RFC 1184 2.1 - the client shouldn't send IAC DO LINEMODE. Ignore
						++i;
						break;
					}
					msg[j++] = TELNET_CMD_IAC;

					if (TELNET_OPT_XMIT_BIN == from_msg[i] || TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i])
					{
						if (TELNET_OPT_XMIT_BIN == from_msg[i])
							opt_xmit_bin = 1;
						else if (TELNET_OPT_SUPPRESS_GOAHEAD == from_msg[i])
							opt_suppress_goahead = 1;
						msg[j++] = TELNET_CMD_WILL;
					}
					else
						msg[j++] = TELNET_CMD_WONT;
					msg[j++] = from_msg[i++];
					break;

				case TELNET_CMD_DONT:
					++i;
					if (TELNET_OPT_XMIT_BIN == from_msg[i])
						opt_xmit_bin = 0;
					else if (TELNET_OPT_LINEMODE == from_msg[i])
					{
						opt_linemode = 0;
                                                if (char_mode_echo_default)
                                                        opt_echo = 1;
					}
					break;

				case TELNET_CMD_SB:
					if (TELNET_OPT_LINEMODE == from_msg[++i])
					{
						++i;
						if (SUBOPT_SLC == from_msg[i])
						{
							// Send response to SLC option sub-negotiation: all local-chars are disabled
							// IAC SB LINEMODE SLC <X\0\0> IAC SE
							msg[j++] = TELNET_CMD_IAC;
							msg[j++] = TELNET_CMD_SB;
							msg[j++] = TELNET_OPT_LINEMODE;
							msg[j++] = SUBOPT_SLC;
							++i;

							while (!(TELNET_CMD_IAC == from_msg[i] && TELNET_CMD_SE == from_msg[i+1]))
							{
								if ((from_msg[i+1] & SLC_ACK) == SLC_ACK)
								{
									i += 3;
									continue;
								}

								if ((from_msg[i+1] & SLC_LEVELBITS) != SLC_NOSUPPORT)
								{
									msg[j++] = from_msg[i];
									msg[j++] = SLC_NOSUPPORT;
									msg[j++] = 0;
								}
								else
								{
								}
								i += 3;
							} //while (octet triplets)

							// IAC SE
							msg[j++] = TELNET_CMD_IAC;
							msg[j++] = TELNET_CMD_SE;
						} // if (SUBOPT_SLC)

						else if (SUBOPT_MODE == from_msg[i])
						{
							// Parse and respond to IAC SB LINEMODE MODE mask IAC SE
							++i;
							// We always require MODE EDIT | TRAPSIG
							if (~(from_msg[i] & MODE_EDIT) || ~(from_msg[i] & MODE_TRAPSIG))
							{
								// IAC SB LINEMODE MODE
								msg[j++] = TELNET_CMD_IAC;
								msg[j++] = TELNET_CMD_SB;
								msg[j++] = TELNET_OPT_LINEMODE;
								msg[j++] = SUBOPT_MODE;

								msg[j++] = from_msg[i] | MODE_EDIT | MODE_TRAPSIG;

								// IAC SE
								msg[j++] = TELNET_CMD_IAC;
								msg[j++] = TELNET_CMD_SE;
							}
							// Do we need to ACK?
							else if (~(from_msg[i] & MODE_ACK))
							{
								// IAC SB LINEMODE MODE
								msg[j++] = TELNET_CMD_IAC;
								msg[j++] = TELNET_CMD_SB;
								msg[j++] = TELNET_OPT_LINEMODE;
								msg[j++] = SUBOPT_MODE;

								msg[j++] = from_msg[i] | MODE_ACK;

								// IAC SE
								msg[j++] = TELNET_CMD_IAC;
								msg[j++] = TELNET_CMD_SE;
							}
							++i;

							// We don't check that after mask there is indeed IAC SE - if it's not, then input is broken anyway
						} // if (SUBOPT_MODE)
						else
						{
							// Another suboption (FORWARDMASK) is illegal to be negotiated by the client, since we didn't initiate DO FORWARDMASK
							while (!(TELNET_CMD_IAC == from_msg[i] && TELNET_CMD_SE == from_msg[i+1]))
								++i;
						}

						// Skip IAC SE
						i += 2;
					} // if (TELNET_OPT_LINEMODE)
					break;

				default:
					++i;
					break;
				} // switch()
			} // if (TELNET_CMD_IAC)
			// Terminal data
			else
			{
				// Skip '\n' after '\r'
				if ('\n' == from_msg[i] || '\0' == from_msg[i])
				{
					++i;
					continue;
				}
				test_cmd[len++] = from_msg[i++];

				if ('\r' == test_cmd[len-1])
				{
					// We've got a complete command
					test_cmd[len-1] = '\0';

					++cmd_count;
					// Check what we've got
					for (k = 0; isspace(test_cmd[k]); ++k)
						;

					printf("Received line: '%s', opt_linemode=%d k=%d\r\n", test_cmd, opt_linemode, k);
					{
						int ii;

						for (ii = 0; ii < len; ++ii)
							printf("%02X ", (unsigned)test_cmd[ii]);
						printf("\n");
					}

					if (strcmp(test_cmd+k, "exit") == 0 || strcmp(test_cmd+k, "quit") == 0)
					{
						printf("Quit requested\r\n");
						shutdown(p->sock, SHUT_RDWR);
						printf("Shutdown started...\r\n");
						close(p->sock);
						free(p);
						return;
					}

					run_cmd(test_cmd+k);
					
#if 0
					// <----- Add your commands parsing, processing and output here
					else if (strcmp(test_cmd+k, "ver") == 0)
					{
						strcpy(ui_msg, "SeptemberOS version 1.0 Copyright (c) Daniel Drubin, 2007-2010\r\n");
						send(p->sock, ui_msg, strlen(ui_msg), 0);
					}
					////////////////////////////////////////////////
#endif


					// Send prompt
					sprintf(ui_msg, "\r\n%s", prompt);
					ui_msg_len = strlen(ui_msg);
					printf("(%d)Sending prompt: '%s'(%d)\r\n", cmd_count, ui_msg, ui_msg_len);
					send(p->sock, ui_msg, ui_msg_len, 0);

					len = 0;
					test_cmd[0] = '$'/*'\0'*/;
				}

				// If echoing, send back echo
				else if (opt_echo)
				{
					ui_msg[0] = test_cmd[len-1];
					ui_msg_len = 1;
					send(p->sock, ui_msg, ui_msg_len, 0);
					printf("Echoed '%c' (%02X)\r\n", ui_msg[0], (unsigned)ui_msg[0]);
				}
			} // else
		} // while (i < rv)

		// The rest deals with telnet options, ignore it
		if (ignore_options)
			continue;

		// At least once tell client to switch to MODE EDIT. Client MUST agree to our settings or bail out (RFC 1184, 2.2)
		if (!mode_edit)
		{
			// IAC SB LINEMODE MODE
			msg[j++] = TELNET_CMD_IAC;
			msg[j++] = TELNET_CMD_SB;
			msg[j++] = TELNET_OPT_LINEMODE;
			msg[j++] = SUBOPT_MODE;

			msg[j++] = MODE_EDIT | MODE_TRAPSIG;

			// IAC SE
			msg[j++] = TELNET_CMD_IAC;
			msg[j++] = TELNET_CMD_SE;
			mode_edit = 1;
		}

		// If we got options, respond to them.
		if (j > 0)
		{
			msg_len = j;
			printf("Dumping response to options:\r\n");
			for (j = 0; j < msg_len; ++j)
			{
				printf("%02X ", (unsigned)msg[j]);
				if (j % 8 == 7)
					printf("\n");
			}
			printf("\r\n\r\n");

			send(p->sock, msg, msg_len, 0);
		}
	} // while (1)
}


int	monitor_printf(char *fmt, ...)
{
	char	buf[MAX_MONITOR_LINE];
	char	out_buf[MAX_MONITOR_LINE];
	int	i, j;
	va_list	arg;

	va_start(arg, fmt);
	vsprintf(buf, fmt, arg);
	for (i = 0, j = 0; j < MAX_MONITOR_LINE - 1 && buf[i]; ++i, ++j)
	{
		if (buf[i] == '\n')
			out_buf[j++] = '\r';
		out_buf[j] = buf[i];
	}
	// TODO: this will not work for now, we need to redefine interface so that it can use multiple terminals via multiple telnet sessions (way to access p->sock)
	send(curr_sock, out_buf, strlen(out_buf), 0);
	va_end(arg);
}


void	app_entry(void)
{
	struct sockaddr_in	my_addr;
	int	list_sock;		// Listening socket
	int	acc_sock;		// Accepted socket
	struct sockaddr_in	remote_addr;
	int	remote_addr_len;
	int	allow = 1;
	int	err;
	int	i = 1;
#if 1 
		pthread_t	tid;
#endif

	printf("*********** %s(): hello, world! *************\n", __func__);

#if 0
	FILE	*f;
	char	fbuf[256];
#endif

	// Since we will sleep, we need an idle task in order not to confuse the task manager
//	pthread_create(&tid, NULL, idle_task, NULL);
//	printf("%s(): idle task created, tid=%08X\n", __func__, tid);
//	start_task(idle_task, NUM_PRIORITY_LEVELS - 1, 0, NULL);
//	start_task_ex(idle_task, NUM_PRIORITY_LEVELS - 1, 0, 0, 0, NULL, NULL);
	
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
		fprintf(stderr, "Error in socket(): %s\r\n", strerror(errno));
		return;
	}
	printf("******** %s(): socket is created, list_sock=%d\n", __func__, list_sock);
	setsockopt(list_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&allow, sizeof(int));
	printf("******** %s(): socket options are set, list_sock=%d\n", __func__, list_sock);

	// Bind socket to local address
	my_addr.sin_addr.s_addr = inet_addr(ip_addr);
	my_addr.sin_port = htons(my_port);
	my_addr.sin_family = AF_INET;

	if (bind(list_sock, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
	{
		fprintf(stderr, "Error in bind(): %s\r\n", strerror(errno));
		close(list_sock);
		return;
	}
	printf("******** %s(): bind succeeded, list_sock=%d\n", __func__, list_sock);

	// Make socket listen
	if (listen(list_sock, max_backlog) == -1)
	{
		printf("Error in listen(): %d\r\n", strerror(errno));
		close(list_sock);
		return;
	}

	while (1)
	{
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

		// Create thread for handling accepted connection
#if 1 
		printf("%s(): Calling pthread_create() to start connection handling task\n", __func__);
		err = pthread_create(&tid, NULL, conn_thread, p);
//		err = (start_task(conn_thread, 1, OPT_TIMESHARE, p) == 0);
//		err = (start_task_ex(conn_thread, 1, OPT_TIMESHARE, 0, 0, NULL, p) == 0);
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

