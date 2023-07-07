#ifndef	EXT2__H
 #define EXT2__H

#include "config.h"

/*******************************************************************************************************
 *
 *	Header
 *
 ********************************************************************************************************/

#define	EXT2_SIGNATURE	0xEF53

enum	{CMD_LS = 1, CMD_CAT, CMD_CP, CMD_MV, CMD_RM};

enum	{INODE_IO_READ, INODE_IO_WRITE};

#define	EXT2_MAX_BLOCK_SIZE	0x10000

#define	ROOT_INODE_NUM	2

/*****************************************************************************************
* Grabbed from Linux "ext2_fs.h". Consider rewriting definitions if license problems arise
*/

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

/*
 * Inode flags (GETFLAGS/SETFLAGS)
 */
#define	EXT2_SECRM_FL			FS_SECRM_FL	/* Secure deletion */
#define	EXT2_UNRM_FL			FS_UNRM_FL	/* Undelete */
#define	EXT2_COMPR_FL			FS_COMPR_FL	/* Compress file */
#define EXT2_SYNC_FL			FS_SYNC_FL	/* Synchronous updates */
#define EXT2_IMMUTABLE_FL		FS_IMMUTABLE_FL	/* Immutable file */
#define EXT2_APPEND_FL			FS_APPEND_FL	/* writes to file may only append */
#define EXT2_NODUMP_FL			FS_NODUMP_FL	/* do not dump file */
#define EXT2_NOATIME_FL			FS_NOATIME_FL	/* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL			FS_DIRTY_FL
#define EXT2_COMPRBLK_FL		FS_COMPRBLK_FL	/* One or more compressed clusters */
#define EXT2_NOCOMP_FL			FS_NOCOMP_FL	/* Don't compress */
#define EXT2_ECOMPR_FL			FS_ECOMPR_FL	/* Compression error */
/* End compression flags --- maybe not all used */	
#define EXT2_BTREE_FL			FS_BTREE_FL	/* btree format dir */
#define EXT2_INDEX_FL			FS_INDEX_FL	/* hash-indexed directory */
#define EXT2_IMAGIC_FL			FS_IMAGIC_FL	/* AFS directory */
#define EXT2_JOURNAL_DATA_FL		FS_JOURNAL_DATA_FL /* Reserved for ext3 */
#define EXT2_NOTAIL_FL			FS_NOTAIL_FL	/* file tail should not be merged */
#define EXT2_DIRSYNC_FL			FS_DIRSYNC_FL	/* dirsync behaviour (directories only) */
#define EXT2_TOPDIR_FL			FS_TOPDIR_FL	/* Top of directory hierarchies*/
#define EXT2_RESERVED_FL		FS_RESERVED_FL	/* reserved for ext2 lib */

#define EXT2_FL_USER_VISIBLE		FS_FL_USER_VISIBLE	/* User visible flags */
#define EXT2_FL_USER_MODIFIABLE		FS_FL_USER_MODIFIABLE	/* User modifiable flags */

/*
 * Structure of the super block
 */
struct ext2_super_block {
	uint32_t	s_inodes_count;		/* Inodes count */
	uint32_t	s_blocks_count;		/* Blocks count */
	uint32_t	s_r_blocks_count;	/* Reserved blocks count */
	uint32_t	s_free_blocks_count;	/* Free blocks count */
	uint32_t	s_free_inodes_count;	/* Free inodes count */
	uint32_t	s_first_data_block;	/* First Data Block */
	uint32_t	s_log_block_size;	/* Block size */
	uint32_t	s_log_frag_size;	/* Fragment size */
	uint32_t	s_blocks_per_group;	/* # Blocks per group */
	uint32_t	s_frags_per_group;	/* # Fragments per group */
	uint32_t	s_inodes_per_group;	/* # Inodes per group */
	uint32_t	s_mtime;		/* Mount time */
	uint32_t	s_wtime;		/* Write time */
	uint16_t	s_mnt_count;		/* Mount count */
	uint16_t	s_max_mnt_count;	/* Maximal mount count */
	uint16_t	s_magic;		/* Magic signature */
	uint16_t	s_state;		/* File system state */
	uint16_t	s_errors;		/* Behaviour when detecting errors */
	uint16_t	s_minor_rev_level; 	/* minor revision level */
	uint32_t	s_lastcheck;		/* time of last check */
	uint32_t	s_checkinterval;	/* max. time between checks */
	uint32_t	s_creator_os;		/* OS */
	uint32_t	s_rev_level;		/* Revision level */
	uint16_t	s_def_resuid;		/* Default uid for reserved blocks */
	uint16_t	s_def_resgid;		/* Default gid for reserved blocks */
	/*
	 * These fields are for EXT2_DYNAMIC_REV superblocks only.
	 *
	 * Note: the difference between the compatible feature set and
	 * the incompatible feature set is that if there is a bit set
	 * in the incompatible feature set that the kernel doesn't
	 * know about, it should refuse to mount the filesystem.
	 * 
	 * e2fsck's requirements are more strict; if it doesn't know
	 * about a feature in either the compatible or incompatible
	 * feature set, it must abort and not try to meddle with
	 * things it doesn't understand...
	 */
	uint32_t	s_first_ino; 		/* First non-reserved inode */
	uint16_t   s_inode_size; 		/* size of inode structure */
	uint16_t	s_block_group_nr; 	/* block group # of this superblock */
	uint32_t	s_feature_compat; 	/* compatible feature set */
	uint32_t	s_feature_incompat; 	/* incompatible feature set */
	uint32_t	s_feature_ro_compat; 	/* readonly-compatible feature set */
	uint8_t	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	uint32_t	s_algorithm_usage_bitmap; /* For compression */
	/*
	 * Performance hints.  Directory preallocation should only
	 * happen if the EXT2_COMPAT_PREALLOC flag is on.
	 */
	uint8_t	s_preext2_alloc_blocks;	/* Nr of blocks to try to preallocate*/
	uint8_t	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
	uint16_t	s_padding1;
	/*
	 * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
	 */
	uint8_t	s_journal_uuid[16];	/* uuid of journal superblock */
	uint32_t	s_journal_inum;		/* inode number of journal file */
	uint32_t	s_journal_dev;		/* device number of journal file */
	uint32_t	s_last_orphan;		/* start of list of inodes to delete */
	uint32_t	s_hash_seed[4];		/* HTREE hash seed */
	uint8_t	s_def_hash_version;	/* Default hash version to use */
	uint8_t	s_reserved_char_pad;
	uint16_t	s_reserved_word_pad;
	uint32_t	s_default_mount_opts;
 	uint32_t	s_first_meta_bg; 	/* First metablock block group */
	uint32_t	s_reserved[190];	/* Padding to the end of the block */
} __attribute__ ((packed));


/*
 * Structure of a blocks group descriptor
 */
struct ext2_group_desc
{
	uint32_t	bg_blocks_bitmap;		/* Blocks bitmap block */
	uint32_t	bg_inodes_bitmap;		/* Inodes bitmap block */
	uint32_t	bg_inode_table;		/* Inodes table block */
	uint16_t	bg_free_blocks_count;	/* Free blocks count */
	uint16_t	bg_free_inodes_count;	/* Free inodes count */
	uint16_t	bg_used_dirs_count;	/* Directories count */
	uint16_t	bg_pad;
	uint32_t	bg_reserved[3];
} __attribute__ ((packed));

/*
 * Structure of an inode on the disk
 */
struct ext2_inode {
	uint16_t	i_mode;		/* File mode */
	uint16_t	i_uid;		/* Low 16 bits of Owner Uid */
	uint32_t	i_size;		/* Size in bytes */
	uint32_t	i_atime;	/* Access time */
	uint32_t	i_ctime;	/* Creation time */
	uint32_t	i_mtime;	/* Modification time */
	uint32_t	i_dtime;	/* Deletion Time */
	uint16_t	i_gid;		/* Low 16 bits of Group Id */
	uint16_t	i_links_count;	/* Links count */
	uint32_t	i_blocks;	/* Blocks count */
	uint32_t	i_flags;	/* File flags */
	union {
		struct {
			uint32_t  l_i_reserved1;
		} linux1;
		struct {
			uint32_t  h_i_translator;
		} hurd1;
		struct {
			uint32_t  m_i_reserved1;
		} masix1;
	} osd1;				/* OS dependent 1 */
	uint32_t	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
	uint32_t	i_generation;	/* File version (for NFS) */
	uint32_t	i_file_acl;	/* File ACL */
	uint32_t	i_dir_acl;	/* Directory ACL */
	uint32_t	i_faddr;	/* Fragment address */
	union {
		struct {
			uint8_t	l_i_frag;	/* Fragment number */
			uint8_t	l_i_fsize;	/* Fragment size */
			uint16_t	i_pad1;
			uint16_t	l_i_uid_high;	/* these 2 fields    */
			uint16_t	l_i_gid_high;	/* were reserved2[0] */
			uint32_t	l_i_reserved2;
		} linux2;
		struct {
			uint8_t	h_i_frag;	/* Fragment number */
			uint8_t	h_i_fsize;	/* Fragment size */
			uint16_t	h_i_mode_high;
			uint16_t	h_i_uid_high;
			uint16_t	h_i_gid_high;
			uint32_t	h_i_author;
		} hurd2;
		struct {
			uint8_t	m_i_frag;	/* Fragment number */
			uint8_t	m_i_fsize;	/* Fragment size */
			uint16_t	m_pad1;
			uint32_t	m_i_reserved2[2];
		} masix2;
	} osd2;				/* OS dependent 2 */
} __attribute__ ((packed));

/*
 * Structure of a directory entry
 */
#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
	uint32_t	inode;			/* Inode number */
	uint16_t	rec_len;		/* Directory entry length */
	uint16_t	name_len;		/* Name length */
	char	name[EXT2_NAME_LEN];	/* File name */
} __attribute__ ((packed));

/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct ext2_dir_entry_2 {
	uint32_t	inode;			/* Inode number */
	uint16_t	rec_len;		/* Directory entry length */
	uint8_t	name_len;		/* Name length */
	uint8_t	file_type;
	char	name[EXT2_NAME_LEN];	/* File name */
} __attribute__ ((packed));

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
enum {
	EXT2_FT_UNKNOWN,
	EXT2_FT_REG_FILE,
	EXT2_FT_DIR,
	EXT2_FT_CHRDEV,
	EXT2_FT_BLKDEV,
	EXT2_FT_FIFO,
	EXT2_FT_SOCK,
	EXT2_FT_SYMLINK,
	EXT2_FT_MAX
};


/*
 * stat() file types - used in inodes
 */

/* Encoding of the file mode.  */

#define	EXT2_S_IFMT	0170000	/* These bits determine file type.  */

/* File types.  */
#define	EXT2_S_IFDIR	0040000	/* Directory.  */
#define	EXT2_S_IFCHR	0020000	/* Character device.  */
#define	EXT2_S_IFBLK	0060000	/* Block device.  */
#define	EXT2_S_IFREG	0100000	/* Regular file.  */
#define	EXT2_S_IFIFO	0010000	/* FIFO.  */
#define	EXT2_S_IFLNK	0120000	/* Symbolic link.  */
#define	EXT2_S_IFSOCK	0140000	/* Socket.  */


/* Protection bits.  */
#define	EXT2_S_ISUID	04000	/* Set user ID on execution.  */
#define	EXT2_S_ISGID	02000	/* Set group ID on execution.  */
#define	EXT2_S_ISVTX	01000	/* Save swapped text after use (sticky).  */
#define	EXT2_S_IREAD	0400	/* Read by owner.  */
#define	EXT2_S_IWRITE	0200	/* Write by owner.  */
#define	EXT2_S_IEXEC	0100	/* Execute by owner.  */

/*******************************************************************************************************/

char	def_fname[] = "";

#ifdef	CFG_EXT2_BLOCK_CACHE_SIZE
struct blocks_cache_key
{
	uint32_t	block_num;
	int	ref_count;
	int	sync;
};
#endif

//unsigned char	buf[0x10000];
struct ext2_fs_priv
{
	off_t block_size;
	struct ext2_super_block	super_block;
	unsigned char	*group_desc_block;
	unsigned	num_groups;
	unsigned	last_group_blocks;
#ifdef	CFG_EXT2_BLOCK_CACHE_SIZE
	struct blocks_cache_key	*blocks_cache_keys;
	unsigned char	*blocks_cache;
#endif
	off_t	last_blocks_bitmap_pos;
	unsigned char	*last_blocks_bitmap;
};

struct ext2_fs_entry
{
	struct ext2_inode	inode;
	uint32_t	inode_num;	
	uint32_t	parent_inode_num;	
};

#define	EXT2_FIND_INODE_DELETED	0x1

// Function prototypes
void	ext2_update_superblock(struct fs *this);
unsigned	ext2_alloc_inode(struct fs *this);
unsigned	ext2_alloc_block(struct fs *this);
int	ext2_free_block(struct fs *this, unsigned block_num);
int	ext2_free_inode(struct fs *this, unsigned inode_num);
int	ext2_read_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode);
int	ext2_write_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode);
int	ext2_inode_read_block(struct fs *this, struct ext2_inode *inode, unsigned block_num, void *buf);
unsigned	ext2_inode_alloc_blocks_count(struct fs *this, struct ext2_inode *inode, unsigned count);
unsigned	ext2_inode_alloc_block(struct fs *this, struct ext2_inode *inode);
int	ext2_find_inode(struct fs *this, const char *path, struct ext2_inode *dest_inode, uint32_t *dest_inode_num, unsigned flags);
unsigned	ext2_inode_io(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *buffer, unsigned size, int io_cmd);
unsigned	ext2_inode_read(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *dest, unsigned size);
unsigned	ext2_inode_write(struct fs *this, struct ext2_inode *inode, off_t offs, const unsigned char *src, unsigned size);
int	ext2_inode_unlink(struct fs *this, char *path);
int	ext2_dirent_create(struct fs *this, struct ext2_inode *parent_inode, char *name, unsigned inode_num, unsigned mode);
int	ext2_dirent_delete(struct fs *this, struct ext2_inode *parent_inode, char *name);
int	ext2_dirent_mv(struct fs *this, char *src_path, char *dest_path);
int	ext2_dirent_read(struct fs *this, struct ext2_inode *parent_inode, char *name, struct  ext2_dir_entry_2 *dest);
int	ext2_dirent_read_idx(struct fs *this, struct ext2_inode *parent_inode, int idx, struct  ext2_dir_entry_2 *dest);
ssize_t	ext2_get_file_size(struct fs *this, void *fs_entry);

#endif	// EXT2__H
