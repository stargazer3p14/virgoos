/****************************************
*
*	io.h
*
*	Header file for POSIX I/O (subset) implementation
*
****************************************/
#ifndef	IO__H
 #define	IO__H

#include "config.h"
#include "sosdef.h"
#include "errno.h"

#define	MAX_PARTITIONS	4

#define	MAX_PATH	256
#define	NAME_MAX	(MAX_PATH-1)

#define	PART_TYPE_FAT12	1
#define	PART_TYPE_FAT16_32M	4
#define	PART_TYPE_EXT_PART	5
#define	PART_TYPE_FAT16	6
#define	PART_TYPE_LINUX_NATIVE	0x83

// Standard streams
enum {STDIN, STDOUT, STDERR};

// File system types
enum	{FS_NONE /* initial value = 0 */, FS_EXT2, FS_FAT};

// File types
enum	{FILE_TYPE_NONE /* initial value = 0 */, FILE_TYPE_FS_FILE, FILE_TYPE_DEVICE, FILE_TYPE_SOCKET, FILE_TYPE_PIPE, FILE_TYPE_FIFO};

// Modes/attributes
#define	O_RDONLY	0x1
#define	O_WRONLY	0x2
#define	O_RDWR		(O_RDONLY | O_WRONLY)
#define	O_APPEND	0x4
#define	O_ASYNC		0x8
#define	O_CREAT		0x10
#define	O_DIRECTORY	0x20
#define	O_SYNC		0x40
#define	O_NONBLOCK	0x80
#define	O_NDELAY	O_NONBLOCK
#define	O_TRUNC		0x100

// Seek constants
enum	{SEEK_SET, SEEK_CUR, SEEK_END};

// Generic disk structure - disk implementations will implement the methods
// (!) for read() and write() methods `offs' needs not be sector- or block-granular. Disk implementation should read sectors/blocks
// in internal buffers and copy data if necessary.

#define	DISK_SECTOR_SIZE	512

/* Encoding of the file mode (attributes). Borrowed from ext2. Other filesystem handlers will have to translate their assignments to this  */
#define	S_IFMT	0170000	/* These bits determine file type.  */

/* File types.  */
#define	S_IFDIR	0040000	/* Directory.  */
#define	S_IFCHR	0020000	/* Character device.  */
#define	S_IFBLK	0060000	/* Block device.  */
#define	S_IFREG	0100000	/* Regular file.  */
#define	S_IFIFO	0010000	/* FIFO.  */
#define	S_IFLNK	0120000	/* Symbolic link.  */
#define	S_IFSOCK	0140000	/* Socket.  */


/* Protection bits.  */
#define	S_ISUID	04000	/* Set user ID on execution.  */
#define	S_ISGID	02000	/* Set group ID on execution.  */
#define	S_ISVTX	01000	/* Save swapped text after use (sticky).  */
#define	S_IRUSR	0400	/* Read by owner.  */
#define	S_IWUSR	0200	/* Write by owner.  */
#define	S_IXUSR	0100	/* Execute by owner.  */
#define	S_IRGRP    00040	// Group can read
#define	S_IWGRP    00020	// Group can write
#define	S_IXGRP    00010	// Group can execute
#define	S_IROTH    00004	// Anyone can read
#define	S_IWOTH    00002	// Anyone can write
#define	S_IXOTH    00001	// Anyone can execute


#define	S_ISREG(m)	(m & S_IFREG)
#define	S_ISDIR(m)	(m & S_IFDIR)

struct disk
{
	void	*disk_priv;
	int	(*read)(struct disk *this, off_t offs, void *buf, unsigned length);
	int	(*write)(struct disk *this, off_t offs, void *buf, unsigned length);
};

// MBR partition record
struct mbr_part_rec
{
	byte	status;
	byte	first_head;
	byte	first_sector_cyl_high;
	byte	first_cyl;
	byte	type;
	byte	last_head;
	byte	last_sector_cyl_high;
	byte	last_cyl;
	uint32_t	first_lba;
	uint32_t	num_blocks;
} __attribute__((packed));

struct stat;

// Generic FS structure - FS implementations will implement the methods
struct fs
{
	int	disk_num;			// Disk number
	int	part_num;			// Partition number (needed? we have `start_offs') -- meanwhile unused --
	off_t	start_offs;			// Starting offset of its partition
	char	*mount_point;			// 'root' name for a FS, may be any string. (!) It's user's responsibility to assign distinct names
	void	*fs_priv;		// Private structure specific to FS which must be instantiated
	int	fs_type;			// ext2, fat, ...
	off_t	dir_pos;			// For directory services
	int	(*unmount)(struct fs *this);		// unmount() entry point
	int	(*file_open)(struct fs *this, const char *pathname, int flags, void **fs_entry);			// FS's generic open() procedure
	int	(*file_creat)(struct fs *this, const char *pathname, mode_t mode, void **fs_entry);		// FS's generic creat() procedure
	ssize_t	(*file_read)(struct fs *this, void *fs_entry, void *buf, off_t offs, size_t count);		// FS's generic read() procedure
	ssize_t	(*file_write)(struct fs *this, void *fs_entry, const void *buf, off_t offs, size_t count);	// FS's generic write() procedure
	int	(*file_close)(struct fs *this, void *fs_entry);					// FS's generic close() procedure
	int	(*file_unlink)(struct fs *this, char *path);					// FS's generic unlink() procedure
	int	(*file_rename)(struct fs *this, char *src_path, char *dest_path);			// FS's generic rename() procedure
	ssize_t	(*get_file_size)(struct fs *this, void *fs_entry);
	uint32_t (*get_file_attrib)(struct fs *this, void *fs_entry);
	struct dirent	*(*read_dir)(struct fs *this, void *fs_entry, off_t offs);
	size_t	(*seek_dir)(struct fs *this, void *fs_entry, off_t offs);			// Returns correct offset for given offs - to be used in further read_dir().
	int	(*stat)(struct fs *this, const char *path, struct stat *buf);			// stat()
	int	(*fstat)(struct fs *this, void *fs_entry, struct stat *buf);			// stat()
	void	(*sync)(struct fs *this);	// Just sync()
};

// Regular file structure
struct reg_file
{
	off_t	pos;				// Current offset
	unsigned	attrib;			// May be we need this separate from "creation attributes" in struct file?
	unsigned	mode;			// read/write/both - this is an open() request mode
	unsigned	flags;			// synced, ...?
	struct fs	*fs;			// FS to which the file belongs
	void	*fs_entry;			// FS entry (inode structure, directory entry etc.)
	int	open_count;			// Synchronize open(), dup()s and close()s
};

// Device file structure
struct dev_file
{
	int	dev_id, subdev_id;		// Device and sub-device ID, valid only for devices
};

// Socket file structure
struct sock_file
{
	int	sock;
};

// Pipe file structure
#define	PIPE_BUF_SIZE	65536
struct pipe_file
{
	int	peer_fd;			// File descriptor of "other side" (needed?)
	int	write_side;			// 0 if read side, 1 if write side
	size_t	buf_size;			// Size of buffer (must be multiple of malloc() alignment)
	void	*buf;				// Buffer to exchange data
	size_t	head, tail;			// Head and tail pointers to data in buffer
};

// FIFO file structure
#define	FIFO_BUF_SIZE	65536
struct fifo_file
{
	size_t	buf_size;			// Size of buffer (must be multiple of malloc() alignment)
	void	*buf;				// Buffer to exchange data
	size_t	head, tail;			// Head and tail pointers to data in buffer
};


// Generic structure - for file or device
struct file
{
	unsigned	attr;			// read/write/both (for both files and devices), execute (files only). This is creation attribute
	unsigned	file_type;
#define	FD_STAT_MAYREAD	0x1
#define	FD_STAT_MAYWRITE	0x2
#define	FD_STAT_NONBLOCK	0x4
	unsigned long	status;			// Status (may read, may write - select() events)
	void	*file_struct;			// struct reg_file, struct dev_file, struct socket etc.
};

struct dirent
{
	ino_t	d_ino;
	char	d_name[NAME_MAX];
};

typedef	struct file	DIR;

struct stat
{
	uint32_t     st_dev;     /* ID of device containing file */ // No meaning under SeptOS, contains 0
	ino_t     st_ino;     /* inode number */		// May have meaning under SeptOS, depends on FS
	mode_t    st_mode;    // /* protection */		// File attributes
	uint32_t   st_nlink;   /* number of hard links */	// May later be supported, now contains 0
	uint32_t     st_uid;     /* user ID of owner */		// Depends on FS
	uint32_t     st_gid;     /* group ID of owner */		// Depends on FS
	uint32_t     st_rdev;    /* device ID (if special file) */	// No meaning under SeptOS, contains 0
	off_t     st_size;    /* total size, in bytes */	// File size
	uint32_t st_blksize; /* blocksize for filesystem I/O */	// No meaning under SeptOS, contains 0
	uint32_t  st_blocks;  /* number of blocks allocated */		// No meaning under SeptOS, contains 0
	time_t    st_atime;   /* time of last access */			// Info from FS
	time_t    st_mtime;   /* time of last modification */		// Info from FS
	time_t    st_ctime;   /* time of last status change */		// Info from FS
};

#define	FD_SETSIZE	(MAX_FILES / 32 + (MAX_FILES % 32 != 0))	// 1 bit for all file descriptors
typedef	struct fd_set
{
	unsigned long	desc[FD_SETSIZE];
} fd_set;

//Mount entry points - those need to be direct
int	ext2_mount(struct fs *this, const char *mount_point, int disk_num, unsigned start_sect);
int	fat_mount(struct fs *this, const char *mount_point, int disk_num, unsigned start_sect);

// Function prototypes

// POSIX I/O interface
int	open(const char *pathname, int flags, ...);				// SeptOS doesn't have users/groups to watch permissions for, but the prototype needs to include it
int	creat(const char *pathname, mode_t mode);
ssize_t	read(int fd, void *buf, size_t count);
ssize_t	write(int fd, const void *buf, size_t count);
off_t	lseek(int fildes, off_t offset, int whence);
int	fcntl(int fd, int cmd, ...);
int	close(int fd);
int	ioctl(int d, int request, ...);
void	sync(void);
int	fsync(int fd);
int	fdatasync(int fd);
int	unlink(const char *pathname);
int	dup(int oldfd);
int	dup2(int oldfd, int newfd);
int	link(const char *oldpath, const char *newpath);
int	mknod(const char *pathname, mode_t mode, dev_t dev);
int	stat(const char *path, struct stat *buf);
int	fstat(int filedes, struct stat *buf);
int	lstat(const char *path, struct stat *buf);

// select() and friends
int  select(int  n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
void FD_CLR(int fd, fd_set *set);
int FD_ISSET(int fd, fd_set *set);
void FD_SET(int fd, fd_set *set);
void FD_ZERO(fd_set *set);

DIR	*opendir(const char *name);
int	closedir(DIR *dir);
struct dirent	*readdir(DIR *dir);
void	rewinddir(DIR *dir);
void	seekdir(DIR *dir, off_t offset);
off_t	telldir(DIR *dir);

FILE *fdopen(int fd, const char *mode);

// POSIX pipes, FIFOs, message queues
int	pipe(int fd[2]);

// SeptOS custom (convenience) I/O interface
int	copy_file(const char *src, const char *dest);
off_t	get_file_size(int fd);
struct fs	*get_fs(const char *path);

#endif	// IO__H

