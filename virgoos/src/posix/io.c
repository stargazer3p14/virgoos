/*****************************************
*
*	io.c
*
*	POSIX I/O implementation
*
*****************************************/

#include "config.h"
#include "sosdef.h"
#include "io.h"
#include "ide.h"
#include "socket.h"

#ifdef SOCKETS
extern struct socket	*psockets;
#endif

// File descriptors are indexes into this structure
struct file	*files;

// select() events multiplexer global queue
EVENTS_SEL_Q *sel_q_hd = NULL;

// Init disks structure array with implementations. This is platform- and system-specific
struct disk	disks[MAX_DISKS];
int	exist_disk_num;
 
struct fs	filesystems[MAX_FILESYSTEMS];

// Returns file type according to its name
// (!) Returned file type doesn't mean that the file/device exists!
static int	get_file_type(const char *path)
{
	if (0 == strncmp(path, "/dev/", sizeof("/dev/")-1))
		return	FILE_TYPE_DEVICE;
	return	FILE_TYPE_FS_FILE;
}

// Returns filesystem found by name (mount_point in naming scheme "/mount_point/path/file")
struct fs	*get_fs(const char *path)
{
	char	mount_point[MAX_PATH];
	const char	*p;
	int	i;

	if (path[0] != '/')
		return	NULL;
	if ((p = strchr(path+1, '/')) == NULL)
		return	NULL;
	memcpy(mount_point, path, p-path);
	mount_point[p-path] = '\0';
	for (i = 0; i < MAX_FILESYSTEMS; ++i)
		if (0 == strcmp(mount_point, filesystems[i].mount_point))
			return	&filesystems[i];

	return	NULL;
}


int	open(const char *pathname, int flags, ...)
{
	int	fd;
	struct reg_file	*reg_file;
	struct dev_file	*dev_file;
	struct fs	*fs;
	int	rv;
	void	*fs_entry;
	char	*p;
	int	dev_id;

	errno = 0;

	for (fd = 0; fd < MAX_FILES; ++fd)
		if (FILE_TYPE_NONE == files[fd].file_type)
			break;

	if (MAX_FILES == fd)
	{
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	-1;
	}

	files[fd].file_type = get_file_type(pathname);
	files[fd].attr = flags;

	switch (files[fd].file_type)
	{
	default:
		break;

	case FILE_TYPE_FS_FILE:
		fs = get_fs(pathname);
		if (NULL == fs)
		{
			files[fd].file_type = FILE_TYPE_NONE;
			errno = ENOENT;
			return	-1;
		}
		// call FS's open() procedure
		p = strchr(pathname+1, '/');
		// If O_TRUNC was specified, unlink() file unconditionally and ignore possible error
		if (flags & O_TRUNC)
			fs->file_unlink(fs, p);
		rv = fs->file_open(fs, p, flags, &fs_entry);
		if (rv != 0)
		{
			// If opening with O_CREAT or for writing, create the file
			if (flags & (O_CREAT | O_WRONLY))
				rv = fs->file_creat(fs, p, 0777, &fs_entry);
			if (rv != 0)
			{
				files[fd].file_type = FILE_TYPE_NONE;
				return	-1;
			}
		}
		reg_file = calloc(1, sizeof(struct reg_file));
		if (NULL == reg_file)
		{
			files[fd].file_type = FILE_TYPE_NONE;
			errno = ENOMEM;
			fs->file_close(fs, fs_entry);
			return	-1;
		}
		reg_file->fs = fs;
		reg_file->fs_entry = fs_entry;
		// TODO: mode semantics
		reg_file->flags = flags;
		reg_file->open_count = 1;
		files[fd].file_struct = reg_file;
		break;

	case FILE_TYPE_DEVICE:
		dev_id = dev_name_to_id(pathname);
		if (dev_id == -1)
		{
			errno = ENODEV;
			return	-1;
		}
		if (open_drv(dev_id) != 0)
			return	-1;
		dev_file = calloc(1, sizeof(struct dev_file));
		if (dev_file == NULL)
		{
			files[fd].file_type = FILE_TYPE_NONE;
			errno = ENOMEM;
			return	-1;
		}
		dev_file->dev_id = dev_id;
		files[fd].file_struct = dev_file;
		break;
	}

	return	fd;
}

int	creat(const char *pathname, mode_t mode)
{
	return	open(pathname, mode | O_CREAT);
}

ssize_t	read(int fd, void *buf, size_t count)
{
	ssize_t	sz;
	struct reg_file	*rf;
	struct dev_file	*df;
	fd_set	rfds;

	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}

	// If fd is a socket, return recv()
//	if (fd >= FIRST_SOCKET_DESCR && fd < FIRST_SOCKET_DESCR + MAX_SOCKETS)
//		return	recv(fd, buf, count, 0);

	if (files[fd].file_type == FILE_TYPE_DEVICE)
	{
		df = files[fd].file_struct;
		return	read_drv(df->dev_id, buf, count);
	}
#ifdef SOCKETS
	else if (files[fd].file_type == FILE_TYPE_SOCKET)
	{
		return  recv(fd, buf, count, 0);
	}
#endif
	else if (files[fd].file_type == FILE_TYPE_PIPE)
	{
		struct pipe_file	*pf, *peer_pf;
		size_t	n;
		size_t	nleft;

		pf = files[fd].file_struct;
		if (pf->write_side)
		{
			errno = EBADF;
			return	-1;
		}

		// If peer fd of this one is not its open pipe peer, we must
		// return EOF (size 0)
		if (files[pf->peer_fd].file_type != FILE_TYPE_PIPE)
		{
			errno = EPIPE;
			return	-1;
		}
		peer_pf = files[pf->peer_fd].file_struct;
		if (!peer_pf->write_side || peer_pf->peer_fd != fd)
		{
			return	0;
		}

		n = pf->tail + (pf->tail < pf->head) * pf->buf_size - pf->head;
		if (n < count)
			count = n;

		// If pipe is empty wait on select() for it
		if (!n)
		{
			if (files[fd].status & FD_STAT_NONBLOCK)
			{
				errno = EAGAIN;
				return	-1;
			}
			// Mark this pipe as "can't read"
			files[fd].status &= ~FD_STAT_MAYREAD;
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			select(fd+1, &rfds, NULL, NULL, NULL);
		}

		nleft = pf->buf_size - pf->head;
		if (count < nleft)
		{
			memcpy(buf, (char*)pf->buf + pf->head, count);
			pf->head += count;
		}
		else
		{
			memcpy(buf, (char*)pf->buf + pf->head, nleft);
			memcpy((char*)buf + nleft, pf->buf, count - nleft);
			pf->head = (pf->head + count) % pf->buf_size;
		}

		// If caller read something (count != 0) and pipe was full, mark peer non-full and send write event
		if (count && !(files[pf->peer_fd].status & FD_STAT_MAYWRITE))
		{
			// Write events (notified sockets) are the same as read events, but offset by MAX_FILES
			files[pf->peer_fd].status |= FD_STAT_MAYWRITE;
			send_event_sel(&sel_q_hd, MAX_FILES * 2, pf->peer_fd + MAX_FILES);
		}
	}
	else if (files[fd].file_type == FILE_TYPE_FIFO)
	{
		struct fifo_file	*ff;
		size_t	n;
		size_t	nleft;

		ff = files[fd].file_struct;

		n = ff->tail + (ff->tail < ff->head) * ff->buf_size - ff->head;
		if (n < count)
			count = n;

		// If FIFO is empty wait on select() for it
		if (!n)
		{
			if (files[fd].status & FD_STAT_NONBLOCK)
			{
				errno = EAGAIN;
				return	-1;
			}
			// Mark this FIFO as "can't read"
			files[fd].status &= ~FD_STAT_MAYREAD;
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			select(fd+1, &rfds, NULL, NULL, NULL);
		}

		nleft = ff->buf_size - ff->head;
		if (count < nleft)
		{
			memcpy(buf, (char*)ff->buf + ff->head, count);
			ff->head += count;
		}
		else
		{
			memcpy(buf, (char*)ff->buf + ff->head, nleft);
			memcpy((char*)buf + nleft, ff->buf, count - nleft);
			ff->head = (ff->head + count) % ff->buf_size;
		}

		// If caller read something (count != 0) and FIFO was full, mark it non-full and send write event
		if (count && !(files[fd].status & FD_STAT_MAYWRITE))
		{
			// Write events (notified sockets) are the same as read events, but offset by MAX_FILES
			files[fd].status |= FD_STAT_MAYWRITE;
			send_event_sel(&sel_q_hd, MAX_FILES * 2, fd + MAX_FILES);
		}
	}
	else if (files[fd].file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return	-1;
	}

	// Call FS's read() procedure
	rf = files[fd].file_struct;
	if (rf == NULL)
	{
		errno = EBADF;
		return	-1;
	}
	sz = rf->fs->file_read(rf->fs, rf->fs_entry, buf, rf->pos, count);
	if (sz > 0)
		rf->pos += sz;
	return	sz;
}

ssize_t	write(int fd, const void *buf, size_t count)
{
	ssize_t	sz;
	struct reg_file	*rf;
	struct dev_file	*df;
	fd_set	wfds;

	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}

	// If fd is a socket, return send()
//	if (fd >= FIRST_SOCKET_DESCR && fd < FIRST_SOCKET_DESCR + MAX_SOCKETS)
//		return	send(fd, buf, count, 0);

	if (files[fd].file_type == FILE_TYPE_DEVICE)
	{
		df = files[fd].file_struct;
		return	write_drv(df->dev_id, buf, count);
	}
#ifdef SOCKETS
	if (files[fd].file_type == FILE_TYPE_SOCKET)
	{
		return	send(fd, buf, count, 0);
	}
#endif
	else if (files[fd].file_type == FILE_TYPE_PIPE)
	{
		struct pipe_file	*pf, *peer_pf;
		size_t	n;
		size_t	nleft;

		pf = files[fd].file_struct;
		if (!pf->write_side)
		{
			errno = EBADF;
			return	-1;
		}
		// If peer fd of this one is not its open pipe peer, we must
		// return EPIPE
		if (files[pf->peer_fd].file_type != FILE_TYPE_PIPE)
		{
			errno = EPIPE;
			return	-1;
		}
		peer_pf = files[pf->peer_fd].file_struct;
		if (peer_pf->write_side || peer_pf->peer_fd != fd)
		{
			errno = EPIPE;
			return	-1;
		}

		// Go on perform pipe write
		n = pf->buf_size - (pf->tail + (pf->tail < pf->head) * pf->buf_size - pf->head);
		if (n < count)
			count = n;

		// If pipe is full wait on select() for it
		if (!n)
		{
			if (files[fd].status & FD_STAT_NONBLOCK)
			{
				errno = EAGAIN;
				return	-1;
			}
			// Mark this pipe as "can't write"
			files[fd].status &= ~FD_STAT_MAYWRITE;
			FD_ZERO(&wfds);
			FD_SET(fd, &wfds);
			select(fd+1, NULL, &wfds, NULL, NULL);
		}
		nleft = pf->buf_size - pf->tail;
		if (count < nleft)
		{
			memcpy((char*)pf->buf + pf->tail, buf, count);
			pf->tail += count;
		}
		else
		{
			memcpy((char*)pf->buf + pf->tail, buf, nleft);
			memcpy(pf->buf, (char*)buf + nleft, count - nleft);
			pf->tail = (pf->head + count) % pf->buf_size;
		}

		// If caller wrote something (count != 0) and pipe was empty, mark peer non-empty and send read event
		if (count && !(files[pf->peer_fd].status & FD_STAT_MAYREAD))
		{
			// Rread event
			files[pf->peer_fd].status |= FD_STAT_MAYREAD;
			send_event_sel(&sel_q_hd, MAX_FILES * 2, pf->peer_fd);
		}
	}
	else if (files[fd].file_type == FILE_TYPE_FIFO)
	{
		struct fifo_file	*ff;
		size_t	n;
		size_t	nleft;

		ff = files[fd].file_struct;
		n = ff->buf_size - (ff->tail + (ff->tail < ff->head) * ff->buf_size - ff->head);
		if (n < count)
			count = n;

		// If FIFO is full wait on select() for it
		if (!n)
		{
			if (files[fd].status & FD_STAT_NONBLOCK)
			{
				errno = EAGAIN;
				return	-1;
			}
			// Mark this FIFO as "can't write"
			files[fd].status &= ~FD_STAT_MAYWRITE;
			FD_ZERO(&wfds);
			FD_SET(fd, &wfds);
			select(fd+1, NULL, &wfds, NULL, NULL);
		}
		nleft = ff->buf_size - ff->tail;
		if (count < nleft)
		{
			memcpy((char*)ff->buf + ff->tail, buf, count);
			ff->tail += count;
		}
		else
		{
			memcpy((char*)ff->buf + ff->tail, buf, nleft);
			memcpy(ff->buf, (char*)buf + nleft, count - nleft);
			ff->tail = (ff->head + count) % ff->buf_size;
		}

		// If caller wrote something (count != 0) and FIFO was empty, mark it non-empty and send read event
		if (count && !(files[fd].status & FD_STAT_MAYREAD))
		{
			// Read event
			files[fd].status |= FD_STAT_MAYREAD;
			send_event_sel(&sel_q_hd, MAX_FILES * 2, fd);
		}
	}
	else if (files[fd].file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return	-1;
	}

	// Call FS's write() procedure
	rf = (struct reg_file*)files[fd].file_struct;
	if (rf == NULL)
	{
		errno = EBADF;
		return	-1;
	}
	sz = rf->fs->file_write(rf->fs, rf->fs_entry, buf, rf->pos, count);
	if (sz > 0)
		rf->pos += sz;
	return	sz;
}

off_t	lseek(int fd, off_t offset, int whence)
{
	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (files[fd].file_type != FILE_TYPE_FS_FILE)
	{
		errno = EINVAL;
		return	-1;
	}
	switch (whence)
	{
		default:
			errno = EINVAL;
			return	-1;
		case SEEK_SET:
			((struct reg_file*)(files[fd].file_struct))->pos = offset;
			break;
		case SEEK_CUR:
			((struct reg_file*)(files[fd].file_struct))->pos += offset;
			break;
		case SEEK_END:
			// TODO: advance file pointer behind EOF. Check for which files it is valid
			break;
	}

	return	0;
}


/*
int	fcntl(int fd, int cmd, ...)
{
	return	0;
}
*/

int	close(int fd)
{
	struct reg_file	*rf;
	struct dev_file	*df;
	struct sock_file	*sf;
	
	errno = 0;
//	if (fd >= FIRST_SOCKET_DESCR && fd < FIRST_SOCKET_DESCR + MAX_SOCKETS)
//		return	close_socket(fd);

	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (NULL == files[fd].file_struct)
	{
		errno = EBADF;
		return	-1;
	}

	if (files[fd].file_type == FILE_TYPE_DEVICE)
	{
		df = files[fd].file_struct;
		if (close_drv(df->dev_id) != 0)
			return -1;				// errno is already set
		free(df);
		files[fd].file_struct = NULL;
		files[fd].file_type = FILE_TYPE_NONE;
		return	0;
	}
#ifdef SOCKETS
	else if (files[fd].file_type == FILE_TYPE_SOCKET)
	{
		sf = files[fd].file_struct;
		if (close_socket(sf->sock) != 0)
			return -1;				// errno is already set
		free(sf);
		files[fd].file_struct = NULL;
		files[fd].file_type = FILE_TYPE_NONE;
		return	0;
	}
#endif
	else if (files[fd].file_type == FILE_TYPE_FS_FILE)
	{
		rf = (struct reg_file*)files[fd].file_struct;
		if (--rf->open_count == 0)
		{
			rf->fs->file_close(rf->fs, rf->fs_entry);
			free(rf);
		}
		files[fd].file_struct = NULL;
		files[fd].file_type = FILE_TYPE_NONE;
		return	0;
	}

	errno = EBADF;
	return	-1;
}

int ioctl(int fd, int request, ...)
{
	struct dev_file	*df;
	fd_set	rfds;
	va_list	argp;

	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}

	if (files[fd].file_type == FILE_TYPE_DEVICE)
	{
		int	rv;

		df = files[fd].file_struct;
		va_start(argp, request);
//		rv = ioctl_drv(df->dev_id, request, argp);
		// Adopted from ioctl_drv()
		if (driver_entries[DEV(df->dev_id)]->ioctl != NULL)
			rv = driver_entries[DEV(df->dev_id)]->ioctl(SUBDEV(df->dev_id), request, argp);
		else
		{
			errno = EINVAL;
			rv = -1;
		}

		va_end(argp);
		return	rv;
	}

	errno = EINVAL;	// Invalid operation for non-device file descriptors
	return	-1;
}


/*
 *  fcntl()
 *      Generic control on file descriptor
 *
 *  Parameters:
 *      sock - socket descriptor
 *      cmd - command (only F_SETFL and F_GETFL are supported)
 *      arg - argument to command (only O_NONBLOCK is supported)
 */
int fcntl(int fd, int cmd, ...)
{
  	int	rv;
	long arg;
	va_list	argp;
	
    // Parameters validation
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EINVAL;
		return	-1;
	}
//	if (0 == psockets[sock].protocol)
	if (FILE_TYPE_NONE == files[fd].file_type)
	{
		errno = EBADF;
		return	-1;
	}
	va_start(argp, cmd);
	arg = (long)va_arg(argp, long);
	va_end(argp);
	
	if (F_GETFL == cmd)
	{
		errno = 0;
		rv = 0;
//		if ((psockets[sock].attrib & ~SOCK_ATTR_NONBLOCKING) != 0)
		if ((files[fd].status & FD_STAT_NONBLOCK) != 0)
		  	rv = O_NONBLOCK;
		return	rv;
	}
	else if (F_SETFL == cmd)
	{
	 	errno = 0;
		rv = 0;
		if ((arg & O_NONBLOCK) != 0)
//		  	psockets[sock].attrib |= SOCK_ATTR_NONBLOCKING;
		  	files[fd].status |= FD_STAT_NONBLOCK;
		else
//		  	psockets[sock].attrib & ~SOCK_ATTR_NONBLOCKING;
		  	files[fd].status &= ~FD_STAT_NONBLOCK;
		return	0;
	}
			 
	errno = 0;
	return	0;
}

void	sync(void)
{
	int	i;

	for (i = 0; i < MAX_FILESYSTEMS; ++i)
		if (filesystems[i].mount_point != NULL && filesystems[i].sync != NULL)
			filesystems[i].sync(&filesystems[i]);
	return;
}

int	fsync(int fd)
{
	errno = 0;
	return	0;
}

int	fdatasync(int fd)
{
	errno = 0;
	return	0;
}

int	unlink(const char *pathname)
{
	struct fs	*fs;
	char	*p;

	errno = 0;

	fs = get_fs(pathname);
	if (NULL == fs)
	{
		errno = ENOENT;
		return	-1;
	}
	// call FS's unlink() procedure
	p = strchr(pathname+1, '/');
	return fs->file_unlink(fs, p);
}

int	dup(int oldfd)
{
	int	fd;

	errno = 0;

	// Check oldfd
	if (oldfd < 0 || oldfd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (NULL == files[oldfd].file_struct)
	{
		errno = EBADF;
		return	-1;
	}

	// Create a copy of oldfd
	for (fd = 0; fd < MAX_FILES; ++fd)
		if (FILE_TYPE_NONE == files[fd].file_type)
			break;

	if (MAX_FILES == fd)
	{
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	-1;
	}
	memcpy(&files[fd], &files[oldfd], sizeof(struct file));
	if (FILE_TYPE_FS_FILE == files[fd].file_type)
	{
		struct reg_file	*rf = files[fd].file_struct;
		++rf->open_count;
	}
	return	0;
}

int	dup2(int oldfd, int newfd)
{
	errno = 0;

	// Check oldfd
	if (oldfd < 0 || oldfd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (NULL == files[oldfd].file_struct)
	{
		errno = EBADF;
		return	-1;
	}

	// Check newfd
	if (oldfd < 0 || oldfd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (NULL != files[oldfd].file_struct)
	{
		struct reg_file	*rf = files[newfd].file_struct;
		struct fs	*fs;

		if (rf != NULL)
		{
			fs = rf->fs;
			if (fs != NULL)
				fs->file_close(fs, rf->fs_entry);
		}
	}

	memcpy(&files[newfd], &files[oldfd], sizeof(struct file));
	if (FILE_TYPE_FS_FILE == files[newfd].file_type)
	{
		struct reg_file	*rf = files[newfd].file_struct;
		++rf->open_count;
	}
	return	0;
}

int	link(const char *oldpath, const char *newpath)
{
	errno = 0;
	return	0;
}

int	mknod(const char *pathname, mode_t mode, dev_t dev)
{
	errno = 0;
	return	0;
}

int	stat(const char *path, struct stat *buf)
{
	struct fs	*fs;
	char	*p;

	errno = 0;
	fs = get_fs(path);
	if (NULL == fs)
	{
		errno = ENOENT;
		return	-1;
	}
	p = strchr(path+1, '/');
	return	fs->stat(fs, p, buf);
}

int	fstat(int fd, struct stat *buf)
{
	struct fs	*fs;

	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	fs = files[fd].file_struct;
	if (NULL == fs)
	{
		errno = EBADF;
		return	-1;
	}
	
	return	0;
}

int	lstat(const char *path, struct stat *buf)
{
	errno = 0;
	// TODO: handle links
	return	stat(path, buf);
}

struct select_timer_prm
{
	unsigned	*timeout;
//	struct task_q	**sleeping_task;
	EVENTS_SEL_Q	**sel_q_hd;
	EVENTS_SEL_Q	*sel_q;
};

// Notifies timeout
static	void	select_timer_handler(void *prm)
{
	struct select_timer_prm	*p = (struct select_timer_prm*)prm;
	
	*p->timeout = 1;
//	if (*p->sleeping_task != NULL)
//		wake(p->sleeping_task);
	remove_events_sel(p->sel_q_hd, p->sel_q);
	if (p->sel_q->task)
		wake(&p->sel_q->task);
}

/*
 *  select()
 *      File descriptors events multiplexer
 *
 *  Parameters:
 *      nfds - According to specs, maximum socket's descriptor value in all three sets + 1. Should probably contain some
 *	optimization hint for us, but we don't find use for it. In SeptOS implementation this parameter is unused
 *      readfds - set of descriptors watching for read events
 *      writefds - set of descriptors watching for write events
 *      exceptfds - set of descriptors watching for exception events (will reflect connection closure by the peer - TODO)
 *      timeout - timeout waiting for events
 *
 *  Return value:
 *      > 0: Success, number of sockets that received events
 *      0: Timeout
 *      -1: error (errno is set appropriately)
 *
 *  NOTES:
 *      (o) readfds, writefds are modified in place to reflect socket descriptor that got events
 *      (o) Socket waiting for accept() may be polled for read event, socket waiting for connect() may be polled for write event
 *	(o) Meanwhile only socket descriptors are tested
 */
int  select(int  nfds,  fd_set  *readfds,  fd_set  *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	int	i, j, k, l;
	unsigned long	time_out;
	int	rv;
	fd_set	rv_readfds, rv_writefds;
	timer_t	select_timer;
	struct select_timer_prm	timer_prm;
	unsigned select_timed_out;
	EVENTS_SEL_Q *sel_q;
	unsigned long set_prm[FD_SETSIZE * 2];
	struct sock_file	*sf;
	int	sock;
	
serial_printf("%s(): called\n", __func__);
	// Parameters validation
	// TODO: reflect TCP connection closure by the other side in exceptfd's
	if (exceptfds != NULL)
	{
		errno = EINVAL;
		return	-1;
	}
	time_out = timeval_to_ticks(timeout);
	FD_ZERO(&rv_readfds);
	FD_ZERO(&rv_writefds);
	
    // Check current state of quiried sockets.
    // This is also return points in case that events arrived
check_state:
	rv = 0;
	for (i = 0; i < MAX_FILES; ++i)
	{
#if 0
		if (files[i].file_type != FILE_TYPE_SOCKET)
			continue;
#endif
		sf = files[i].file_struct;
		sock = sf->sock;

		//if (FD_ISSET(i, writefds) && (psockets[sock].status & SOCK_STAT_MAYSEND))
		if (FD_ISSET(i, writefds) && files[i].status & FD_STAT_MAYWRITE)
		{
			FD_SET(i, &rv_writefds);
			++rv;
		}
/*
		if (FD_ISSET(i, readfds) && 
			((psockets[sock].attrib & SOCK_ATTR_LISTEN) && (psockets[sock].status & SOCK_STAT_MAYACC) ||
			!(psockets[sock].attrib & SOCK_ATTR_LISTEN) && (psockets[sock].status & SOCK_STAT_HASDATA)))
*/
		if (FD_ISSET(i, readfds) && files[i].status & FD_STAT_MAYREAD) 
		{
			FD_SET(i, &rv_readfds);
			++rv;
		}
	}
	
	// If some sockets have pending events, return them
	if (rv > 0)
	{
		if (writefds != NULL)
			memmove(writefds, &rv_writefds, sizeof(fd_set));
		if (readfds != NULL)
			memmove(readfds, &rv_readfds, sizeof(fd_set));
		errno = 0;
		return	rv;
	}
	
	// Prepare to wait for sockets events
	memset(&set_prm, 0, sizeof(set_prm));
	if (readfds)
	{
#if 0
		for (i = 0; i < MAX_SOCKETS; ++i)
			if (is_event_set(readfds->desc, MAX_SOCKETS, psockets[i].fd))
				set_event(set_prm, MAX_FILES * 2, psockets[i].fd);
#else
		for (i = 0; i < MAX_FILES; ++i)
			if (is_event_set(readfds->desc, MAX_FILES, i))
				set_event(set_prm, MAX_FILES * 2, i);
#endif
	}
	if (writefds)
	{
#if 0
		for (i = 0; i < MAX_SOCKETS; ++i)
			if (is_event_set(writefds->desc, MAX_FILES, psockets[i].fd))
				set_event(set_prm, MAX_FILES * 2, MAX_FILES + psockets[i].fd);
#else
		for (i = 0; i < MAX_FILES; ++i)
			if (is_event_set(writefds->desc, MAX_FILES, i))
				set_event(set_prm, MAX_FILES * 2, MAX_FILES + i);
#endif
	}
	sel_q = new_events_sel(MAX_FILES * 2, set_prm);
	select_timed_out = 0;

	if (time_out != TIMEOUT_INFINITE)
	{
		select_timer.timeout = time_out;
		select_timer.latch = 0;			// Used by timers implementation
		select_timer.resolution = TICKS_PER_SEC;	// How many checks per second - maximum
		select_timer.flags = 0;			// One-shot timer
		select_timer.task_priority = 0;	// No task may mask reporting of this timer
	
		timer_prm.timeout = &select_timed_out;						// Timeout variable to set
		timer_prm.sel_q_hd = &sel_q_hd;
		timer_prm.sel_q = sel_q;
		select_timer.prm = &timer_prm;	// Parameter to callback
		select_timer.callback = select_timer_handler;	// Callback
		install_timer(&select_timer);
	}
	
	wait_events_sel(&sel_q_hd, sel_q);
	
	if (select_timed_out)
	{
		FD_ZERO(writefds);
		FD_ZERO(readfds);
		errno = 0;
		return	0;
	}
	
	// Woke up. There should be only one condition satisfied
	errno = 0;
	goto	check_state;
}

// Clear a specific socket in fd_set structure
void FD_CLR(int fd, fd_set *set)
{
	if (NULL == set)
		return;
//	fd -= FIRST_SOCKET_DESCR;
	set->desc[fd / (sizeof(unsigned long)*CHAR_BIT)] &= ~(0x1UL << fd % (sizeof(unsigned long)*CHAR_BIT));
}

// Check if specific socket is set in fd_set structure
int FD_ISSET(int fd, fd_set *set)
{
	if (NULL == set)
		return 0;
//	fd -= FIRST_SOCKET_DESCR;
	return	(set->desc[fd / (sizeof(unsigned long)*CHAR_BIT)] & 0x1UL << fd % (sizeof(unsigned long)*CHAR_BIT)) != 0;
}

// Set a specific socket in fd_set structure
void FD_SET(int fd, fd_set *set)
{
	if (NULL == set)
		return;
//	fd -= FIRST_SOCKET_DESCR;
	set->desc[fd / (sizeof(unsigned long)*CHAR_BIT)] |= 0x1UL << fd % (sizeof(unsigned long)*CHAR_BIT);
}

// Clear fd_set structure
void FD_ZERO(fd_set *set)
{
	int	i;
    
	if (NULL == set)
		return;
	
	for (i = 0; i < FD_SETSIZE; ++i)
		set->desc[i] = 0;
}

void	init_io(void)
{
	int	disk_num, part_num, fs_num;
	int	max_partitions;
	unsigned char	mbr[DISK_SECTOR_SIZE];
	int	rv;
	struct mbr_part_rec	*mbr_part_rec;
	char	mount_point[16] = "/a";

	files = calloc(sizeof(struct file), MAX_FILES);

	serial_printf("%s(): %d disks found\n", __func__, exist_disk_num);
	
	// Probe partitions and filesystems on disks
	fs_num = 0;
	for (disk_num = 0; disk_num < exist_disk_num && disks[disk_num].read != NULL; ++disk_num)
	{
		max_partitions = MAX_PARTITIONS;
		rv = disks[disk_num].read(&disks[disk_num], 0, mbr, DISK_SECTOR_SIZE);

		// ERROR?
		if (rv != 0)
			continue; 

		// If 1st sector on disk is not MBR, the disk is not partitioned - there is a single partition of unknown type
		if (!(0x55 == mbr[0x1FE] && 0xAA == mbr[0x1FF]))
		{
			max_partitions = 1;

			serial_printf("%s(): disk %d has no MBR - not partitioned\n", __func__, disk_num);
			
			// Mount filesystems if we can. Meanwhile follow a simple scheme: first FS = "/a", second = "/b" etc.
			rv = fat_mount(&filesystems[fs_num], mount_point, disk_num, 0);
			if (0 == rv)
			{
				serial_printf("%s(): disk %d fat_mount() succeeded\n", __func__, disk_num);
				++fs_num;
				++mount_point[1];
				continue;
			}
			serial_printf("%s(): disk %d fat_mount() failed\n", __func__, disk_num);
			
			rv = ext2_mount(&filesystems[fs_num], mount_point, disk_num, 0);
			if (0 == rv)
			{
				serial_printf("%s(): disk %d ext2_mount() succeeded\n", __func__, disk_num);
				++fs_num;
				++mount_point[1];
				continue;
			}
			serial_printf("%s(): disk %d ext2_mount() failed\n", __func__, disk_num);
			
			// Other filesystems mount will be here. When they are supported
			continue;
		}
		else
		{
			serial_printf("%s(): disk %d has MBR - partitioned\n", __func__, disk_num);
			mbr_part_rec = (struct mbr_part_rec*)(mbr + 0x1BE);
		}

		for (part_num = 0; part_num < max_partitions; ++part_num, ++mbr_part_rec)
		{
			serial_printf("%s(): disk %d partition %d: status=%d first_head=%d first_sector_cyl_high=%d first_cyl=%d type=%02X last_head=%d last_sector_cyl_high=%d last_cyl=%d first_lba=%lu num_blocks=%lu\n", __func__, disk_num, part_num, (int)mbr_part_rec->status, (int)mbr_part_rec->first_head,
				(int)mbr_part_rec->first_sector_cyl_high, (int)mbr_part_rec->first_cyl, (int)mbr_part_rec->type, (int)mbr_part_rec->last_head,
				(int)mbr_part_rec->last_sector_cyl_high, (int)mbr_part_rec->last_cyl, mbr_part_rec->first_lba, mbr_part_rec->num_blocks);
			rv = fat_mount(&filesystems[fs_num], mount_point, disk_num, mbr_part_rec[part_num].first_lba);
			if (0 == rv)
			{
				serial_printf("%s(): disk %d partition %d fat_mount() succeeded\n", __func__, disk_num, part_num);
				++fs_num;
				++mount_point[1];
				continue;
			}
			serial_printf("%s(): disk %d partition %d fat_mount() failed\n", __func__, disk_num, part_num);
			rv = ext2_mount(&filesystems[fs_num], mount_point, disk_num, mbr_part_rec[part_num].first_lba);
			if (0 == rv)
			{
				serial_printf("%s(): disk %d partition %d ext2_mount() succeeded\n", __func__, disk_num, part_num);
				++fs_num;
				++mount_point[1];
				continue;
			}
			serial_printf("%s(): disk %d partition %d ext2_mount() failed\n", __func__, disk_num, part_num);
			
			// Other filesystems mount will be here. When they are supported
			continue;

		}
	}

	// Open stdin, stdout, stderr
	// File descriptors work as they usually work in Unix: open() always opens the lowest fd available, so stdin, stdout and stderr may be easily redirected
	open(STDIN_DEVNAME, O_RDONLY);		// First open - will open fd = 0
	open(STDOUT_DEVNAME, O_WRONLY);		// Will open fd = 1
	open(STDERR_DEVNAME, O_WRONLY);		// Will open fd = 2
}

DIR	*opendir(const char *pathname)
{
	int	fd;
	struct reg_file	*reg_file;
	struct fs	*fs;
	int	rv;
	void	*fs_entry;
	char	*p;

	errno = 0;

	for (fd = 0; fd < MAX_FILES; ++fd)
		if (FILE_TYPE_NONE == files[fd].file_type)
			break;

	if (MAX_FILES == fd)
	{
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	NULL;
	}

	files[fd].file_type = get_file_type(pathname);
	files[fd].attr = (O_DIRECTORY | O_RDONLY);

	// Allocate and fill FS structure
	switch (files[fd].file_type)
	{
	default:
		errno = EINVAL;
		break;
	case FILE_TYPE_FS_FILE:
		fs = get_fs(pathname);
		if (NULL == fs)
		{
			errno = ENOENT;
			return	NULL;
		}
		// call FS's open() procedure
		p = strchr(pathname+1, '/');
		rv = fs->file_open(fs, p, (O_DIRECTORY | O_RDONLY), &fs_entry);
		if (rv != 0)
		{
			// errno is set by fs->file_open()
			return	NULL;	
		}
		reg_file = calloc(1, sizeof(struct reg_file));
		if (NULL == reg_file)
		{
			errno = ENOMEM;
			fs->file_close(fs, fs_entry);
			return	NULL;
		}
		reg_file->fs = fs;
		reg_file->fs_entry = fs_entry;
		reg_file->flags = (O_DIRECTORY | O_RDONLY);
		reg_file->attrib = fs->get_file_attrib(fs, fs_entry);
		if (!S_ISDIR(reg_file->attrib))
		{
			fs->file_close(fs, fs_entry);
			free(reg_file);
			files[fd].file_struct = NULL;
			errno = ENOTDIR;
			return	NULL;
		}

		reg_file->fs->dir_pos = 0;
		files[fd].file_struct = reg_file;
		break;
	}

	return	&files[fd];
}

int	closedir(DIR *dir)
{
	struct reg_file	*rf;

	errno = 0;
	rf = (struct reg_file*)dir->file_struct;
	rf->fs->file_close(rf->fs, rf->fs_entry);
	free(rf);
	return	0;	
}

struct dirent	*readdir(DIR *dir)
{
	struct reg_file	*rf;

	errno = 0;
	if (dir->file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return	NULL;
	}
	rf = (struct reg_file*)dir->file_struct;
	if (!S_ISDIR(rf->attrib))
	{
		errno = EBADF;
		return	NULL;
	}
	return	rf->fs->read_dir(rf->fs, rf->fs_entry, rf->fs->dir_pos);
}

void	rewinddir(DIR *dir)
{
	struct reg_file	*rf;

	errno = 0;
	if (dir->file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return;
	}
	rf = (struct reg_file*)dir->file_struct;
	if (!S_ISDIR(rf->attrib))
	{
		errno = EBADF;
		return;
	}
	rf->fs->dir_pos = 0;
}

void	seekdir(DIR *dir, off_t offset)
{
	struct reg_file	*rf;

	errno = 0;
	if (dir->file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return;
	}
	rf = (struct reg_file*)dir->file_struct;
	if (!S_ISDIR(rf->attrib))
	{
		errno = EBADF;
		return;
	}
	rf->fs->dir_pos = rf->fs->seek_dir(rf->fs, rf->fs_entry, offset);
}

off_t	telldir(DIR *dir)
{
	struct reg_file	*rf;

	errno = 0;
	if (dir->file_type != FILE_TYPE_FS_FILE)
	{
		errno = EBADF;
		return	(off_t)-1;
	}
	rf = (struct reg_file*)dir->file_struct;
	if (!S_ISDIR(rf->attrib))
	{
		errno = EBADF;
		return	(off_t)-1;
	}
	return	rf->fs->dir_pos;
}


int	copy_file(const char *src, const char *dest)
{
	int	fd, dest_fd;
	ssize_t	sz, wsz;
	unsigned char	buf[1024];
	int	rv;

	fd = open(src, O_RDONLY);
	if (fd < 0)
		return fd;
	dest_fd = open(dest, O_RDWR); 
	if (dest_fd < 0)
	{
		close(fd);
		return dest_fd;
	}

	rv = 0;
	while ((sz = read(fd, buf, sizeof(buf))) > 0)
	{
		wsz = write(dest_fd, buf, sz);
		if (wsz < 0)
			break;
	}

	close(fd);
	close(dest_fd);
	return	rv;
}


off_t	get_file_size(int fd)
{
	struct reg_file	*rf;
	errno = 0;
	if (fd < 0 || fd >= MAX_FILES)
	{
		errno = EBADF;
		return	-1;
	}
	if (files[fd].file_type != FILE_TYPE_FS_FILE)
	{
		errno = EINVAL;
		return	-1;
	}
	rf = files[fd].file_struct;
	return	rf->fs->get_file_size(rf->fs, rf->fs_entry);
}

#ifdef SOCKETS
int open_socket(int domain, int type, int protocol);

int socket(int domain, int type, int protocol)
{
	int	sock = open_socket(domain, type, protocol);
	int	fd;
	struct sock_file	*sock_file;

	if (sock < 0)
		return	-1;		// open_socket() already set errno

	errno = 0;

	for (fd = 0; fd < MAX_FILES; ++fd)
		if (FILE_TYPE_NONE == files[fd].file_type)
			break;

	if (MAX_FILES == fd)
	{
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	-1;
	}

	files[fd].file_type = FILE_TYPE_SOCKET;
	sock_file = calloc(1, sizeof(struct sock_file));
	if (sock_file == NULL)
	{
		files[fd].file_type = FILE_TYPE_NONE;
		errno = ENOMEM;
		return	-1;
	}
	sock_file->sock = sock;
	files[fd].file_struct = sock_file;
	psockets[sock].fd = fd;

	if (psockets[sock].status & SOCK_STAT_MAYSEND)
		files[fd].status |= FD_STAT_MAYWRITE;

	return	fd;
}
#endif


// Create POSIX pipe
int	pipe(int fd[2])
{
	int	fd1, fd2;
	struct pipe_file	*pf1, *pf2;

	errno = 0;

	for (fd1 = 0; fd1 < MAX_FILES; ++fd1)
		if (FILE_TYPE_NONE == files[fd1].file_type)
			break;

	if (MAX_FILES == fd1)
	{
nofds_ret:
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	-1;
	}

	for (fd2 = 0; fd2 < MAX_FILES; ++fd2)
		if (FILE_TYPE_NONE == files[fd2].file_type)
			break;
	if (MAX_FILES == fd2)
		goto	nofds_ret;

	// fd1 will be read size of pipe, fd2 - write side
	pf1 = calloc(1, sizeof(struct pipe_file));
	if (!pf1)
	{
nomem_ret:
		errno = ENOMEM;
		return	-1;
	}
	pf2 = calloc(1, sizeof(struct pipe_file));
	if (!pf2)
	{
nomem_ret2:
		free(pf1);
		goto	nomem_ret;
	}

	pf1->buf = malloc(PIPE_BUF_SIZE);
	if (!pf1->buf)
		goto	nomem_ret2;
	pf1->buf_size = PIPE_BUF_SIZE;
	pf1->peer_fd = fd2;
	
	pf2->buf = pf1->buf;
	pf2->buf_size = pf1->buf_size;
	pf2->peer_fd = fd1;
	pf2->write_side = 1;

	files[fd1].file_type = FILE_TYPE_PIPE;
	files[fd2].file_type = FILE_TYPE_PIPE;

	fd[0] = fd1;
	fd[1] = fd2;

	return	0;
}


// Create POSIX FIFO (even though creation method differs)
// Anonymous FIFO doesn't need a name. Returned is a single file descriptor,
// which may be used for both reading and writing. Restricting permissions is
// not provided, if you need to separate permissions - use pipes
int	fifo(void)
{
	int	fd;
	struct fifo_file	*ff;

	errno = 0;

	for (fd = 0; fd < MAX_FILES; ++fd)
		if (FILE_TYPE_NONE == files[fd].file_type)
			break;

	if (MAX_FILES == fd)
	{
nofds_ret:
		// `ENOMEM' here means no available file descriptors (MAX_FILES already are opened)
		errno = ENOMEM;
		return	-1;
	}

	ff = calloc(1, sizeof(struct fifo_file));
	if (!ff)
	{
nomem_ret:
		errno = ENOMEM;
		return	-1;
	}

	ff->buf = malloc(FIFO_BUF_SIZE);
	if (!ff->buf)
		goto	nomem_ret;
	ff->buf_size = FIFO_BUF_SIZE;

	files[fd].file_type = FILE_TYPE_FIFO;
	return	fd;
}


