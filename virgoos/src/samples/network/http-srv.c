/*
 *	http-srv.c
 *
 *	A simple HTTP (over TCP) server.
 *
 *	To compile:
 *		gcc -ohttp-srv http-srv.c -lpthread
 *
 *	Command line:
 *		./http-srv [-ip x.x.x.x] [-p nnn] [-v]
 *			-ip -- specifies IP address to bind to (if omitted, follow gethostname() finding
 *			-p -- specifies port to bind to (if omitted default to 80)
 *			-v -- send some verbose output to stdout
 *
 *	Serves HTTP PUT and GET requests; TODO - run scripts answering to POST requests (should be simple...)
 *
 *	Currently works exporting current directory where started (may add option to override). (Means that "/" means "./index.html", "/cgi-bin/test.cgi" means "./cgi-bin/test.cgi" etc.
 *	Currently uses thread-per-connection model (with pthreads).
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "socket.h"
#include "errno.h"
#include "pthread.h"

#define	ACTION_GET	1
#define	ACTION_PUT	2
#define	ACTION_POST	3
#define	ACTION_HEAD	4
#define	ACTION_DELETE	5
#define	ACTION_TRACE	6
#define	ACTION_OPTIONS	7

// Default action (?)
int	action = ACTION_GET;

char	my_name[1024];
struct hostent	*host_ent;
char	my_addr[64];
struct sockaddr_in	uaddr;
unsigned short	my_port = 80;
int	verbose = 1;

extern char	**environ;
//
// Parameter to accept_handler() thread
struct	conn_prm
{
	int	sock;
	struct sockaddr_in	remote_addr;
	int	remote_addr_len;
};

//
// Parse URL
// Returns: 0 - success, -1 - error
//
int	parse_url(char *url, char *host_name, unsigned short *dest_port, char *res_name)
{
	char	*p;

	// Skip protocol specification
	p = strstr(url, "://");
	if (p != NULL)
		url = p + sizeof("://")-1;

	p = strchr(url, '/');
	if (NULL == p)
		return	-1;

	memcpy(host_name, url, p-url);
	host_name[p-url] = '\0';
	url = p;

	p = strchr(host_name, ':');
	if (p != NULL)
	{
		if (sscanf(p+1, "%hu", &dest_port) != 1)
		{
			fprintf(stderr, "Bad port number\n");
			return	-1;
		}
		*p = '\0';
	}
	strcpy(res_name, url);

	if (verbose)
	{
		printf("url parsed successfully:\n"
				"	host_name = %s\n"
				"	dest_port = %hu\n"
				"	res_name = %s\n",
					host_name, dest_port, res_name);
	}

	return	0;
}

//
// Receive \r\n-terminated line from socket into \0-terminated line in dest.
// Returns number of characters copied (not including \r\n or \0).
// In case of error, returns 0.
// Assumption: '\0' doesn't appear in input stream (must be ASCII!)
// This input function is used to receive HTTP headers
//
static unsigned	recv_line(int sock, unsigned char *dest, const unsigned size)
{
	unsigned	len;
	int	rv;
	unsigned char	buf[1024];

	for (len = 0; len < size - 1; ++len)
	{
		rv = recv(sock, buf + len, 1, 0);
		if (0 == rv)
			return	0;
		if (-1 == rv)
		{
			fprintf(stderr, "recv() returned error %d (%s)\n", errno, strerror(errno));
			return	0;
		}
		if ('\n' == buf[len])
		{
			if (len > 0 && '\r' == buf[len-1])
				--len;
			buf[len] = '\0';
			strcpy(dest, buf);
			return	len;
		}
	}
	buf[len] = '\0';
	strcpy(dest, buf);
	return	len;
}

void	*accept_handler(void *arg)
//void	accept_handler(void *arg)
{
	//---------------------------- vars -------------------------------
	int	rv;
	unsigned char	allow = 1;
	int	i;
	char	*p;
	FILE	*f;
	unsigned long	n;
	int	hdr_packet_len;
	int	sock;
	struct	conn_prm	*prm;
	char	http_request[4096];
	char	http_response[1024] = 
		"HTTP/1.1 200 OK\r\n"
		"Accept-Ranges: bytes\r\n"
		"Connection: close\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n";
	unsigned char	tcp_buf[4096];
	char	fname[2048];
	unsigned long	content_length;
	char	content_type[256];
	char	http_line[1024];
	struct sockaddr_in	remote_addr;
	int	remote_addr_len;
	char	request_method[16];
	char	request_uri[1024];
	char	request_host[256];
	unsigned short	dest_port;

	//--------------------------------- code -------------------------------
	memset(tcp_buf, 0, sizeof(tcp_buf));

	prm = (struct	conn_prm*)arg;
	sock = prm->sock;
	memcpy(&remote_addr, &prm->remote_addr, prm->remote_addr_len);
	
	if (verbose)
		printf("%s(): Connected from: %s:%hu (sock=%d remote_addr_len=%d)\n", __func__, inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port), sock, prm->remote_addr_len);

	// Receive and parse HTTP header
	// Fixed to read byte-by-byte in order to not break when HTTP header is sent in several packets or when it's incorporated with data in a same packet
	http_request[0] = '\0';
	do
	{
		int	len;

		len = recv_line(sock, http_line, 1024);
		strcat(http_request, http_line);

		if (0 == len)
			break;

		if (verbose)
			printf("Parsing: <%s>\n", http_line);
		if (sscanf(http_line, "GET %s", fname) == 1)
		{
			action = ACTION_GET;
			strcpy(request_method, "GET");
		}
		else if (sscanf(http_line, "PUT %s", fname) == 1)
		{
			action = ACTION_PUT;
			strcpy(request_method, "PUT");
		}
		else if (sscanf(http_line, "POST %s", fname) == 1)
		{
			action = ACTION_POST;
			strcpy(request_method, "POST");
		}
		else if (sscanf(http_line, "HEAD %s", fname) == 1)
		{
			action = ACTION_HEAD;
			strcpy(request_method, "HEAD");
		}
		else if (sscanf(http_line, "DELETE %s", fname) == 1)
		{
			action = ACTION_DELETE;
			strcpy(request_method, "DELETE");
		}
		else if (sscanf(http_line, "TRACE %s", fname) == 1)
		{
			action = ACTION_TRACE;
			strcpy(request_method, "TRACE");
		}
		else if (sscanf(http_line, "OPTIONS %s", fname) == 1)
		{
			action = ACTION_OPTIONS;
			strcpy(request_method, "OPTIONS");
		}
		else if (sscanf(http_line, "Content-Length: %lu", &content_length) == 1)
			;
	} while (1);

#if 0
	rv = recv(sock, (char*)tcp_buf, sizeof(tcp_buf), 0);
	if (rv < 0)
	{
		fprintf(stderr, "Error in recv(): %d (%s)\n", errno, strerror(errno));
		close(sock);
		free(prm);
		return	NULL;
	}
	if (verbose)
		printf("Received: %s\n", tcp_buf);
	hdr_packet_len = rv;

	// Parse HTTP request
	// Right now we care only about GET/PUT/POST request line and Content-Length (for PUT/POST)
	p = tcp_buf;
	
	while (*p != '\0')
	{
		char	*temp;
		
		temp = strchr(p, '\r');
		if (temp != NULL)
		{
			strncpy(http_line, p, temp-p);
			http_line[temp-p] = '\0';
			if (verbose)
				printf("Parsing: <%s>\n", http_line);
			if (sscanf(http_line, "GET %s", fname) == 1)
				action = ACTION_GET;
			else if (sscanf(http_line, "PUT %s", fname) == 1)
				action = ACTION_PUT;
			else if (sscanf(http_line, "POST %s", fname) == 1)
				action = ACTION_POST;
			else if (sscanf(http_line, "HEAD %s", fname) == 1)
				action = ACTION_HEAD;
			else if (sscanf(http_line, "DELETE %s", fname) == 1)
				action = ACTION_DELETE;
			else if (sscanf(http_line, "TRACE %s", fname) == 1)
				action = ACTION_TRACE;
			else if (sscanf(http_line, "OPTIONS %s", fname) == 1)
				action = ACTION_OPTIONS;
			else if (sscanf(http_line, "Content-Length: %lu", &content_length) == 1)
				;
		}
		p = strchr(p, '\n');
		if (p != NULL)
			++p;
		if ('\r' == *p && '\n' == *(p+1))
		{
			p += 2;
			break;
		}
	}
#endif

#if 0
// In SeptOS we accept meanhwile only absolute path
	if (fname[0] != '\0')
	{
		memmove(fname, fname + 1, strlen(fname));
		while (strchr(fname, '/') != NULL)
			*strchr(fname, '/') = '\\';
	}
#endif

	if (verbose)
	{
		printf("action=%d content_length=%lu fname=%s\n", action, content_length, fname);
	}
	
	if (ACTION_GET == action || ACTION_HEAD == action)
	{
		unsigned	k, l, m;

		f = fopen(fname, "rb");
		if (NULL == f)
		{
printf("%s(): error opening file %s: %s\n", __func__, fname, strerror(errno));
			sprintf(tcp_buf, "HTTP/1.1 404 Not Found\r\n"
			"\r\n");
			rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);
			goto	ending;
		}
printf("%s(): success opening file %s: f=%08X\n", __func__, fname, f);
		
		k = strlen(http_response);
		memcpy(tcp_buf, http_response, k);

		// For "HEAD" request, send only headers and quit (don't send actual content)
		if (ACTION_HEAD == action)
		{
			rv = send(sock, tcp_buf, k, 0);
			goto	ending;
		}

		// THIS is funny. IE recovers ASF (and probably other formats that go under "text/plain" cover) by examining data in
		// the very first TCP response packet that immediately follows HTTP header. That is, if beginning of an ASF (or another)
		// file is sent by the next packet (next send()) - then IE will not correctly handle it and just display as text
		// We're not alone with our TODO clause above...
		rv = fread(tcp_buf + k, 1, 4096 - k, f);
printf("%s(): fread() returned %d\n", __func__, rv);
printf("%s(): calling send() to send %d bytes\n", __func__, l+rv);
		l = rv;
		rv = send(sock, (char*)tcp_buf, k + rv, 0);
printf("%s(): send() returned %d\n", __func__, rv);
		if (rv < 0)
			goto	error_in_send;
		m = rv;
		
		while (!feof(f))
		{
printf("%s(): while(), !feof(), calling fread(). Using feof()!!!!!\n", __func__);
			rv = fread(tcp_buf, 1, 4096, f);
printf("%s(): while(), fread() returned %d\n", __func__, rv);
			l += rv;
//printf("%s(): total read %lu bytes\n", __func__, l);
//printf("%s(): <dump>\n");
//fwrite(tcp_buf, 1, rv, stdout);
printf("%s(): </dump>\n");
printf("%s(): while(), calling send() to send %d bytes\n", __func__, rv);
			rv = send(sock, (char*)tcp_buf, rv, 0);
printf("%s(): while(), send() returned %d\n", __func__, rv);
			if (rv < 0)
			{
error_in_send:
				// On communication error, don't shutdown socket
				fprintf(stderr, "Error in send(): %d (%s)\n", errno, strerror(errno));
				close(sock);
				free(prm);
				return	NULL;
			}
			m += rv;
printf("%s(): total sent %lu bytes\n", __func__, m);
		}
printf("%s(): feof(), calling fclose()\n", __func__);
		fclose(f);
//		strcpy(tcp_buf, test_footer);
//		rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);			
	}
	else if (ACTION_PUT == action)
	{
		f = fopen(fname, "wb");
		if (NULL == f)
		{
			sprintf(tcp_buf, "HTTP/1.1 403 Forbidden\r\n"
			"\r\n");
			rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);
			goto	ending;
		}
		n = 0;
		
		n = hdr_packet_len - (p - (char*)tcp_buf);
		fwrite(p, 1, n, f);
		
		while (n < content_length)
		{
			rv = recv(sock, tcp_buf, content_length - n > 4096 ? 4096 : content_length - n, 0);
			if (rv < 0)
			{
				fprintf(stderr, "Error in recv(): %d (%s)\n", errno, strerror(errno));
				close(sock);
				free(prm);
				return	NULL;
			}
			n += rv;
			fwrite(tcp_buf, 1, rv, f);
		}
		fclose(f);
		
		strcpy(tcp_buf, http_response);
		rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);			
	}
	else if (ACTION_POST == action)
	{
#if 0
		// POST request: invoke relevant script. Run the program specified in URL before '?', setting up environment for it,
		// parameters after '?' go to environment and request body goes to stdin.
		// Stdout goes to the connected socket directly.
		// (This is very easy: fork(), redirect socket to stdin, redirect it also to stdout (or dup()ed handle), set up
		// environment and execvp(). Probably as security measure no PATH and other sensitive variables, or just complete
		// set up of environment from ground zero.
		
		char	*script_name = fname;	// Relative to "./"
		char	params[2048];		// Stuff that appears after "?"
		char	*p;
		char	exec_buf[1024];
		int	pid;

		parse_url(fname, request_host, &dest_port, request_uri);
			
		// Get params
		params[0] = '\0';
		if ((p = strchr(fname, '?')) != NULL)
		{
			strcpy(params, p+1);
			*p = '\0';
		}

		// Run script via system(), so that shell processor can parse all those scripts interpreters
		pid = fork();
		if (pid < 0)
		{
			// fork() failed, say something to log error
			fprintf(stderr, "fork() failed [%s] whie trying to execute script = '%s' with parameters = '%s'\n",
				strerror(errno), script_name, params);
			goto	ending;
		}
		else if (0 == pid)
		{
			// Child process runs script
			char	**new_environ;
			char	temp[1024];


			sprintf(exec_buf, script_name);
			// TODO: set up environment to include parameters - no more than 1024 strings
			new_environ = malloc(sizeof(*new_environ) * 1024);
			sprintf(temp, "DOCUMENT_ROOT=\"./\"");
			new_environ[0] = malloc(strlen(temp) + 1);
			strcpy(new_environ[0], temp);
			sprintf(temp, "GATEWAY_INTERFACE=\"CGI/1.1\"");
			new_environ[1] = malloc(strlen(temp) + 1);
			strcpy(new_environ[1], temp);
			// TODO: retrieve from HTTP header
			sprintf(temp, "HTTP_ACCEPT=\"*/*\"");
			new_environ[2] = malloc(strlen(temp) + 1);
			strcpy(new_environ[2], temp);
			// TODO: retrieve from HTTP header
			sprintf(temp, "HTTP_ACCEPT_ENCODING=\"\"");
			new_environ[3] = malloc(strlen(temp) + 1);
			strcpy(new_environ[3], temp);
			// TODO: retrieve from HTTP header
			sprintf(temp, "HTTP_ACCEPT_LANGUAGE=\"en-us\"");
			new_environ[4] = malloc(strlen(temp) + 1);
			strcpy(new_environ[4], temp);
			// TODO: retrieve from HTTP header
			sprintf(temp, "HTTP_HOST=\"\"");
			new_environ[5] = malloc(strlen(temp) + 1);
			strcpy(new_environ[5], temp);
			// TODO: retrieve from HTTP header
			sprintf(temp, "HTTP_USER_AGENT=\"\"");
			new_environ[6] = malloc(strlen(temp) + 1);
			strcpy(new_environ[6], temp);
			// TODO:
			sprintf(temp, "PATH=\"\"");
			new_environ[7] = malloc(strlen(temp) + 1);
			strcpy(new_environ[7], temp);
			sprintf(temp, "QUERY_STRING=\"%s\"", params);
			new_environ[8] = malloc(strlen(temp) + 1);
			strcpy(new_environ[8], temp);
			sprintf(temp, "REMOTE_ADDR=\"%s\"", inet_ntoa(remote_addr.sin_addr));
			new_environ[9] = malloc(strlen(temp) + 1);
			strcpy(new_environ[9], temp);
			sprintf(temp, "REMOTE_PORT=\"%hu\"", htons(remote_addr.sin_port));
			new_environ[10] = malloc(strlen(temp) + 1);
			strcpy(new_environ[10], temp);
			sprintf(temp, "REQUEST_METHOD=\"%s\"", request_method);
			new_environ[11] = malloc(strlen(temp) + 1);
			strcpy(new_environ[11], temp);
			sprintf(temp, "REQUEST_URI=\"%s\"", request_uri);
			new_environ[12] = malloc(strlen(temp) + 1);
			strcpy(new_environ[12], temp);
			sprintf(temp, "SCRIPT_FILENAME=\"./%s\"", fname);
			new_environ[13] = malloc(strlen(temp) + 1);
			strcpy(new_environ[13], temp);
			sprintf(temp, "SCRIPT_NAME=\"%s\"", fname);
			new_environ[14] = malloc(strlen(temp) + 1);
			strcpy(new_environ[14], temp);
			sprintf(temp, "SERVER_ADDR=\"%s\"", my_addr);
			new_environ[15] = malloc(strlen(temp) + 1);
			strcpy(new_environ[15], temp);
			// TODO: SERVER_NAME may help to support virtual hosts
			sprintf(temp, "SERVER_NAME=\"%s\"", my_addr);
			new_environ[16] = malloc(strlen(temp) + 1);
			strcpy(new_environ[16], temp);
			sprintf(temp, "SERVER_PORT=\"%hu\"", my_port);
			new_environ[17] = malloc(strlen(temp) + 1);
			strcpy(new_environ[17], temp);
			sprintf(temp, "SERVER_PROTOCOL=\"HTTP/1.1\"", my_port);
			new_environ[18] = malloc(strlen(temp) + 1);
			strcpy(new_environ[18], temp);
			sprintf(temp, "SERVER_SOFTWARE=\"Daniel Drubin's Test HTTP Server 1.0\"", my_port);
			new_environ[19] = malloc(strlen(temp) + 1);
			strcpy(new_environ[19], temp);
			// -------------------------------------------------
			new_environ[20] = NULL;

			// Duplicate sock as stdin and stdout
			dup2(sock, 0);
//			dup2(sock, 1);

			// TODO: test if this gets redirected stdin/stdout and environment correctly
			environ = new_environ;
			exit(system(exec_buf));
		}
		else
		{
			int	status;

			// Father process waits for script to complete
			waitpid(pid, &status, 0);
			fprintf(stderr, "Script '%s' with parameters '%s' terminated with code = %d\n", script_name, params, status);
			goto	ending;
		}
#endif
	}
	else if (ACTION_TRACE == action)
	{
		// Send back request
		strcpy(tcp_buf, http_response);
		strcat(tcp_buf, http_request);
		rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);			
	}
	else if (ACTION_DELETE == action)
	{
		// Delete a file
		rv = unlink(fname);

		// TODO: if not successful, report HTTP status
		strcpy(tcp_buf, http_response);
		rv = send(sock, (char*)tcp_buf, strlen(tcp_buf), 0);			
	}
	else if (ACTION_OPTIONS == action)
	{
		// TODO: Determine if we can offer anything useful for this
	}
ending:		
	shutdown(sock, 2);
	
	close(sock);
	free(prm);
	return	NULL;
}


void	usage(char *pname)
{
	printf("Usage: %s [-ip x.x.x.x] [-p NN] [-v] [--help]\n"
			"-ip -- specifies IP address to bind to (if omitted, follow gethostname() finding\n"
			"-p -- specifies port to bind to (if omitted default to 80)\n"
			"-v -- send some verbose output to stdout\n"
			"--help -- displays this help and exits\n", pname);
}

#if 0
int main(int argc, char **argv)
#else
void	http_srv_main(void *unused)
#endif
{
	int	rv;
	unsigned char	allow = 1;
	int	i;
	char	*p;
	FILE	*f;
	unsigned long	n;
	int	hdr_packet_len;
	struct conn_prm	*prm;
	pthread_t	tid;
	int	sock;
	int	usock;
	struct sockaddr_in	remote_addr;
	int	remote_addr_len;

	// Since we will sleep, we need an idle task in order not to confuse the task manager
//	start_task(idle_task, NUM_PRIORITY_LEVELS - 1, 0, NULL);
	
#if 0
	for (i = 1; i < argc; ++i)
	{
		if (0 == strcmp(argv[i], "-p"))
		{
			sscanf(argv[++i], "%hu", &my_port);
			continue;
		}
		else if (0 == strcmp(argv[i], "-v"))
		{
			verbose = 1;
			continue;
		}
		else if (0 == strcmp(argv[i], "-ip"))
		{
			sscanf(argv[++i], "%s", my_addr);
			continue;
		}
		else if (0 == strcmp(argv[i], "--help") || 0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "-?"))
		{
			usage(argv[0]);
			exit(0);
		}
	}
#endif
	
#if 0
	// Listen on default address
	if ('\0' == my_addr[0])
	{
		// Get my host name
		rv = gethostname(my_name, sizeof(my_name));
		if (rv != 0)
		{
			fprintf(stderr, "Error in gethostname(): %d (%s)\n", errno, strerror(errno));
			return	rv;
		}
		
		if (verbose)
			printf("My name: %s\n", my_name);

		// Get my address by host name
		host_ent = gethostbyname(my_name);
		if (NULL == host_ent)
		{
			fprintf(stderr, "Error in gethostbyname(): %d (%s)\n", errno, strerror(errno));
			return	-1;
		}
		strcpy(my_addr, inet_ntoa(*(struct in_addr*)host_ent->h_addr_list[0]));
	}
#endif
	strcpy(my_addr, DEF_IP_ADDR_STR);
	
	if (verbose)
		printf("My address: %s:%hu\n", my_addr, my_port);

	// Bind socket to local address
	uaddr.sin_addr.s_addr = inet_addr(my_addr);
	uaddr.sin_port = htons(my_port);
	uaddr.sin_family = AF_INET;

	// Create socket for accepting connections
	usock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (-1 == usock)
	{
		fprintf(stderr, "Error in socket(): %d (%s)\n", errno, strerror(errno));
		return;
	}
	setsockopt(usock, SOL_SOCKET, SO_REUSEADDR, (char*)&allow, sizeof(int));

	// Bind socket to local address
	if (bind(usock, (struct sockaddr *)&uaddr, sizeof(struct sockaddr)) == -1)
	{
		fprintf(stderr, "Error in bind(): %d (%s)\n", errno, strerror(errno));
		close(usock);
		return;
	}

	// Make socket listen
	if (listen(usock, 5) == -1)
	{
		fprintf(stderr, "Error in listen(): %d\n", errno, strerror(errno));
		close(usock);
		return;
	}

accept_again:
	// Accept incomming connection
	remote_addr_len = sizeof(struct sockaddr_in);
	if ((sock = accept(usock, (struct sockaddr*)&remote_addr, &remote_addr_len)) == -1)
	{
		fprintf(stderr, "Error in accept(): %d (%s)\n", errno, strerror(errno));
		close(usock);
		return;
	}
	printf("%s(): Connected from: %s:%hu (sock=%d) remote_addr_len=%d\n", __func__, inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port), sock, remote_addr_len);
	
	prm = malloc(sizeof(*prm));
	prm->sock = sock;
	memcpy(&prm->remote_addr, &remote_addr, remote_addr_len);
	prm->remote_addr_len = remote_addr_len;

	// It looks like pthread_create() instead of start_task() doesn't mess up things - faulty behavior is the same.
	// But we will need to fix things with start_task() first (!)
	// There are similar problems with telnet-srv.
	// Is it an inherent problem with the situation when two task recently scheduled go to sleep (wait)? But there's an idle_task then...
	// Time-sharing is not an issue (happens without it too). With both tasks being deleted from schedule queue, which becomes empty?
	pthread_create(&tid, NULL, accept_handler, prm);
//	start_task(accept_handler, 1, OPT_TIMESHARE, prm);
	
	goto	accept_again;
}

void	app_entry()
{
//	start_task(http_srv_main, 1, OPT_TIMESHARE, NULL);
	http_srv_main(NULL);

	printf("%s(): telnet_main() returned, halting\r\n", __func__);

//	After start_task() this code will never be reached.
	for (;;)
		;
}

