/*
 *	socktests.c
 *
 *	Unit tests for SeptemberOS sockets implementation
 */
	
#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "socket.h"
#include "errno.h"


/*************************************************************
 *
 *	Test functions
 *
 *************************************************************/

void	sock_report_error(char *msg, int code)
{
  	char	buf[1024];
	
	fprintf(stderr, "%s(): %s (%d)\r\n", __func__, msg, code);
}

/*
 *	UDP packet sending test
 *	-----------------------
 *
 *	This test sends and receives UDP packets to/from destination IP address. It executes the following functionality (success tests):
 *
 *		1) Create socket of type SOCK_DGRAM and protocol IPPROTO_UDP
 *		2) Bind it to default IP address configured in the system (DEF_IP_ADDR_STR)
 *		3) Send a test packet to IP address set in peer_addr_str
 *		4) Receive a response packet from peer. Continue to step 3 until "exit" is received
 *		5) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), sendto(), recvfrom(), inet_addr(), close()
 */
int	test_udp_send(void)
{
	int	sock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr;
	socklen_t	peer_addr_len;
	unsigned short	my_port = 0x2325;
	unsigned short	dest_port = 0x2326;
	//char	peer_addr_str[] = "10.0.0.3";
	char	peer_addr_str[] = "192.168.1.10";
	int	count = 1;
	int	done;
	
	printf("%s\n", __func__);
	
	// Create socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		sock_report_error("Error creating socket", errno);
		return 0;
	}
	printf("Created socket, sock=%d\n", sock);
	
	// Bind socket to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)DEF_IP_ADDR_STR);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding socket", errno);
		return 0;
	}
	printf("Socket bound, rv=%d\n", rv);

	done = 0;
	do
	{
		memset(buf, 0, sizeof(buf));
		peer_addr_len = sizeof(peer_addr);
		rv = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
		if (rv < 0)
		{
			sock_report_error("Error receiving data", errno);
			return 0;
		}
		printf("Test data received: %s, rv=%d peer_addr=%s:%hu peer_addr_len=%lu\n", buf, rv, inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port), peer_addr_len);
		if (strcmp(buf, "exit") == 0)
			done = 1;

		// Send test string
		sprintf(buf, "Test UDP string: hello, world! (%d)\n", count++);
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_port = htons(dest_port);
		peer_addr.sin_addr.s_addr = inet_addr(peer_addr_str);
		rv = sendto(sock, buf, strlen(buf)+1, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
		if (rv < 0)
		{
			sock_report_error("Error sending data", errno);
			return 0;
		}
		printf("Test data sent, rv=%d\n", rv);

	} while(!done);
		
	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket", errno);
		return 0;
	}
	printf("Socket closed, rv=%d\n", rv);
	      
	return	1;
}


/*
 *	TCP accept/communication test
 *	-----------------------------
 *
 *	This test sends and receives TCP packets to/from destination IP address. It executes the following functionality (success tests):
 *
 *		1) Create socket of type SOCK_STREAM and protocol IPPROTO_TCP
 *		2) Bind it to default IP address configured in the system (DEF_IP_ADDR_STR)
 *		3) Make it listen
 *		4) Let it accept incomming connections
 *		5) Send a test packet to IP address that connected
 *		6) Receive a response packet from peer. Continue to step 5 until "exit" is received
 *		7) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), listen(), accept(), send(), recv(), inet_addr(), close()
 */
int	test_tcp_accept(void)
{
	int	sock, acc_sock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr;
	socklen_t	peer_addr_len;
	unsigned short	my_port = 0x2325;
	int	count = 1;
	
	printf("%s\n", __func__);
	
	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		sock_report_error("Error creating socket", errno);
		return 0;
	}
	printf("Created socket, sock=%d\n", sock);
	
	// Bind socket to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)DEF_IP_ADDR_STR);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding socket", errno);
		return 0;
	}
	printf("Socket bound, rv=%d\n", rv);
	
	// Make the socket listen
	rv = listen(sock, 5);
	if (rv < 0)
	{
		sock_report_error("Error making socket listen", errno);
		return 0;
	}
	printf("Socket is listening, rv=%d\n", rv);

	peer_addr_len = sizeof(peer_addr);
	acc_sock = accept(sock, (struct sockaddr*)&peer_addr, &peer_addr_len);
	if (acc_sock < 0)
	{
		sock_report_error("Error accepting connection", errno);
		return 0;
	}
	printf("Socket is connected, acc_sock=%d, remote=%s:%hu peer_addr_len=%lu\n", acc_sock, inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port), peer_addr_len);

	do
	{
		// Send test string
		sprintf(buf, "Test TCP string: hello, world! (%d)\n", count++);
		
		rv = send(acc_sock, buf, strlen(buf)+1, 0);
		if (rv < 0)
		{
			sock_report_error("Error sending data", errno);
			return 0;
		}
		printf("Test data sent, rv=%d\n", rv);
		memset(buf, sizeof(buf), 0);
		rv = recv(acc_sock, buf, sizeof(buf), 0);
		if (rv < 0)
		{
			sock_report_error("Error receiving data", errno);
			return 0;
		}
		printf("Test data received: %s, rv=%d\n", buf, rv);
	} while(strcmp(buf, "exit") != 0);
		
	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket", errno);
		return 0;
	}
	printf("Socket closed, rv=%d\n", rv);
	rv = close(acc_sock);
	if (rv < 0)
	{
		sock_report_error("Error closing accepted socket", errno);
		return 0;
	}
	printf("Socket (accepted) closed, rv=%d\n", rv);
	      
	return	1;
}


/*
 *	TCP connect/communication test
 *	-----------------------------
 *
 *	This test sends and receives TCP packets to/from destination IP address. It executes the following functionality (success tests):
 *
 *		1) Create socket of type SOCK_STREAM and protocol IPPROTO_TCP
 *		2) Bind it to default IP address configured in the system (DEF_IP_ADDR_STR)
 *		3) Connect to peer (server) address
 *		4) Send a test packet to server
 *		5) Receive a response packet from server. Continue to step 4 until "exit" is received
 *		6) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), connect(), send(), recv(), inet_addr(), close()
 */
int	test_tcp_connect(void)
{
	int	sock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr;
	unsigned short	my_port = 0x2335;
	unsigned short	dest_port = 0x2326;	
	int	count = 1;
	//char	peer_addr_str[] = "10.0.0.3";
	char	peer_addr_str[] = "192.168.1.10";
	
	printf("%s\n", __func__);
	
	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		sock_report_error("Error creating socket", errno);
		return 0;
	}
	printf("Created socket, sock=%d\n", sock);
	
	// Bind socket to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)DEF_IP_ADDR_STR);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding socket", errno);
		return 0;
	}
	printf("Socket bound, rv=%d\n", rv);

	// Connect to remote
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = htons(dest_port);
	peer_addr.sin_addr.s_addr = inet_addr(peer_addr_str);
	rv = connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
	if (rv < 0)
	{
		sock_report_error("Error connecting socket", errno);
		return 0;
	}
	printf("Socket connected, rv=%d\n", rv);
	
	do
	{
		// Send test string
		sprintf(buf, "Test TCP string: hello, world! (%d)\n", count++);
		
		rv = send(sock, buf, strlen(buf)+1, 0);
		if (rv < 0)
		{
			sock_report_error("Error sending data", errno);
			return 0;
		}
		printf("Test data sent, rv=%d\n", rv);
		memset(buf, sizeof(buf), 0);
		rv = recv(sock, buf, sizeof(buf), 0);
		if (rv < 0)
		{
			sock_report_error("Error receiving data", errno);
			return 0;
		}
		printf("Test data received: %s, rv=%d\n", buf, rv);
	} while(strcmp(buf, "exit") != 0);
		
	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket", errno);
		return 0;
	}
	printf("Socket closed, rv=%d\n", rv);
	      
	return	1;
}


/*
 *	TCP+UDP accept/select/communication test
 *	----------------------------------------
 *
 *	This test sends and receives TCP and UDP packets to/from 2 sockets: one TCP and one UDP.
 *	It executes the following functionality (success tests):
 *
 *		1) Create 1 socket of type SOCK_STREAM and protocol IPPROTO_TCP and 1 socket of type SOCK_DGRAM and protocol IPPROTO_UDP
 *		2) Bind them to default IP address configured in the system (DEF_IP_ADDR_STR)
 *		3) Make TCP socket listen
 *		4) Accept connection on TCP socket
 *		5) Use select() to determine when data are received on either of the sockets
 *		6) Receive a packet from socket that signalled on select(). 
 *		7) Send a response packet to the same socket. Continue to step 5 until "exit" is received
 *		8) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), listen(), accept(), send(), sendto(), recv(), recvfrom(), 
 *	select(), inet_addr(), close()
 */
int	test_select(void)
{
	int	sock, acc_sock, usock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr, peer_uaddr;
	socklen_t	peer_addr_len;
	unsigned short	my_port = 0x2325;
	unsigned short	dest_port = 0x2326;	
	int	count = 1;
	//char	peer_addr_str[] = "10.0.0.3";
	char	peer_addr_str[] = "192.168.1.10";
	fd_set	rfds;
	struct timeval	tv;


	printf("%s\n", __func__);
	
	// Create sockets
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		sock_report_error("Error creating TCP socket", errno);
		return 0;
	}
	printf("Created TCP socket, sock=%d\n", sock);
	rv = fcntl(sock, F_SETFL, O_NONBLOCK);
    
	usock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (usock < 0)
	{
		sock_report_error("Error creating UDP socket", errno);
		return 0;
	}
	printf("Created UDP socket, sock=%d\n", usock);
	
	// Bind sockets to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)DEF_IP_ADDR_STR);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding TCP socket", errno);
		return 0;
	}
	printf("TCP socket bound, rv=%d\n", rv);

	rv = bind(usock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding UDP socket", errno);
		return 0;
	}
	printf("UDP socket bound, rv=%d\n", rv);

	// Make the socket listen
	rv = listen(sock, 5);
	if (rv < 0)
	{
		sock_report_error("Error making TCP socket listen", errno);
		return 0;
	}
	printf("Socket is listening, rv=%d\n", rv);

	peer_addr_len = sizeof(peer_addr);
	acc_sock = accept(sock, (struct sockaddr*)&peer_addr, &peer_addr_len);
    
	if (acc_sock < 0 && EAGAIN == errno)
	{
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
	
		printf("EAGAIN, going to wait for connection\n");
		rv = select(sock+1, &rfds, NULL, NULL, NULL);

		printf("Accepting select() returned: rv=%d, rfds=%08X\n", rv, rfds.desc[0]);
	
		if (rv < 0)
		{
			printf("Error on select(): %d\n", errno);
			return	0;
		}
		else if (0 == rv)
		{
			printf("Timeout on select\n");
			return 0;
		}
	
		acc_sock = accept(sock, (struct sockaddr*)&peer_addr, &peer_addr_len);        
	}
    
	if (acc_sock < 0)
	{
		sock_report_error("Error accepting connection", errno);
		return 0;
	}
	printf("Socket is connected, acc_sock=%d, remote=%s:%hu peer_addr_len=%lu\n", acc_sock, inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port), peer_addr_len);

#if 0
	// Start with sending test data on both sockets
	sprintf(buf, "Test string: hello, world! (%d)\n", count++);
	peer_uaddr.sin_family = AF_INET;
	peer_uaddr.sin_addr.s_addr = inet_addr(peer_addr_str);
	peer_uaddr.sin_port = htons(dest_port);
	rv = sendto(usock, buf, strlen(buf)+1, 0, (struct sockaddr*)&peer_uaddr, sizeof(peer_uaddr));
	if (rv < 0)
	{
		sock_report_error("Error sending data (UDP)", errno);
		return 0;
	}
	printf("Test data sent (UDP), rv=%d\n", rv);
	#endif
	
	sprintf(buf, "Test string: hello, world! (%d)\n", count++);
	rv = send(acc_sock, buf, strlen(buf)+1, 0);
	if (rv < 0)
	{
		sock_report_error("Error sending data (TCP)", errno);
		return 0;
	}
	printf("Test data sent (TCP), rv=%d\n", rv);

	do
	{
		FD_ZERO(&rfds);
		FD_SET(acc_sock, &rfds);
		FD_SET(usock, &rfds);
		
		// Timeout = 5.5 seconds
		tv.tv_sec = 5;
		tv.tv_usec = 500000;
		
		rv = select(acc_sock+1, &rfds, NULL, NULL, &tv);
		
		printf("select() returned: rv=%d, rfds=%08X\n", rv, rfds.desc[0]);
	
		if (rv < 0)
		{
			printf("Error on select(): %d\n", errno);
			close(sock);
			close(usock);
			return	0;
		}
		else if (0 == rv)
		{
			printf("Timeout on select()\n");
			continue;
		}
	  
		// Read data from sockets that appeared selected and send test string to them
		if (FD_ISSET(usock, &rfds))
		{
			memset(buf, sizeof(buf), 0);
			peer_addr_len = sizeof(peer_uaddr);
			rv = recvfrom(usock, buf, sizeof(buf), 0, (struct sockaddr*)&peer_uaddr, &peer_addr_len);

			if (rv < 0)
			{
				sock_report_error("Error receiving data (UDP)", errno);
				return 0;
			}
			printf("Test data received (UDP) from %s: %s, rv=%d\n", inet_ntoa(peer_uaddr.sin_addr), buf, rv);

			// TESTTESTTEST
//			after_select = 1;
			
			if(strcmp(buf, "exit") == 0)
				break;
			
			sprintf(buf, "Test string: hello, world! (%d)\n", count++);

			peer_uaddr.sin_family = AF_INET;
			peer_uaddr.sin_addr.s_addr = inet_addr(peer_addr_str);
			peer_uaddr.sin_port = htons(dest_port);
			rv = sendto(usock, buf, strlen(buf)+1, 0, (struct sockaddr*)&peer_uaddr, sizeof(peer_uaddr));
			
			if (rv < 0)
			{
				sock_report_error("Error sending data (UDP)", errno);
				return 0;
			}
			printf("Test data sent (UDP), rv=%d\n", rv);
		}
		if (FD_ISSET(acc_sock, &rfds))
		{
			memset(buf, sizeof(buf), 0);
			rv = recv(acc_sock, buf, sizeof(buf), 0);
			if (rv < 0)
			{
				sock_report_error("Error receiving data (TCP)", errno);
				return 0;
			}
			printf("Test data received (TCP): %s, rv=%d\n", buf, rv);
			
			if(strcmp(buf, "exit") == 0)
				break;

			sprintf(buf, "Test string: hello, world! (%d)\n", count++);
			
			rv = send(acc_sock, buf, strlen(buf)+1, 0);
			if (rv < 0)
			{
				sock_report_error("Error sending data (TCP)", errno);
				return 0;
			}
			printf("Test data sent (TCP), rv=%d\n", rv);
		}
		
		// Send test string to sockets that appeared selected
	} while(1);
		
	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket (TCP)", errno);
		return 0;
	}
	printf("Socket (TCP) closed, rv=%d\n", rv);
	rv = close(acc_sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket (TCP accepted)", errno);
		return 0;
	}
	printf("Socket (TCP accepted) closed, rv=%d\n", rv);
	
	rv = close(usock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket (UDP)", errno);
		return 0;
	}
	printf("Socket (UDP) closed, rv=%d\n", rv);
	      
	return	1;
}


/*
 *	UDP packet sending test
 *	-----------------------
 *
 *	This test sends and receives UDP packets to/from destination IP address. It executes the following functionality (success tests):
 *
 *		1) Create socket of type SOCK_DGRAM and protocol IPPROTO_UDP
 *		2) Bind it to default IP address configured in the system (DEF_IP_ADDR_STR)
 *		3) Send a test packet to IP address set in peer_addr_str
 *		4) Receive a response packet from peer. Continue to step 3 until "exit" is received
 *		5) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), sendto(), recvfrom(), inet_addr(), close()
 */
int	test_udp_send_nonblock(void)
{
	int	sock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr;
	socklen_t	peer_addr_len;
	unsigned short	my_port = 0x2325;
	unsigned short	dest_port = 0x2326;
	//char	peer_addr_str[] = "10.0.0.3";
	char	peer_addr_str[] = "192.168.1.10";
	int	count = 1;
	int	done;
	
	printf("%s\n", __func__);
	
	// Create socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		sock_report_error("Error creating socket", errno);
		return 0;
	}
	printf("Created socket, sock=%d\n", sock);
	
	// Bind socket to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)DEF_IP_ADDR_STR);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding socket", errno);
		return 0;
	}
	printf("Socket bound, rv=%d\n", rv);
	
	rv = fcntl(sock, F_SETFL, O_NONBLOCK);
	printf("fcntl() returned %d\n", rv);

	done = 0;
	do
	{
		memset(buf, 0, sizeof(buf));
		peer_addr_len = sizeof(peer_addr);
		sleep(5);
		rv = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
		if (rv < 0)
		{
			if (EAGAIN != errno)
			{
				sock_report_error("Error receiving data", errno);
				return 0;
			}
			printf("recvfrom(): EAGAIN\n");
			continue;
		}
		printf("Test data received: %s, rv=%d peer_addr=%s:%hu peer_addr_len=%lu\n", buf, rv, inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port), peer_addr_len);
		if (strcmp(buf, "exit") == 0)
			done = 1;

		// Send test string
		sprintf(buf, "Test UDP string: hello, world! (%d)\n", count++);
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_port = htons(dest_port);
		peer_addr.sin_addr.s_addr = inet_addr(peer_addr_str);
		rv = sendto(sock, buf, strlen(buf)+1, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
		if (rv < 0)
		{
			if (EAGAIN != errno)
			{
				sock_report_error("Error sending data", errno);
				return 0;
			}
			printf("sendto(): EAGAIN\n");
			--count;
		}
		printf("Test data sent, rv=%d\n", rv);

	} while(!done);

	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket", errno);
		return 0;
	}
	printf("Socket closed, rv=%d\n", rv);
	      
	return	1;
}


void	sock_tests(void *unused)
{
	int	rv;
  
	// Since we will sleep, we need an idle task in order not to confuse the task manager
	start_task(idle_task, IDLE_PRIORITY_LEVEL, 0, NULL);
	
	printf("Starting sockets tests\n"
		    "----------------------\n");
	
#if 0 
	// UDP packets sending test
	rv = test_udp_send();
	printf(rv ? "PASS\n\n" : "FAIL\n\n");
	
	// TCP accepting/communication test
	rv = test_tcp_accept();
	printf(rv ? "PASS\n\n" : "FAIL\n\n");
	
	// TCP connecting/communication test
	rv = test_tcp_connect();
	printf(rv ? "PASS\n\n" : "FAIL\n\n");
#endif
	
#if 1 
	// TCP connecting/communication test
	rv = test_select();
	printf(rv ? "PASS\n\n" : "FAIL\n\n");
#endif
	
	// Test non-blocking sockets
	rv = test_udp_send_nonblock();
	printf(rv ? "PASS\n\n" : "FAIL\n\n");
	
	// Finished sockets tests
	printf("Sockets tests finished\n"
		    "----------------------\n");

	for(;;)
	  ;
}

void	app_entry(void)
{
//	start_task(sock_tests, DEF_PRIORITY_LEVEL, OPT_TIMESHARE, NULL);
	sock_tests(NULL);

//	After start_task() this code will never be reached.
	for (;;)
		;
}


