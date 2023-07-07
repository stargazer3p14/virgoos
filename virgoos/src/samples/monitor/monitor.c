/*
 *	monitor.c
 *
 *	September OS sample monitor program. This is a generic monitor engine.
 *	A separate frond-end connects it to console (PC keyboard+terminal), telnet or UART
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "keyboard.h"
#include "memman.h"
#include "io.h"
#include "inet.h"
#include "socket.h"
#include <stdarg.h>

//#define	serial_printf	printf

#define	MAX_MONITOR_CMD_SIZE	256
#define	MAX_MONITOR_LINE	1024
#define	SEPTOS_VERSION	"1.0"

extern dword   current_stack_top;


static void	cmd_version(char *cmd_line);
static void	cmd_mount(char *cmd_line);
static void	cmd_pci(char *cmd_line);
static void	cmd_mem(char *cmd_line);
static void	cmd_ip(char *cmd_line);
static void	cmd_ls(char *cmd_line);
static void	cmd_cat(char *cmd_line);
static void	cmd_cp(char *cmd_line);
static void	cmd_mv(char *cmd_line);
static void	cmd_rm(char *cmd_line);
static void     cmd_date(char *cmd_line);
static void	cmd_sleep(char *cmd_line);
static void	cmd_udelay(char *cmd_line);
static void	cmd_sync(char *cmd_line);
static void	cmd_test_tcp_accept(char *cmdline);
static void	cmd_help(char *cmd_line);

int	monitor_printf(char *fmt, ...);

struct mon_cmd
{
	char	*cmd;
	char	*help;
	void	(*cmd_func)(char *cmdline);
} monitor_cmds[] = 
{
	{"version", "show SeptOS version", cmd_version},
	{"mount", "show mounted disks", cmd_mount},
	{"pci", "show PCI devices information", cmd_pci},
	{"mem", "show memory usage information", cmd_mem},
	{"ip", "show IP addresses information", cmd_ip},
	{"ls", "show filesystem directory contents", cmd_ls},
	{"cat", "show filesystem file's contents", cmd_cat},
	{"cp", "copy one filesystem file to another", cmd_cp},
	{"mv", "rename filesystem file", cmd_mv},
	{"rm", "delete filesystem file", cmd_rm},
	{"date", "display local date and time", cmd_date},
	{"sleep", "sleep N seconds", cmd_sleep},
	{"udelay", "delay N microseconds (busy-poll)", cmd_udelay},
	{"sync", "sync all filesystems data to disks", cmd_sync},
	{"test_tcp_accept", "Test primitive TCP server (against client peer)", cmd_test_tcp_accept},
	{"help", "show help summary", cmd_help}
};

void	*m;

static void	sock_report_error(char *msg, int code)
{
	serial_printf("%s(): %s (%d)\r\n", __func__, msg, code);
}

static void	cmd_version(char *cmd_line)
{
	monitor_printf("September OS version %s. Copyright (c) Daniel Drubin 2007-2010. All rights reserved\n", SEPTOS_VERSION);

	m = malloc(100);
	monitor_printf("m=%08X\n", m);
}

extern struct fs       filesystems[MAX_FILESYSTEMS];

static void	cmd_mount(char *cmd_line)
{
	int	i;
	int	mounts = 0;

	for (i = 0; i < MAX_FILESYSTEMS; ++i)
	{
		 if (FS_NONE == filesystems[i].fs_type)
			continue;
		monitor_printf("Filesystem slot #%d: type %s, disk number %d, partition number %d, start %lu mount point \"%s\"\n",
			i, FS_EXT2 == filesystems[i].fs_type ? "ext2" : "fat", filesystems[i].disk_num, filesystems[i].part_num, (unsigned long)filesystems[i].start_offs, filesystems[i].mount_point);
		++mounts;
	}

	if (!mounts)
		monitor_printf("no mounts\n");
}

static void	cmd_ls(char *cmd_line)
{
	char	cmd[16];
	char	path[MAX_PATH];
	int	i;
	char	mount_point[MAX_PATH];
	char	*p;
	DIR	*d;
	struct dirent	*dent;
	struct stat	stat_buf;
	int	rv;
	char	fname[MAX_PATH];

	sscanf(cmd_line, " ls %s ", path);

	if (path[0] == '\0')
		return;

	if (path[strlen(path)-1] != '/')
	{
		// stat just this file and that's it
		rv = stat(path, &stat_buf);
		if (rv != 0)
			monitor_printf("<stat error>\n");
		else
		{
			monitor_printf("%c", S_ISDIR(stat_buf.st_mode) ? 'd' : '-');	
			monitor_printf("%c", (stat_buf.st_mode & S_IRUSR) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWUSR) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXUSR) ? 'x' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IRGRP) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWGRP) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXGRP) ? 'x' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IROTH) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWOTH) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXOTH) ? 'x' : '-'); 
			monitor_printf("\t%u:%u\t%u", stat_buf.st_uid, stat_buf.st_gid, stat_buf.st_size);
			// TODO: print time in sensible format
			monitor_printf("\n");
		}
		return;
	}

	// Stat directory
	d = opendir(path);
	if (NULL == d)
	{
		monitor_printf("Can't open %s, error = %d\n", path, errno);
		return;
	}
	while (1)
	{
		dent = readdir(d);
		if (NULL == dent)
			break;
		monitor_printf("\t%s\t", dent->d_name);
		sprintf(fname, "%s%s", path, dent->d_name);		// '/' is the last character of path[]
		rv = stat(fname, &stat_buf);
		if (rv != 0)
			monitor_printf("<stat error>\n");
		else
		{
			monitor_printf("%c", S_ISDIR(stat_buf.st_mode) ? 'd' : '-');	
			monitor_printf("%c", (stat_buf.st_mode & S_IRUSR) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWUSR) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXUSR) ? 'x' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IRGRP) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWGRP) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXGRP) ? 'x' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IROTH) ? 'r' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IWOTH) ? 'w' : '-'); 
			monitor_printf("%c", (stat_buf.st_mode & S_IXOTH) ? 'x' : '-'); 
			monitor_printf("\t%u:%u\t%u", stat_buf.st_uid, stat_buf.st_gid, stat_buf.st_size);
			// TODO: print time in sensible format
			monitor_printf("\n");
		}
	}
	closedir(d);
}

static void	cmd_cat(char *cmd_line)
{
	char	cmd[16];
	char	path[MAX_PATH];
	int	i;
	char	mount_point[MAX_PATH];
	char	*p;
	int	fd;
	unsigned char	buf[0x8000];
	ssize_t	sz;
	ssize_t	total_read = 0;

	sscanf(cmd_line, " cat %s ", path);

	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		monitor_printf("Error opening file: %s\n", strerror(errno));
		return;
	}
	while ((sz = read(fd, buf, sizeof(buf))) > 0)
	{
		monitor_printf("read() returned %d\n", sz);
		for (i = 0; i < sz; ++i)
			serial_printf("%c", buf[i]);
		total_read += sz;
	}
	serial_printf("\n");
	if (sz < 0)
		monitor_printf("Error reading file: %s\n", strerror(errno));
	monitor_printf("Total read: %d bytes\n", total_read);
	close(fd);
}

static void	cmd_cp(char *cmd_line)
{
	char	cmd[16];
	char	path[MAX_PATH], dest_path[MAX_PATH];
	int	i;
	char	mount_point[MAX_PATH], dest_mount_point[MAX_PATH];
	char	*p, *dp;
	int	fd, dest_fd;
	unsigned char	buf[0x8000];
	ssize_t	sz, wsz;
	ssize_t	total_read = 0;

	sscanf(cmd_line, " cp %s %s ", path, dest_path);

	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		monitor_printf("Error opening source file: %s\n", strerror(errno));
		return;
	}

	dest_fd = open(dest_path, O_RDWR); 
	if (dest_fd < 0)
	{
		monitor_printf("Error opening dest file: %s\n", strerror(errno));
		close(fd);
		return;
	}

	while ((sz = read(fd, buf, sizeof(buf))) > 0)
	{
		monitor_printf("read() returned %d\n", sz);
		if (sz < 0)
		{
			monitor_printf("Error reading source file: %s\n", strerror(errno));
			break;
		}
		total_read += sz;
		wsz = write(dest_fd, buf, sz);
		if (wsz < 0)
		{
			monitor_printf("Error writing destination file: %s\n", strerror(errno));
			break;
		}
		monitor_printf("write() returned %d\n", wsz);
	}
	serial_printf("\n");
to_ret:
	monitor_printf("Total copied: %d bytes\n", total_read);
	close(fd);
	close(dest_fd);
}

static void	cmd_mv(char *cmd_line)
{
	char	cmd[16];
	char	path[MAX_PATH], dest_path[MAX_PATH];
	int	rv;

	sscanf(cmd_line, " mv %s %s ", path, dest_path);
	rv = rename(path, dest_path);
	if (rv != 0)
		monitor_printf("\tError moving file: %s\n", strerror(errno));
	else
		monitor_printf("\tOk\n");
}

static void	cmd_rm(char *cmd_line)
{
	char	cmd[16];
	char	path[MAX_PATH];
	int	rv;

	sscanf(cmd_line, " rm %s ", path);
	rv = unlink(path);
	if (rv != 0)
		monitor_printf("\tError deleting file: %s\n", strerror(errno));
	else
		monitor_printf("\tOk\n");
}

static void     cmd_date(char *cmd_line)
{
	time_t	cur_time = time(NULL);

	monitor_printf("%u -- %s\n", cur_time, ctime(&cur_time));
}

static void	cmd_pci(char *cmd_line)
{
	free(m);
	monitor_printf("PCI output stub. See serial output\n");
}

static void	cmd_sleep(char *cmd_line)
{
	unsigned	sec;

	if (sscanf (cmd_line, " sleep %u ", &sec) != 1)
		monitor_printf("Bad or missing number of seconds\n");
	else
		monitor_printf("sleep() -> %u\n", sleep(sec));
}

static void	cmd_udelay(char *cmd_line)
{
	unsigned	usec;

	if (sscanf (cmd_line, " udelay %u ", &usec) != 1)
		monitor_printf("Bad or missing number of microseconds\n");
	else
	{
		monitor_printf("udelay(%u) [%u:%u]...", usec, calibrated_udelay_count1, calibrated_udelay_count2);
		udelay(usec);
		monitor_printf("\n");
	}
}

static void	cmd_sync(char *cmd_line)
{
	sync();
}

extern dword	dyn_mem_start, dyn_mem_size;

static void	cmd_mem(char *cmd_line)
{
	struct  block_hdr	*p;
	dword	dyn_mem_end = dyn_mem_start + dyn_mem_size;
	unsigned long	mem_total = 0, mem_used = 0;
	int	blocks_count = 0, blocks_used = 0;

	for (p = (BLOCK_HDR*)dyn_mem_start; (dword)p < dyn_mem_end; *(dword*)&p += sizeof(BLOCK_HDR) + p->size)
	{
		mem_total += sizeof(BLOCK_HDR) + p->size;
		++blocks_count;
		if (p->flags & BLOCK_ALLOCATED)
		{
			mem_used +=  sizeof(BLOCK_HDR) + p->size;
			++blocks_used;
		}
	}

	monitor_printf("Total dynamic memory blocks: %d\n"
			"Total dynamic memory size: %u\n"
			"Used dynamic memory blocks: %d\n"
			"Used dynamic memory size: %lu\n", blocks_count, mem_total, blocks_used, mem_used);
	monitor_printf("Task manager uses stack of %u\n", STACK_START - current_stack_top);
}

extern struct net_if	net_interfaces[MAX_NET_INTERFACES];

static void	cmd_ip(char *cmd_line)
{
	int	i;
	int	net_ifs = 0;

	for (i = 0; i < MAX_NET_INTERFACES; ++i)
	{
		if (net_interfaces[i].eth_dev != NULL)
		{
			monitor_printf("Network interface #%d: ip=%d.%d.%d.%d mask=%d.%d.%d.%d hw=%02X:%02X:%02X:%02X:%02X:%02X\n", i, 
				(int)net_interfaces[i].ip_addr[0], (int)net_interfaces[i].ip_addr[1], (int)net_interfaces[i].ip_addr[2], (int)net_interfaces[i].ip_addr[3],
				(int)net_interfaces[i].mask[0], (int)net_interfaces[i].mask[1], (int)net_interfaces[i].mask[2], (int)net_interfaces[i].mask[3],
				(unsigned)net_interfaces[i].eth_dev->addr[0], (unsigned)net_interfaces[i].eth_dev->addr[1], (unsigned)net_interfaces[i].eth_dev->addr[2], (unsigned)net_interfaces[i].eth_dev->addr[3], (unsigned)net_interfaces[i].eth_dev->addr[4], (unsigned)net_interfaces[i].eth_dev->addr[5]);

		}
		++net_ifs;
	}

	if (!net_ifs)
		monitor_printf("no network interfaces\n");
}


/*
 *	TCP accept/communication test
 *	-----------------------------
 *
 *	This test sends and receives TCP packets to/from destination IP address. It executes the following functionality (success tests):
 *
 *		1) Create socket of type SOCK_STREAM and protocol IPPROTO_TCP
 *		2) Bind it to default IP address configured in the system (default_ipaddr)
 *		3) Make it listen
 *		4) Let it accept incomming connections
 *		5) Send a test packet to IP address that connected
 *		6) Receive a response packet from peer. Continue to step 5 until "exit" is received
 *		7) Close the socket
 *
 *	The following sockets calls are invoked: socket(), bind(), listen(), accept(), send(), recv(), inet_addr(), close()
 */
static void	cmd_test_tcp_accept(char *cmdline)
{
	int	sock, acc_sock;
	char	buf[256];
	int	rv;
	struct sockaddr_in	my_addr;
	struct sockaddr_in	peer_addr;
	socklen_t	peer_addr_len;
	unsigned short	my_port = 0x2325;
	int	count = 1;
	char	*default_ipaddr = DEF_IP_ADDR_STR;
	
	serial_printf("%s\n", __func__);
	
	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		sock_report_error("Error creating socket", errno);
		return;
	}
	serial_printf("Created socket, sock=%d\n", sock);
	
	// Bind socket to configured IP address
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_port);
	my_addr.sin_addr.s_addr = inet_addr((char*)default_ipaddr);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (rv < 0)
	{
		sock_report_error("Error binding socket", errno);
		return;
	}
	serial_printf("Socket bound, rv=%d\n", rv);
	
	// Make the socket listen
	rv = listen(sock, 5);
	if (rv < 0)
	{
		sock_report_error("Error making socket listen", errno);
		return;
	}
	serial_printf("Socket is listening, rv=%d\n", rv);

	peer_addr_len = sizeof(peer_addr);
	acc_sock = accept(sock, (struct sockaddr*)&peer_addr, &peer_addr_len);
	if (acc_sock < 0)
	{
		sock_report_error("Error accepting connection", errno);
		return;
	}
	serial_printf("Socket is connected, acc_sock=%d, remote=%s:%hu peer_addr_len=%lu\n", acc_sock, inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port), peer_addr_len);

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
		serial_printf("Test data received: %s, rv=%d\n", buf, rv);
	} while(strcmp(buf, "exit") != 0);
		
	rv = close(sock);
	if (rv < 0)
	{
		sock_report_error("Error closing socket", errno);
		return;
	}
	serial_printf("Socket closed, rv=%d\n", rv);
	rv = close(acc_sock);
	if (rv < 0)
	{
		sock_report_error("Error closing accepted socket", errno);
		return;
	}
	serial_printf("Socket (accepted) closed, rv=%d\n", rv);
	      
	sock_report_error("PASS", 0);	
}

static void	cmd_help(char *cmd_line)
{
	char	cmd_to_help[MAX_MONITOR_LINE];
	int	i;
	int	this_command = 0;

	if (sscanf(cmd_line, "help %s ", cmd_to_help) == 1)
		this_command = 1;
	
	for (i = 0; i < sizeof(monitor_cmds) / sizeof(monitor_cmds[0]); ++i)
	{
		if (this_command && strcmp(cmd_to_help, monitor_cmds[i].cmd) != 0)
			continue;

		monitor_printf("\t%s\t\t%s\n", monitor_cmds[i].cmd, monitor_cmds[i].help);
	}
}

void	run_cmd(char *cmdline)
{
	int	i;
	char	cmd[MAX_MONITOR_CMD_SIZE];
	char	*p;

	p = strchr(cmdline, '\n');
	if (p != NULL)
		*p = '\0';
	p = strchr(cmdline, '\r');
	if (p != NULL)
		*p = '\0';

	sscanf(cmdline, " %s ", cmd);

	for (i = 0; i < sizeof(monitor_cmds) / sizeof(monitor_cmds[0]); ++i)
	{
		if (0 == strcmp(cmd, monitor_cmds[i].cmd))
		{
			monitor_cmds[i].cmd_func(cmdline);
			break;
		}
	}
	if (i == sizeof(monitor_cmds) / sizeof(monitor_cmds[0]))
		monitor_printf("Bad command or file name :-)\n");
}


