/*
 *	Implementation of ext2 fs for SeptOS.
 *
 *	Current limitation: block size <= 64K (is it a limitation?)
 */

#include <stddef.h>
#include "errno.h"
#include "config.h"
#include "sosdef.h"
#include "io.h"
#include "ext2.h"

#define	DEBUG_INODE_IO	0	

// Just to compile, we need to implement time().
#define	time(a)	(0)

#define	printf	serial_printf

extern struct disk	disks[MAX_DISKS];

// Function prototypes
#ifdef CFG_EXT2_BLOCK_CACHE_SIZE
static int	ext2_read_block_cache(struct fs *this, uint32_t block_num, void *buf);
static int	ext2_write_block_cache(struct fs *this, uint32_t block_num, void *buf);
#endif
static int	ext2_read_block(struct fs *this, uint32_t block_num, void *buf);
static int	ext2_write_block(struct fs *this, uint32_t block_num, void *buf);
int	ext2_write_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode);
int	ext2_read_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode);
int	ext2_inode_read_block(struct fs *this, struct ext2_inode *inode, unsigned block_num, void *buf);
void	dump_super_block(struct ext2_super_block *s);
void	dump_inode(struct fs *this, struct ext2_inode *i);
void	dump_group_desc(struct ext2_group_desc *gd);
void	ext2_update_superblock(struct fs *this);
unsigned	ext2_alloc_inode(struct fs *this);
static int	ext2_refill_blocks_bitmap(struct fs *this, off_t blocks_bitmap_pos);
unsigned	ext2_alloc_block(struct fs *this);
int	ext2_free_block(struct fs *this, unsigned block_num);
int	ext2_free_inode(struct fs *this, unsigned inode_num);
unsigned	ext2_inode_alloc_blocks_count(struct fs *this, struct ext2_inode *inode, unsigned count);
unsigned	ext2_inode_alloc_block(struct fs *this, struct ext2_inode *inode);
int	ext2_find_inode(struct fs *this, const char *path, struct ext2_inode *dest_inode, uint32_t *dest_inode_num, unsigned flags);
unsigned	ext2_inode_io(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *buffer, unsigned size, int io_cmd);
unsigned	ext2_inode_read(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *dest, unsigned size);
unsigned	ext2_inode_write(struct fs *this, struct ext2_inode *inode, off_t offs, const unsigned char *src, unsigned size);
int	ext2_inode_unlink(struct fs *this, char *dest);
int	ext2_dirent_mv(struct fs *this, char *src, char *dest);
unsigned	mode_to_file_type(unsigned mode);
int	ext2_dirent_create(struct fs *this, struct ext2_inode *parent_inode, char *name, unsigned inode_num, unsigned mode);
int	ext2_dirent_delete(struct fs *this, struct ext2_inode *parent_inode, char *name);
int	ext2_dirent_read(struct fs *this, struct ext2_inode *parent_inode, char *name, struct  ext2_dir_entry_2 *dest);
int	ext2_dirent_read_idx(struct fs *this, struct ext2_inode *parent_inode, int idx, struct  ext2_dir_entry_2 *dest);
static int file_stat(struct fs *this, struct ext2_inode	*inode, uint32_t inode_num, struct stat *buf);

/***********************************************************************************
* Generic FS interface implementation
************************************************************************************/
int	ext2_unmount(struct fs *this);
int	ext2_file_open(struct fs *this, const char *pathname, int flags, void **fs_entry);
int	ext2_file_creat(struct fs *this, const char *pathname, mode_t mode, void **fs_entry);
ssize_t	ext2_file_read(struct fs *this, void *fs_entry, void *buf, off_t offs, size_t count);
ssize_t	ext2_file_write(struct fs *this, void *fs_entry, const void *buf, off_t offs, size_t count);
int	ext2_file_close(struct fs *this, void *fs_entry);
int	ext2_file_unlink(struct fs *this, char *dest);
int	ext2_file_rename(struct fs *this, char *src, char *dest);
ssize_t	ext2_get_file_size(struct fs *this, void *fs_entry);
uint32_t	ext2_get_file_attrib(struct fs *this, void *fs_entry);
struct dirent	*ext2_read_dir(struct fs *this, void *fs_entry, off_t pos);
size_t	ext2_seek_dir(struct fs *this, void *fs_entry, off_t pos);
int     ext2_fstat(struct fs *this, void *fs_entry, struct stat *buf);
int     ext2_stat(struct fs *this, const char *path, struct stat *buf);
void	ext2_sync(struct fs *this);
int	ext2_mount(struct fs *this, const char *mount_point, int disk_num, unsigned start_sect);

/*
 *	Limitations:
 *
 *	1) Block sizes up to 64K
 *	2) Ext2 related fields only (may no understand ext3 specifics)
 *	3) Little-endian
 *	4) No multi-threading
 *	5) Files sizes up to 4G
 */

int	debug_ext2_inode_io = DEBUG_INODE_IO;


#ifdef CFG_EXT2_BLOCK_CACHE_SIZE

enum {EXT2_CACHE_UPDATE_WB, EXT2_CACHE_UPDATE_WT};

int	ext2_cache_update = EXT2_CACHE_UPDATE_WB;

// What kind of blocks we would usually want to cache?
// Inode tables, block and inode allocation bitmaps, directory entries etc.
// Probably NOT regular files (for now, but who knows...)

// It appears that we don't get tremendous achievents on emulator. We'll see on real hw
// Caching file I/O is questionable to performance, we can easily end up constantly updating cache, wasting much time on memocy copying, or the cache must be big
// Or is it only questionable on emulators that have their emulated disk file cached by the host OS?

// Read block from cache
static int	ext2_read_block_cache(struct fs *this, uint32_t block_num, void *buf)
{
	uint32_t	blknum, repl_cand;
	struct ext2_fs_priv	*priv = this->fs_priv;
	int	rv;

	if (priv->blocks_cache_keys == NULL || priv->blocks_cache == NULL)
		return	-1;

	blknum = block_num % CFG_EXT2_BLOCK_CACHE_SIZE;
	repl_cand = blknum;
	// The loop will find the block with minimum reference count or first empty entry.
	// Empty entries fit in algorithm because they have ref_count == 0, while any non-empty entry has it at least 1
	do
	{
		if (block_num == priv->blocks_cache_keys[blknum].block_num)
		{
			// Great, satisfy read from cache
			if (priv->blocks_cache_keys[blknum].ref_count < INT_MAX)
				++priv->blocks_cache_keys[blknum].ref_count;
			memcpy(buf, priv->blocks_cache + blknum * priv->block_size, priv->block_size);
			return	0;
		}
		// Advance replacement candidate in necessary
		if (priv->blocks_cache_keys[blknum].ref_count < priv->blocks_cache_keys[repl_cand].ref_count)
			repl_cand = blknum;
		blknum = (blknum + 1) % CFG_EXT2_BLOCK_CACHE_SIZE;
	} while (blknum != block_num % CFG_EXT2_BLOCK_CACHE_SIZE);

	// Block not found in cache. If we are replacing, write-back entry
	if (ext2_cache_update == EXT2_CACHE_UPDATE_WB)
	{
		if (priv->blocks_cache_keys[repl_cand].block_num != 0 && !priv->blocks_cache_keys[repl_cand].sync)
		{
			rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->block_size * priv->blocks_cache_keys[repl_cand].block_num, priv->blocks_cache + repl_cand * priv->block_size, priv->block_size);
			if (rv != 0)
				return	rv;
		}
	}

	//Read from disk to cache and to dest
	rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->block_size * block_num, priv->blocks_cache + repl_cand * priv->block_size, priv->block_size);
	if (rv != 0)
		return	rv;

	priv->blocks_cache_keys[repl_cand].ref_count = 1;
	priv->blocks_cache_keys[repl_cand].block_num = block_num;
	priv->blocks_cache_keys[repl_cand].sync = 1;
	memcpy(buf, priv->blocks_cache + repl_cand * priv->block_size, priv->block_size);
	return	0;
}

// Write block through cache
static int	ext2_write_block_cache(struct fs *this, uint32_t block_num, void *buf)
{
	uint32_t	blknum, repl_cand;
	struct ext2_fs_priv	*priv = this->fs_priv;
	int	rv;

	if (priv->blocks_cache_keys == NULL || priv->blocks_cache == NULL)
		return	-1;

	blknum = block_num % CFG_EXT2_BLOCK_CACHE_SIZE;
	repl_cand = blknum;
	do
	{
		if (block_num == priv->blocks_cache_keys[blknum].block_num)
		{
			// Great, satisfy read from cache
			if (priv->blocks_cache_keys[blknum].ref_count < INT_MAX)
				++priv->blocks_cache_keys[blknum].ref_count;
			memcpy(priv->blocks_cache + blknum * priv->block_size, buf, priv->block_size);
			return disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->block_size * block_num, priv->blocks_cache + repl_cand * priv->block_size, priv->block_size);
		}
		// Advance replacement candidate in necessary
		if (priv->blocks_cache_keys[blknum].ref_count < priv->blocks_cache_keys[repl_cand].ref_count)
			repl_cand = blknum;
		blknum = (blknum + 1) % CFG_EXT2_BLOCK_CACHE_SIZE;
	} while (blknum != block_num % CFG_EXT2_BLOCK_CACHE_SIZE);

	// Block not found in cache. Cache it and then write to disk
	priv->blocks_cache_keys[repl_cand].ref_count = 1;
	priv->blocks_cache_keys[repl_cand].block_num = block_num;
	memcpy(priv->blocks_cache + repl_cand * priv->block_size, buf, priv->block_size);
	// Write-through cache
	if (ext2_cache_update == EXT2_CACHE_UPDATE_WT)
	{
		rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->block_size * block_num, priv->blocks_cache + repl_cand * priv->block_size, priv->block_size);
		if (rv != 0)
			return	rv;
		priv->blocks_cache_keys[repl_cand].sync = 1;
		return	0;
	}
	// Write-back cache
	priv->blocks_cache_keys[repl_cand].sync = 0;
	return	0;	
}
#endif


static int	ext2_read_block(struct fs *this, uint32_t block_num, void *buf)
{
	struct ext2_fs_priv	*priv = this->fs_priv;
#ifdef CFG_EXT2_BLOCK_CACHE_SIZE
	if (priv->blocks_cache != NULL)
		return	ext2_read_block_cache(this, block_num, buf);
#endif
	return disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->block_size * block_num, buf, priv->block_size);
}

static int	ext2_write_block(struct fs *this, uint32_t block_num, void *buf)
{
	struct ext2_fs_priv	*priv = this->fs_priv;
#ifdef CFG_EXT2_BLOCK_CACHE_SIZE
	if (priv->blocks_cache != NULL)
		return	ext2_write_block_cache(this, block_num, buf);
#endif
	return disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->block_size * block_num, buf, priv->block_size);
}


// Writes an inode entry into inode table
// (!) This function receives a *real* inode_num (1-based), not a 0-based index 
int	ext2_write_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode)
{
	unsigned	curr_group, inode_idx;
	unsigned	block_num;
	unsigned	block_offs;
	struct	ext2_group_desc	group_desc;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;
	int	disk_num = this->disk_num;
	off_t	start_offs = this->start_offs;
	int	rv;

	if (0 == inode_num)
	{
		errno = EINVAL;
		return	-1;
	}

	--inode_num;
	curr_group = inode_num / priv->super_block.s_inodes_per_group;
	inode_idx = inode_num % priv->super_block.s_inodes_per_group;
	memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct ext2_group_desc), sizeof(struct ext2_group_desc));
	block_num = group_desc.bg_inode_table + inode_idx * sizeof(struct ext2_inode) / block_size;
	block_offs = inode_idx * sizeof(struct ext2_inode) % block_size;

	// Commit new inode data
	rv = ext2_read_block(this, block_num, buf);
	//rv = disks[disk_num].read(&disks[disk_num], start_offs + (off_t)block_size * block_num, buf, block_size);
	if (rv != 0)
		return	rv;

	memcpy(buf + block_offs, inode, sizeof(*inode));
	rv = ext2_write_block(this, block_num, buf);
	//rv = disks[disk_num].write(&disks[disk_num], start_offs + (off_t)block_size * block_num, buf, block_size);
	return	rv;
}

// Reads an inode entry from inode table
// (!) This function receives a *real* inode_num (1-based), not a 0-based index 
int	ext2_read_inode_tbl(struct fs *this, unsigned inode_num, struct ext2_inode *inode)
{
	unsigned	curr_group, inode_idx;
	unsigned	block_num;
	unsigned	block_offs;
	struct	ext2_group_desc	group_desc;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;
	int	disk_num = this->disk_num;
	off_t	start_offs = this->start_offs;
	int	rv;

	if (0 == inode_num)
	{
		errno = EINVAL;
		return	-1;
	}

	--inode_num;
	curr_group = inode_num / priv->super_block.s_inodes_per_group;
	inode_idx = inode_num % priv->super_block.s_inodes_per_group;
	memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct  ext2_group_desc), sizeof(group_desc));
	block_num = group_desc.bg_inode_table + inode_idx * sizeof(struct ext2_inode) / block_size;
	block_offs = inode_idx * sizeof(struct ext2_inode) % block_size;

	rv = ext2_read_block(this, block_num, buf);
	//rv = disks[disk_num].read(&disks[disk_num], start_offs + block_size * block_num, buf, block_size);
	if (rv != 0)
		return	rv;

	memcpy(inode, buf + block_offs, sizeof(struct ext2_inode));
	return	0;
}

// Read a single data block
int	ext2_inode_read_block(struct fs *this, struct ext2_inode *inode, unsigned block_num, void *buf)
{
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;

	return	ext2_inode_read(this, inode, block_num * priv->block_size, buf, priv->block_size);
}


// Dump ext2 Superblock information
void	dump_super_block(struct ext2_super_block *s)
{
	int	i;

	printf("Superblock summary:\n");
	printf("------------------------------------------------------------------------\n");
	printf("Inodes count:			%u\r\n", s->s_inodes_count);
	printf("Blocks count:			%u\r\n", s->s_blocks_count);
	printf("Reserved blocks count:		%u\r\n", s->s_r_blocks_count);
	printf("Free blocks count:		%u\r\n", s->s_free_blocks_count);
	printf("Free inodes count:		%u\r\n", s->s_free_inodes_count);
	printf("First Data Block:		%u\r\n", s->s_first_data_block);
	printf("Block size:			%u\r\n", 1 << s->s_log_block_size + 10);
	printf("Fragment size:			%u\r\n", 1 << s->s_log_frag_size + 10);
	printf("# Blocks per group:		%u\r\n", s->s_blocks_per_group);
	printf("# Fragments per group:		%u\r\n", s->s_frags_per_group);
	printf("# Inodes per group:		%u\r\n", s->s_inodes_per_group);
	printf("Mount time:			%08X\r\n", s->s_mtime);
	printf("Write time:			%08X\r\n", s->s_wtime);
	printf("Mount count:			%hu\r\n", s->s_mnt_count);
	printf("Maximal mount count:		%hu\r\n", s->s_max_mnt_count);
	printf("Magic signature:		%04X\r\n", (unsigned)s->s_magic);
	printf("File system state:		%04X\r\n", (unsigned)s->s_state);
	printf("When detecting errors:		%04X\r\n", (unsigned)s->s_errors);
	printf("minor revision level:		%04X\r\n", (unsigned)s->s_minor_rev_level);
	printf("time of last check:		%08X\r\n", s->s_lastcheck);
	printf("Check interval:			%08X\r\n", s->s_checkinterval);
	printf("OS:				%u\r\n", s->s_creator_os);
	printf("Revision level:			%08X\r\n", s->s_rev_level);
	printf("Default uid for reserved:	%hu\r\n", s->s_def_resuid);
	printf("Default gid for reserved:	%hu\r\n", s->s_def_resgid);
	printf("First inode:			%08X\r\n", s->s_first_ino);
	printf("size of inode structure:	%hu\r\n", s->s_inode_size);
	printf("block group # of this superblock: %hu\r\n", s->s_block_group_nr);
	printf("compatible feature set:		%08X\r\n", s->s_feature_compat);
	printf("incompatible feature set:	%08X\r\n", s->s_feature_incompat);
	printf("RO-compatible feature set:	%08X\r\n", s->s_feature_ro_compat);
	printf("uuid for volume:		");
	for (i = 0; i < 16; ++i)
		printf("%02X", s->s_uuid[i]);
	printf("\r\n");
	printf("volume name:			");
	for (i = 0; i < 16; ++i)
		printf("%c", s->s_volume_name[i]);
	printf("\r\n");
	printf("directory where last mounted:	");
	for (i = 0; i < 64; ++i)
		printf("%c", s->s_last_mounted[i]);
	printf("\r\n");
	printf("For compression:		%08X\r\n", s->s_algorithm_usage_bitmap);
	printf("Nr of blocks to try to preallocate: %u\r\n", (unsigned)s->s_preext2_alloc_blocks);
	printf("Nr to preallocate for dirs:	%u\r\n", (unsigned)s->s_prealloc_dir_blocks);
#if 0
	printf("uuid of journal superblock:	");
        for (i = 0; i < 16; ++i)
                printf("%02X", s->s_journal_uuid[i]);
        printf("\r\n");
	printf("inode number of journal file:	%08X\r\n", s->s_journal_inum);
	printf("device number of journal file:	%08X\r\n", s->s_journal_dev);
	printf("start of list of inodes to delete: %08X\r\n", s->s_last_orphan);
	printf("HTREE hash seed:		%08X %08X %08X %08X\r\n", s->s_hash_seed[0], s->s_hash_seed[1], s->s_hash_seed[2], s->s_hash_seed[3]);
	printf("Default hash version to use:	%u\r\n", (unsigned)s->s_def_hash_version);
	printf("First metablock block group:	%u\r\n", s->s_first_meta_bg);
#endif
	printf("------------------------------------------------------------------------\r\n");
	printf("\r\n");
}


void	dump_inode(struct fs *this, struct ext2_inode *i)
{
	int	j;
	struct ext2_fs_priv	*priv = this->fs_priv;

//	printf("Inode:\n");
//	printf("------------------------------------------------------------------------\n");
	printf("Access rights:			%06o\n", (unsigned)i->i_mode);
	if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR)
		printf(" directory;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFCHR)
		printf(" char device;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFBLK)
		printf(" char device;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFREG)
		printf(" file;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFIFO)
		printf(" FIFO;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFLNK)
		printf(" symlink;");
	else if ((i->i_mode & EXT2_S_IFMT) == EXT2_S_IFSOCK)
		printf(" socket;");
	else
		printf(" strange format;");

	if (i->i_mode & EXT2_S_ISUID)
		printf(" setuid;");
	if (i->i_mode & EXT2_S_ISGID)
		printf(" setgid;");
	if (i->i_mode & EXT2_S_ISVTX)
		printf(" sticky;");
	if (i->i_mode & EXT2_S_IREAD)
		printf(" r;");
	if (i->i_mode & EXT2_S_IWRITE)
		printf(" w;");
	if (i->i_mode & EXT2_S_IEXEC)
		printf(" x;");
	if (i->i_mode & EXT2_S_IREAD >> 3)
		printf(" g+r;");
	if (i->i_mode & EXT2_S_IWRITE >> 3)
		printf(" g+w;");
	if (i->i_mode & EXT2_S_IEXEC >> 3)
		printf(" g+x;");
	if (i->i_mode & EXT2_S_IREAD >> 6)
		printf(" o+r;");
	if (i->i_mode & EXT2_S_IWRITE >> 6)
		printf(" o+w;");
	if (i->i_mode & EXT2_S_IEXEC >> 6)
		printf(" o+x;");
	printf("\n");

	printf("UID:				%hu\n", i->i_uid);
	printf("File size:			%u\n", i->i_size);
	printf("Access time:			%08X\n", i->i_atime);
	printf("Creation time:			%08X\n", i->i_ctime);
	printf("Modification time:		%08X\n", i->i_mtime);
	printf("Deletion time:			%08X\n", i->i_dtime);
	printf("GID:				%hu\n", i->i_gid);
	printf("Links count:			%hu\n", i->i_links_count);
	printf("Blocks count:			%u\n", i->i_blocks);
	printf("Flags:				%08X\n", i->i_flags);
	printf("I-blocks:			");
	for (j = 0; j < EXT2_N_BLOCKS; ++j)
		printf("%08X ", i->i_block[j]);
	printf("\n");
	if (i->i_block[EXT2_IND_BLOCK] != 0)
	{
		unsigned char	ind_block[EXT2_MAX_BLOCK_SIZE];
		unsigned char	ind2_block[EXT2_MAX_BLOCK_SIZE];

		printf("Ind1-dir-blocks:                 ");
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + i->i_block[EXT2_IND_BLOCK] * priv->block_size, ind_block, priv->block_size);	
		for (j = 0; j < priv->block_size / sizeof(uint32_t); ++j)
			printf("%08X ", *((uint32_t*)ind_block + j));
		printf("\n");
		printf("..............................................................................\n");

		if (i->i_block[EXT2_DIND_BLOCK] != 0)
		{
			printf("Ind2-ind-blocks:                                 ");
			disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + i->i_block[EXT2_DIND_BLOCK] * priv->block_size, ind2_block, priv->block_size);	
			for (j = 0; j < priv->block_size / sizeof(uint32_t); ++j)
			{
				printf("%08X::::: ", *((uint32_t*)ind2_block + j));
				if (*((uint32_t*)ind2_block + j) != 0)
				{
					int	k;

					disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + *((uint32_t*)ind2_block + j) * priv->block_size, ind_block, priv->block_size);	
					for (k = 0; k < priv->block_size / sizeof(uint32_t); ++k)
						printf("%08X ", *((uint32_t*)ind_block + k));
				}
				printf("\n");
			}
			printf("\n");
		}
	}
	printf("------------------------------------------------------------------------\n");
}

void	dump_group_desc(struct ext2_group_desc *gd)
{
	printf("%s(): bg_blocks_bitmap = %u bg_inodes_bitmap = %u bg_inode_table = %u bg_free_blocks_count = %hu bg_free_inodes_count = %hu bg_used_dirs_count = %hu\r\n", __func__,
		gd->bg_blocks_bitmap, gd->bg_inodes_bitmap, gd->bg_inode_table, gd->bg_free_blocks_count, gd->bg_free_inodes_count, gd->bg_used_dirs_count);
}

// Updates all copies of superblock
void	ext2_update_superblock(struct fs *this)
{
	unsigned	i;
	struct ext2_super_block super_block1;
	off_t	group_desc_offs;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;

	// Update superblock (all copies)
	for (i = 0; i < priv->num_groups; ++i)
	{
		// Ext2 web book is wrong on block group structure - ALL copies of superblock are offset by 0x400 from the beginning of the block group
		// (Are all those groups supposed to keep a copy of a boot record too?)
		memset(&super_block1, 0, sizeof(super_block1));
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)i * priv->super_block.s_blocks_per_group * priv->block_size + 0x400, &super_block1, sizeof(super_block1));
		if (super_block1.s_magic != EXT2_SIGNATURE)
			continue;
		disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)i * priv->super_block.s_blocks_per_group * priv->block_size + 0x400, &super_block1, sizeof(super_block1));
		// Update group descriptor block
		group_desc_offs = (off_t)i * priv->super_block.s_blocks_per_group * priv->block_size + 0x400 + sizeof(struct ext2_super_block);
		if (sizeof(struct ext2_super_block) + 0x400 < priv->block_size)
			group_desc_offs += priv->block_size - (sizeof(struct ext2_super_block) + 0x400);
		disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + group_desc_offs, priv->group_desc_block, priv->block_size);
		
	}
}


// Finds first free inode on filesystem and allocates it
// Returns inode index (0-based - NOT an official inode number!) or 0 if not found
unsigned	ext2_alloc_inode(struct fs *this)
{
	int	i, j;
	struct ext2_group_desc	group_desc;
	unsigned curr_group;
	unsigned char	inodes_bitmap[EXT2_MAX_BLOCK_SIZE];
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	int	rv;

	for (curr_group = 0; curr_group < priv->num_groups; ++curr_group)
	{ 
		// Read group descriptor
		memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct ext2_group_desc), sizeof(group_desc));
		rv = ext2_read_block(this, group_desc.bg_inodes_bitmap, inodes_bitmap);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + group_desc.bg_inodes_bitmap * priv->block_size, inodes_bitmap, priv->block_size);
		if (rv != 0)
			return	rv;

		// Look in the current group for available inode
		for (i = 0; i < priv->block_size; ++i)
			if (inodes_bitmap[i] != 0xFF)
				// Get inode index
				for (j = 0; j < 8; ++j)
					if ((1 << (j & 7) & inodes_bitmap[i]) == 0)
					{
						inodes_bitmap[i] |= 1 << (j & 7);
						--group_desc.bg_free_inodes_count;
						--priv->super_block.s_free_inodes_count;
						// Update inodes bitmap
						rv = ext2_write_block(this, group_desc.bg_inodes_bitmap, inodes_bitmap);
						//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + group_desc.bg_inodes_bitmap * priv->block_size, inodes_bitmap, priv->block_size);
						if (rv != 0)
							return	rv;

//						ext2_update_superblock(this);
						return	curr_group * priv->super_block.s_inodes_per_group + i * 8 + j;
					}
	}

	return	0;
}


// Refill last_blocks_bitmap buffer for `this', flushing the currently held buffer if necessary, Returns 0 on success or known error (non-0)
// (!) If an error occurs during flushing, the previously loaded bitmap is discarded (marked empty)
// (!) If an error occurs during loading, the bitmap remains empty
static int	ext2_refill_blocks_bitmap(struct fs *this, off_t blocks_bitmap_pos)
{
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	int	rv;

	if (blocks_bitmap_pos != priv->last_blocks_bitmap_pos)
	{
		// If last_blocks_bitmap is not empty, save it
		if (priv->last_blocks_bitmap_pos != 0)
			ext2_write_block(this, priv->last_blocks_bitmap_pos, priv->last_blocks_bitmap);

		// In case of error, this will indicate that last_blocks_bitmap is empty
		priv->last_blocks_bitmap_pos = 0;

		// Load relevant blocks bitmap into last_blocks_bitmap
		rv =  ext2_read_block(this, blocks_bitmap_pos, priv->last_blocks_bitmap);
		if (rv != 0)
			return	rv;

		priv->last_blocks_bitmap_pos = blocks_bitmap_pos;
	}

	return	0;
}


// Finds first free data block on filesystem and allocates it
// Returns block index (0-based) or 0 if not allocated
// NOTE: block 0 is always allocated (superblock, then group descriptors), so returned value of 0 means error
unsigned	ext2_alloc_block(struct fs *this)
{
	unsigned char	*blocks_bitmap;
	int	i, j;
	struct ext2_group_desc	group_desc;
	unsigned curr_group;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	int	rv;

	for (curr_group = 0; curr_group < priv->num_groups; ++curr_group)
	{ 
		// Read group descriptor
		memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct ext2_group_desc), sizeof(group_desc));

		rv = ext2_refill_blocks_bitmap(this, (off_t)group_desc.bg_blocks_bitmap);
		if (rv != 0)
			return	0;

//		rv = ext2_read_block(this, group_desc.bg_blocks_bitmap, blocks_bitmap);
//		if (rv != 0)
//			return	rv;
		blocks_bitmap = priv->last_blocks_bitmap;

		// Look in the current group for available block
		for (i = 0; i < priv->block_size; ++i)
			if (blocks_bitmap[i] != 0xFF)
				// Get inode index
				for (j = 0; j < 8; ++j)
					if ((1 << (j & 7) & blocks_bitmap[i]) == 0)
					{
						blocks_bitmap[i] |= 1 << (j & 7);
						--group_desc.bg_free_blocks_count;
						--priv->super_block.s_free_blocks_count;
						// Update blocks bitmap
//						rv = ext2_write_block(this, group_desc.bg_blocks_bitmap, blocks_bitmap);
//						if (rv != 0)
//							return	rv;
//						ext2_update_superblock(this);
						return	curr_group * priv->super_block.s_blocks_per_group + i * 8 + j;
					}
	}

	return	0;
}



// Frees a data block
int	ext2_free_block(struct fs *this, unsigned block_num)
{
	struct	ext2_group_desc	group_desc;
	unsigned	curr_group, block_idx;
//	unsigned char	blocks_bitmap[EXT2_MAX_BLOCK_SIZE];
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	int	rv;

	curr_group = block_num / priv->super_block.s_blocks_per_group;
	block_idx = block_num %  priv->super_block.s_blocks_per_group;
	memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct ext2_group_desc), sizeof(struct ext2_group_desc));
//	rv = ext2_read_block(this, (off_t)group_desc.bg_blocks_bitmap, blocks_bitmap);
//	if (rv != 0)
//		return	rv;

	rv = ext2_refill_blocks_bitmap(this, (off_t)group_desc.bg_blocks_bitmap);
	if (rv != 0)
		return	rv;

	priv->last_blocks_bitmap[block_idx / 8] &= ~(1 << block_idx % 8);
//	blocks_bitmap[block_idx / 8] &= ~(1 << block_idx % 8);
//	rv = ext2_write_block(this, (off_t)group_desc.bg_blocks_bitmap, blocks_bitmap);
//	if (rv != 0)
//		return	rv;

	++group_desc.bg_free_blocks_count;
	++priv->super_block.s_free_blocks_count;
//	ext2_update_superblock(this);

	return	0;
}


// Frees an inode
// (!) `inode_num' is 0-based!
int	ext2_free_inode(struct fs *this, unsigned inode_num)
{
	struct	ext2_group_desc	group_desc;
	unsigned	curr_group, inode_idx;
	unsigned char	inodes_bitmap[EXT2_MAX_BLOCK_SIZE];
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	int	rv;

	curr_group = inode_num / priv->super_block.s_inodes_per_group;
	inode_idx = inode_num %  priv->super_block.s_inodes_per_group;
	memcpy(&group_desc, priv->group_desc_block + curr_group * sizeof(struct ext2_group_desc), sizeof(struct ext2_group_desc));
	rv = ext2_read_block(this, (off_t)group_desc.bg_inodes_bitmap, inodes_bitmap);
	//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)group_desc.bg_inodes_bitmap * priv->block_size, inodes_bitmap, priv->block_size);
	if (rv != 0)
		return	rv;

	inodes_bitmap[inode_idx / 8] &= ~(1 << inode_idx % 8);
	rv = ext2_write_block(this, (off_t)group_desc.bg_inodes_bitmap, inodes_bitmap);
	//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)group_desc.bg_inodes_bitmap * priv->block_size, inodes_bitmap, priv->block_size);
	if (rv != 0)
		return	rv;

	++group_desc.bg_free_inodes_count;
	++priv->super_block.s_free_inodes_count;
//	ext2_update_superblock(this);

	return	0;
}


// Allocate multiple blocks to inode
// Returns number of first allocated block - for convenience use of ext2_inode_alloc_block(inode, 1)
// (!) Modifies inode's block count
// Allocates additional indirect, double-indirect and triple-indirect blocks
unsigned	ext2_inode_alloc_blocks_count(struct fs *this, struct ext2_inode *inode, unsigned count)
{
	const struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	const uint32_t	max_ind_blocks = priv->block_size / 4;
	const uint32_t	max_ind2_blocks = priv->block_size / 4 * priv->block_size / 4;
	const uint32_t	first_ind_block = EXT2_IND_BLOCK;
	const uint32_t	first_ind2_block = first_ind_block + max_ind_blocks;
	const uint32_t	first_ind3_block = first_ind2_block + max_ind2_blocks;
	uint32_t	commit_block1 = 0, commit_block2 = 0, commit_block3 = 0;				// Last block to commit
	uint32_t	ind_block_num = 0, ind2_block_num = 0;
	unsigned	returned_blocks = 0;
	uint32_t	curr_block, curr_block_num;
	int	need_ind, need_ind2, need_ind3;
	unsigned	dir_idx, ind_idx, ind2_idx;
	unsigned	needed_blocks;
	unsigned char	ind_block[EXT2_MAX_BLOCK_SIZE], ind2_block[EXT2_MAX_BLOCK_SIZE], ind3_block[EXT2_MAX_BLOCK_SIZE];
	int	rv;


	// Check that there are enough free blocks to allocate
	// Calculate number of direct and indirect blocks

	// Enough direct blocks?
	if (count <= EXT2_NDIR_BLOCKS)
		needed_blocks = count;
	else if (count <= EXT2_NDIR_BLOCKS + max_ind_blocks)
		needed_blocks = count + 1;
	else if (count <= EXT2_NDIR_BLOCKS + max_ind_blocks + max_ind2_blocks)
		needed_blocks = count + 1 + max_ind_blocks;
	else
		needed_blocks = count + 1 + max_ind_blocks * (1 + (count / max_ind2_blocks));

	if (needed_blocks >= priv->super_block.s_free_blocks_count * 95 / 100)
		return	0;

	// Enough blocks, go on to allocate them

	curr_block_num = inode->i_size / priv->block_size;
	if (inode->i_size % priv->block_size)
		++curr_block_num;

	if (curr_block_num < EXT2_NDIR_BLOCKS)
	{
		need_ind = 0;
		need_ind2 = 0;
		need_ind3 = 0; 
	}
	else if (curr_block_num < first_ind2_block)
	{
		need_ind = 1;
		need_ind2 = 0;
		need_ind3 = 0; 
		if (curr_block_num != first_ind_block)
		{
			rv = ext2_read_block(this, inode->i_block[EXT2_IND_BLOCK], ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * inode->i_block[EXT2_IND_BLOCK], ind_block, priv->block_size);
			if (rv != 0)
				return	rv;
			ind_block_num = inode->i_block[EXT2_IND_BLOCK];
			dir_idx = curr_block_num - EXT2_NDIR_BLOCKS;
		}
		// The (curr_block_num == first_ind_block) case will be initialized in run-time loop
	}
	else if (curr_block_num < first_ind3_block)
	{
		need_ind = 1;
		need_ind2 = 1;
		need_ind3 = 0; 
		if (curr_block_num != first_ind2_block)
		{
			ind2_block_num = inode->i_block[EXT2_DIND_BLOCK];
			rv = ext2_read_block(this, ind2_block_num, ind2_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * ind2_block_num, ind2_block, priv->block_size);
			if (rv != 0)
				return	rv;
			ind_idx = (curr_block_num - first_ind2_block) / max_ind_blocks;
			ind_block_num = *((uint32_t*)ind2_block + ind_idx); 
			rv = ext2_read_block(this, ind_block_num, ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * ind_block_num, ind_block, priv->block_size);
			if (rv != 0)
				return	rv;
			dir_idx = (curr_block_num - first_ind2_block) % max_ind_blocks;
		}
		// The (curr_block_num == first_ind2_block) case will be initialized in run-time loop
	}
	else
	{
		need_ind = 1;
		need_ind2 = 1;
		need_ind3 = 1; 
		if (curr_block_num != first_ind3_block)
		{
			rv = ext2_read_block(this, inode->i_block[EXT2_TIND_BLOCK], ind3_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * inode->i_block[EXT2_TIND_BLOCK], ind3_block, priv->block_size);
			if (rv != 0)
				return	rv;
			ind2_idx = (curr_block_num - first_ind3_block) / max_ind2_blocks;
			ind2_block_num = *((uint32_t*)ind3_block + ind2_idx); 
			rv = ext2_read_block(this, ind2_block_num, ind2_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * ind2_block_num, ind2_block, priv->block_size);
			if (rv != 0)
				return	rv;
			ind_idx = (curr_block_num - first_ind3_block) % max_ind2_blocks / max_ind_blocks; 
			ind_block_num = *((uint32_t*)ind2_block + ind_idx); 
			rv = ext2_read_block(this, ind_block_num, ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)priv->block_size * ind_block_num, ind_block, priv->block_size);
			if (rv != 0)
				return	rv;
			dir_idx = (curr_block_num - first_ind3_block) % max_ind2_blocks % max_ind_blocks; 
		}
	}

	// Allocation loop
	while (count > 0)
	{
		if (curr_block_num < EXT2_NDIR_BLOCKS)
		{
			curr_block = ext2_alloc_block(this);
			if (curr_block == 0)
			{
				errno = EIO;
				return	0;
			}	
			inode->i_block[curr_block_num] = curr_block;
			if (0 == returned_blocks)
				returned_blocks = curr_block;
		}
		else if (need_ind3 && curr_block_num % max_ind2_blocks == 0)
		{
			commit_block3 = inode->i_block[EXT2_TIND_BLOCK];
			++ind2_idx;
			goto	setup_ind2_block;
		}
		else if (need_ind2 && curr_block_num % max_ind_blocks == 0)
		{
			commit_block2 = ind2_block_num;
			++ind_idx;
			goto	setup_ind_block;
		}
		else if (first_ind_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 0;
			need_ind3 = 0; 
			ind_block_num = ext2_alloc_block(this);
			if (ind_block_num == 0)
			{
				errno = EIO;
				return	0;
			}
			inode->i_block[EXT2_IND_BLOCK] = ind_block_num;
			commit_block1 = ind_block_num;
			memset(ind_block, 0, priv->block_size);
			dir_idx = 0;
			++inode->i_blocks;
			goto	setup_dir_block;
		}
		else if (first_ind2_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 1;
			need_ind3 = 0; 

			ind2_block_num = ext2_alloc_block(this);
			if (ind2_block_num == 0)
			{
				errno = EIO;
				return	0;
			}
			inode->i_block[EXT2_DIND_BLOCK] = ind2_block_num;
			commit_block2 = ind2_block_num;
			memset(ind2_block, 0, priv->block_size);
			ind_idx = 0;
			++inode->i_blocks;
			goto	setup_ind_block;
		}
		else if (first_ind3_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 1;
			need_ind3 = 1; 

			curr_block = ext2_alloc_block(this);
			if (curr_block == 0)
			{
				errno = EIO;
				return	0;
			}
			inode->i_block[EXT2_TIND_BLOCK] = curr_block;
			commit_block3 = curr_block;
			memset(ind3_block, 0, priv->block_size);
			ind2_idx = 0;
			++inode->i_blocks;
setup_ind2_block:
			ind2_block_num = ext2_alloc_block(this);
			if (ind2_block_num == 0)
			{
				errno = EIO;
				return	0;
			}
			*((uint32_t*)ind3_block + ind2_idx) = ind2_block_num;
			if (commit_block2)
			{
				rv = ext2_write_block(this, commit_block2, ind2_block);
				//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)commit_block2 * priv->block_size, ind2_block, priv->block_size);
				if (rv != 0)
					return	rv;
			}

			++ind2_idx;
			commit_block2 = ind2_block_num;
			memset(ind2_block, 0, priv->block_size);
			ind_idx = 0;
			++inode->i_blocks;
			//goto	setup_ind_block;
setup_ind_block:
			ind_block_num = ext2_alloc_block(this);
			if (ind_block_num == 0)
			{
				errno = EIO;
				return	0;
			}
			*((uint32_t*)ind2_block + ind_idx) = ind_block_num;
			if (commit_block1)
			{
				rv = ext2_write_block(this, commit_block1, ind_block);
				//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)commit_block1 * priv->block_size, ind_block, priv->block_size);
				if (rv != 0)
					return	rv;
			}

			++ind_idx;
			commit_block1 = ind_block_num;
			memset(ind_block, 0, priv->block_size);
			dir_idx = 0;
			++inode->i_blocks;
			goto	setup_dir_block;
		}
		else
		{
setup_dir_block:
			curr_block = ext2_alloc_block(this);
			if (curr_block == 0)
			{
				errno = EIO;
				return	0;
			}
			*((uint32_t*)ind_block + dir_idx) = curr_block;
			if (0 == returned_blocks)
				returned_blocks = curr_block;
			commit_block1 = ind_block_num;
			++dir_idx;
		}

		--count;
		++inode->i_blocks;
		++curr_block_num;
	} // while()

	// Commit last blocks if necessary
	if (commit_block1)
	{
		rv = ext2_write_block(this, commit_block1, ind_block);
		//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)commit_block1 * priv->block_size, ind_block, priv->block_size);
		if (rv != 0)
			return	rv;
	}
	if (commit_block2)
	{
		rv = ext2_write_block(this, commit_block2, ind2_block);
		//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)commit_block2 * priv->block_size, ind2_block, priv->block_size);
		if (rv != 0)
			return	rv;
	}
	if (commit_block3)
	{
		rv = ext2_write_block(this, commit_block3, ind3_block);
		//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)commit_block3 * priv->block_size, ind3_block, priv->block_size);
		if (rv != 0)
			return	rv;
	}

	return	returned_blocks;
}


// Allocates a new data block to inode.
// May additionally allocate indirect, double-indirect and triple-indirect blocks as needed
// Returns block index or 0
// (!)	Inode is modified, must be saved by the caller
unsigned	ext2_inode_alloc_block(struct fs *this, struct ext2_inode *inode)
{
	return	ext2_inode_alloc_blocks_count(this, inode, 1);
}

// Find inode that corresponds to path
// Returns 0 on success, non-0 on failure
// 
// Parameters:
// 	path - full path to look in
// 	dest_inode - output parameter to fill
//	dest_inode_num - output parameter
//	parent_inode_num - output parameter
//	flags - specify whether to match deleted files during search, etc. (more flags TBD)
//
// If target_inode_num == 0 behavior is as described. If it is != 0 (a valid inode number), then behavior is different: the function searches for
// inode with that number (rather than for name); guess that the caller is interested in parent_inode_num.
//
// (!) This function is recursive, it calls itself to find next inode in directory subtree
// (!) This function returns *real* inode number into `dest_inode_num' (1-based), not 0-based index
int	ext2_find_inode(struct fs *this, const char *path, struct ext2_inode *dest_inode, uint32_t *dest_inode_num, unsigned flags)
{
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	struct ext2_group_desc	group_desc;
	off_t	curr_dir_offs;
	struct ext2_dir_entry_2	dirent;
	off_t	offs;
	unsigned group_num;
	unsigned inode_num;
	unsigned i, j;
	unsigned len;
	char	fname[256];
	char	search_name[256];
	char	*p = NULL;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;
	int	disk_num = this->disk_num;
	off_t	start_offs = this->start_offs;
	int	rv;

	if (NULL == path)
		return	-1;

	// Root directory
	if ('/' == path[0])
	{
		// Read root directory's inode. It's inode 2 in blocks group 0
		memcpy(&group_desc, priv->group_desc_block, sizeof(group_desc));
		*dest_inode_num = ROOT_INODE_NUM;
		ext2_read_inode_tbl(this, *dest_inode_num, dest_inode);
		return	ext2_find_inode(this, ++path, dest_inode, dest_inode_num, flags);
	}

	// Name was ending with '/', return with parent directory name filled
	if (path[0] == '\0')
		return	0;

	p = strchr(path, '/');

	// Look for sub-directory
	if (p != NULL)
	{
		memcpy(search_name, path, p-path);
		search_name[p-path] = '\0';
	}
	else
		strcpy(search_name, path);

	curr_dir_offs = 0;

	rv = ext2_dirent_read(this, dest_inode, search_name, &dirent);
	if (rv < 0)
		return	rv; 

	*dest_inode_num = dirent.inode;
	rv = ext2_read_inode_tbl(this, *dest_inode_num, dest_inode);
	if (dirent.file_type == EXT2_FT_DIR && p != NULL) 
		return	ext2_find_inode(this, p+1, dest_inode, dest_inode_num, flags);
	else
		return	0;
}

//
// Reading and writing inode was evacuated into `ext2_inode_io()' function because most of the work is data blocks traversal, which is common
// (!) The caller is responsible for updating inode table because here we don't know inode's number
//
unsigned	ext2_inode_io(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *buffer, unsigned size, int io_cmd)
{
	const struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	const unsigned	block_size = priv->block_size;
	const int	disk_num = this->disk_num;
	const off_t	start_offs = this->start_offs;
	const uint32_t	max_ind_blocks = block_size / 4;
	const uint32_t	max_ind2_blocks = block_size / 4 * block_size / 4;
	const uint32_t	first_ind_block = EXT2_IND_BLOCK;
	const uint32_t	first_ind2_block = first_ind_block + max_ind_blocks;
	const uint32_t	first_ind3_block = first_ind2_block + max_ind2_blocks;
	uint32_t	curr_block, curr_block_num;
	int	need_ind, need_ind2, need_ind3;
	unsigned	dir_idx, ind_idx, ind2_idx;
	unsigned	count;
	unsigned char	ind_block[EXT2_MAX_BLOCK_SIZE], ind2_block[EXT2_MAX_BLOCK_SIZE], ind3_block[EXT2_MAX_BLOCK_SIZE];
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	sz;
	int	rv;

	if (io_cmd == INODE_IO_READ && offs >= inode->i_size)
		return	0;

	count = 0;

	if (debug_ext2_inode_io)
		printf("%s(): inode = %08X offs = %u buffer = %08X size = %u io_cmd = %d\n", __func__, inode, offs, buffer, size, io_cmd);

	// Read part of first block
	curr_block_num = offs / block_size;

	if (curr_block_num < EXT2_NDIR_BLOCKS)
	{
		need_ind = 0;
		need_ind2 = 0;
		need_ind3 = 0; 
		curr_block = inode->i_block[curr_block_num];
	}
	else if (curr_block_num < EXT2_NDIR_BLOCKS + max_ind_blocks)
	{
		need_ind = 1;
		need_ind2 = 0;
		need_ind3 = 0; 
		rv = ext2_read_block(this, inode->i_block[EXT2_IND_BLOCK], ind_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_IND_BLOCK], ind_block, block_size);
		if (rv != 0)
			return	rv;
		dir_idx = curr_block_num - EXT2_NDIR_BLOCKS;
		curr_block = *((unsigned*)ind_block + dir_idx);
	}
	else if (curr_block_num < EXT2_NDIR_BLOCKS + max_ind_blocks + max_ind2_blocks)
	{
		need_ind = 1;
		need_ind2 = 1;
		need_ind3 = 0; 
		rv = ext2_read_block(this, inode->i_block[EXT2_DIND_BLOCK], ind2_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_DIND_BLOCK], ind2_block, block_size);
		if (rv != 0)
			return	rv;
		ind_idx = (curr_block_num - first_ind2_block) / max_ind_blocks;
		rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
		if (rv != 0)
			return	rv;
		dir_idx = (curr_block_num - first_ind2_block) % max_ind_blocks;
		curr_block = *((unsigned*)ind_block + dir_idx);
	}
	else
	{
		need_ind = 1;
		need_ind2 = 1;
		need_ind3 = 1; 
		rv = ext2_read_block(this, inode->i_block[EXT2_TIND_BLOCK], ind3_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_TIND_BLOCK], ind3_block, block_size);
		if (rv != 0)
			return	rv;
		ind2_idx = (curr_block_num - first_ind3_block) / max_ind2_blocks;
		rv = ext2_read_block(this, *((unsigned*)ind3_block + ind2_idx), ind2_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind3_block + ind2_idx), ind2_block, block_size);
		if (rv != 0)
			return	rv;
		ind_idx = (curr_block_num - first_ind3_block) % max_ind2_blocks / max_ind_blocks; 
		rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
		if (rv != 0)
			return	rv;
		dir_idx = (curr_block_num - first_ind3_block) % max_ind2_blocks % max_ind_blocks; 
		curr_block = *((unsigned*)ind_block + dir_idx);
	}

	sz = block_size - offs % block_size;
	if (size < sz)
		sz = size;

	rv = ext2_read_block(this, curr_block, buf);
	//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)curr_block * block_size, buf, block_size);
	if (debug_ext2_inode_io)
		printf("%s(): Read first block, rv=%d\n", __func__, rv);
	if (rv != 0)
		return	rv;
	if (INODE_IO_READ == io_cmd)
	{
		if (debug_ext2_inode_io)
			printf("%s(): reading first block\n", __func__);

		if (inode->i_size - offs < sz)
			sz = inode->i_size - offs;

		memcpy(buffer, buf + offs % block_size, sz);
	}
	else
	{
		memcpy(buf + offs % block_size, buffer, sz);
		if (debug_ext2_inode_io)
			printf("%s(): writing first block (may be partially) start_offs = %u curr_block = %u size = %u offs = %u buffer = %08X count = %u disk offset = %u\n", __func__, this->start_offs, curr_block, size, offs, buf, block_size, this->start_offs + (off_t)curr_block * block_size);
		rv = ext2_write_block(this, curr_block, buf);
		//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)curr_block * block_size, buf, block_size);
		if (rv != 0)
			return	rv;
	}

	size -= sz;
	offs += sz;
	buffer += sz;
	count += sz;
	++curr_block_num;

	// Main data read/write loop
	while (size > 0 && offs < inode->i_size)
	{

		if (curr_block_num < EXT2_NDIR_BLOCKS)
		{
			curr_block = inode->i_block[curr_block_num];
		}
		else if (first_ind_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 0;
			need_ind3 = 0; 
			rv = ext2_read_block(this, inode->i_block[EXT2_IND_BLOCK], ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_IND_BLOCK], ind_block, block_size);
			if (rv != 0)
				return	rv;
			dir_idx = 0;
			curr_block = *((unsigned*)ind_block + dir_idx);
		}
		else if (first_ind2_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 1;
			need_ind3 = 0; 
			rv = ext2_read_block(this, inode->i_block[EXT2_DIND_BLOCK], ind2_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_DIND_BLOCK], ind2_block, block_size);
			if (rv != 0)
				return	rv;
			ind_idx = 0;
			rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
			if (rv != 0)
				return	rv;
			dir_idx = 0;
			curr_block = *((unsigned*)ind_block + dir_idx);
		}
		else if (first_ind3_block == curr_block_num)
		{
			need_ind = 1;
			need_ind2 = 1;
			need_ind3 = 1; 
			rv = ext2_read_block(this, inode->i_block[EXT2_TIND_BLOCK], ind3_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * inode->i_block[EXT2_TIND_BLOCK], ind3_block, block_size);
			if (rv != 0)
				return	rv;
			ind2_idx = 0;
			rv = ext2_read_block(this, *((unsigned*)ind3_block + ind2_idx), ind2_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind3_block + ind2_idx), ind2_block, block_size);
			if (rv != 0)
				return	rv;
			ind_idx = 0;
			rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
			if (rv != 0)
				return	rv;
			dir_idx = 0; 
			curr_block = *((unsigned*)ind_block + dir_idx);
		}
		else if (need_ind3 && curr_block_num % max_ind2_blocks == 0)
		{
			++ind2_idx;
			rv = ext2_read_block(this, *((unsigned*)ind3_block + ind2_idx), ind2_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind3_block + ind2_idx), ind2_block, block_size);
			if (rv != 0)
				return	rv;
			ind_idx = 0;
			rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
			if (rv != 0)
				return	rv;
			dir_idx = 0; 
			curr_block = *((unsigned*)ind_block + dir_idx);
		}
		else if (need_ind2 && curr_block_num % max_ind_blocks == 0)
		{
			++ind_idx;
			rv = ext2_read_block(this, *((unsigned*)ind2_block + ind_idx), ind_block);
			//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)block_size * *((unsigned*)ind2_block + ind_idx), ind_block, block_size);
			if (rv != 0)
				return	rv;
			dir_idx = 0;
			curr_block = *((unsigned*)ind_block + dir_idx);
		}
		else
		{
			++dir_idx;
			curr_block = *((unsigned*)ind_block + dir_idx);
		}

		sz = block_size;
		if (size < sz)
			sz = size;

		// Sanity checking for bad block number got
		if (curr_block >= priv->super_block.s_blocks_count)
		{
			errno = EIO;
			return	-1;
		}

		rv = ext2_read_block(this, curr_block, buf);
		//rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (off_t)curr_block * block_size, buf, block_size);
		if (rv != 0)
			return	rv;
		if (INODE_IO_READ == io_cmd)
		{
			if (inode->i_size - offs < sz)
				sz = inode->i_size - offs;
			memcpy(buffer, buf, sz);
		}
		else
		{
			memcpy(buf, buffer, sz);
			if (debug_ext2_inode_io)
				printf("%s(): writing additional block (%d) size = %u offs = %u buffer = %08X count = %u\n", __func__, curr_block_num, size, offs, buffer, count);
			rv = ext2_write_block(this, curr_block, buf);
			//rv = disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + (off_t)curr_block * block_size, buf, block_size);
			if (rv != 0)
				return	rv;
		}
		
		size -= sz;
		offs += sz;
		buffer += sz;
		count += sz;
		++curr_block_num;
// We actually don't need a loop here
//break;
	} // while()

	inode->i_atime = time(NULL);
	if (INODE_IO_WRITE == io_cmd)
	{
		inode->i_mtime = time(NULL);

		// if written past end of file, increase the file
		if (offs > inode->i_size)
			inode->i_size = offs;
	}

	if (debug_ext2_inode_io)
		printf("%s(): returning count=%d\n", __func__, count);
	return	count;
}


//
// Read `size' bytes from `inode' starting at data offset `offs'
// Returns number of bytes read (may be less than `size' or 0 if EOF reached)
//
unsigned	ext2_inode_read(struct fs *this, struct ext2_inode *inode, off_t offs, unsigned char *dest, unsigned size)
{
	return	ext2_inode_io(this, inode, offs, dest, size, INODE_IO_READ);
}

//
// Write `size' bytes from `inode' starting at data offset `offs'
// Returns number of bytes written (normally equal to `size')
//
unsigned	ext2_inode_write(struct fs *this, struct ext2_inode *inode, off_t offs, const unsigned char *src, unsigned size)
{
	return	ext2_inode_io(this, inode, offs, (unsigned char*)src, size, INODE_IO_WRITE);
}


// Unlinks inode by marking it deleted (setting dtime), setting i_blocks to 0 and marking its data blocks as free
// Unlink is VERY slow - it writes blocks bitmaps for EVERY BLOCK (BIT!)
int	ext2_inode_unlink(struct fs *this, char *dest)
{
	struct ext2_inode	inode;
	int	rv;
	uint32_t	inode_num;
	char	path[256];
	char	file[256];
	char	*p;
	unsigned	max_dir_blocks, max_ind_blocks, max_ind2_blocks;
	int	in_ind_block;
	unsigned	first_ind_block, first_ind2_block, first_ind3_block;
	unsigned	dir_idx, ind_idx, ind2_idx;
	unsigned	curr_block;
	unsigned	block_idx;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;
	unsigned char	ind_block[EXT2_MAX_BLOCK_SIZE], ind2_block[EXT2_MAX_BLOCK_SIZE], ind3_block[EXT2_MAX_BLOCK_SIZE];

	p = strrchr(dest, '/');
	if (p != NULL)
	{
		memcpy(path, dest, p-dest);
		path[p-dest] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, dest);
	}
	rv = ext2_find_inode(this, dest, &inode, &inode_num, 0);
	if (0 == rv)
	{
		// Free inode's data blocks, including indirection pointers blocks
		// (!) The blocks are marked as free in blocks table and superblock's and group descriptor's block bitmaps are updated.
		// But the blocks contents are not changed and inode is only marked with deletion time, it's not deleted.
		// So until the next block allocation and writing operation it may be safe to undelete files
		max_ind_blocks = block_size / 4;
		max_ind2_blocks = block_size / 4 * block_size / 4;
		first_ind_block = EXT2_IND_BLOCK;
		first_ind2_block = first_ind_block + max_ind_blocks;
		first_ind3_block = first_ind2_block + max_ind2_blocks;
		max_dir_blocks = EXT2_NDIR_BLOCKS;
		block_idx = 0;

		for (; block_idx < inode.i_blocks && block_idx < EXT2_NDIR_BLOCKS; ++block_idx)
		{
			curr_block = inode.i_block[block_idx];
			rv = ext2_free_block(this, curr_block);
			if (rv != 0)
				return	rv;
		}

		if (block_idx == inode.i_blocks)
			goto	mark_inode_deleted;
		rv = ext2_read_block(this, inode.i_block[EXT2_IND_BLOCK], ind_block);
		if (rv != 0)
			return	rv;
		rv = ext2_free_block(this, inode.i_block[EXT2_IND_BLOCK]);
		if (rv != 0)
			return	rv;

		for (dir_idx = 0; block_idx < inode.i_blocks && dir_idx < max_ind_blocks; ++block_idx, ++dir_idx)
		{
			curr_block = ((unsigned*)ind_block)[dir_idx];
			rv = ext2_free_block(this, curr_block);
			if (rv != 0)
				return	rv;
		}

		if (block_idx == inode.i_blocks)
			goto	mark_inode_deleted;
		rv = ext2_read_block(this, inode.i_block[EXT2_DIND_BLOCK], ind2_block);
		if (rv != 0)
			return	rv;
		rv = ext2_free_block(this, inode.i_block[EXT2_DIND_BLOCK]);
		if (rv != 0)
			return	rv;

		for (ind_idx = 0; ind_idx < max_ind_blocks; ++ind_idx)
		{
			rv = ext2_read_block(this, ((unsigned*)ind2_block)[ind_idx], ind_block);
			if (rv != 0)
				return	rv;

			for (dir_idx = 0; block_idx < inode.i_blocks && dir_idx < max_ind_blocks; ++block_idx, ++dir_idx)
			{
				curr_block = ((unsigned*)ind_block)[dir_idx];
				rv = ext2_free_block(this, curr_block);
				if (rv != 0)
					return	rv;
			}
			rv = ext2_free_block(this, ((unsigned*)ind2_block)[ind_idx]);
			if (rv != 0)
				return	rv;
			if (block_idx == inode.i_blocks)
				goto	mark_inode_deleted;
		}

		rv = ext2_read_block(this, inode.i_block[EXT2_TIND_BLOCK], ind3_block);
		if (rv != 0)
			return	rv;
		rv = ext2_free_block(this, inode.i_block[EXT2_TIND_BLOCK]);
		if (rv != 0)
			return	rv;

		for (ind2_idx = 0; ind2_idx < max_ind2_blocks; ++ind2_idx)
		{
			rv = ext2_read_block(this, ((unsigned*)ind3_block)[ind2_idx], ind2_block);
			if (rv != 0)
				return	rv;
			for (ind_idx = 0; ind_idx < max_ind_blocks; ++ind_idx)
			{
				rv = ext2_read_block(this, ((unsigned*)ind2_block)[ind_idx], ind_block);
				if (rv != 0)
					return	rv;
				for (dir_idx = 0; block_idx < inode.i_blocks && dir_idx < max_ind_blocks; ++block_idx, ++dir_idx)
				{
					curr_block = ((unsigned*)ind_block)[dir_idx];
					rv = ext2_free_block(this, curr_block);
					if (rv != 0)
						return	rv;
				}
				rv = ext2_free_block(this, ((unsigned*)ind2_block)[ind_idx]);
				if (rv != 0)
					return	rv;
				if (block_idx == inode.i_blocks)
					goto	mark_inode_deleted;
			}
			rv = ext2_free_block(this, ((unsigned*)ind3_block)[ind2_idx]);
			if (rv != 0)
				return	rv;
		}
mark_inode_deleted:
		// Update inode in inode tables as deleted
		inode.i_dtime = time(NULL);
		inode.i_blocks = 0;

		rv = ext2_write_inode_tbl(this, inode_num, &inode);
		if (rv != 0)
			return	rv;

		// Mark inode as free in inodes bitmap
		// It will also update group descriptor and superblock with new free inodes count
		rv = ext2_free_inode(this, inode_num-1);
		if (rv != 0)
			return	rv;
	}
	else
	{
		errno = ENOENT;
		return	-1;
	}

	return	0;
}

int	ext2_dirent_mv(struct fs *this, char *src, char *dest)
{
	struct ext2_inode	olddir_inode, inode, newdir_inode;
	char	src_path[256];
	char	src_file[256];
	char	dest_path[256];
	char	dest_file[256];
	char	*p;
	uint32_t	olddir_inode_num, newdir_inode_num, inode_num;
	int	rv;

	p = strrchr(src, '/');
	if (p != NULL)
	{
		memcpy(src_path, src, p-src);
		src_path[p-src] = '\0';
		strcpy(src_file, p+1);
	}
	else
	{
		strcpy(src_path, "/");
		strcpy(src_file, src);
	}

	p = strrchr(dest, '/');
	if (p != NULL)
	{
		memcpy(dest_path, dest, p-dest);
		dest_path[p-dest] = '\0';
		strcpy(dest_file, p+1);
	}
	else
	{
		strcpy(dest_path, "/");
		strcpy(dest_file, dest);
	}
	rv = ext2_find_inode(this, src_path, &olddir_inode, &olddir_inode_num, 0);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	rv = ext2_find_inode(this, src, &inode, &inode_num, EXT2_FIND_INODE_DELETED);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	rv = ext2_find_inode(this, dest_path, &newdir_inode, &newdir_inode_num, 0);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	rv = ext2_dirent_create(this, &newdir_inode, dest_file, inode_num, inode.i_mode);
	if (rv < 0)
	{
		return	-1;
	}
	rv = ext2_inode_unlink(this, src);
	if (rv < 0)
	{
		return	-1;
	}
	return	0;
}


unsigned	mode_to_file_type(unsigned mode)
{
	unsigned	file_type = 0;

	switch (mode & EXT2_S_IFMT)
	{
	default:
		break;
	case EXT2_S_IFDIR:
		file_type = EXT2_FT_DIR;
		break;
	case EXT2_S_IFCHR:
		file_type = EXT2_FT_CHRDEV;
		break;
	case EXT2_S_IFBLK:
		file_type = EXT2_FT_BLKDEV;
		break;
	case EXT2_S_IFREG:
		file_type = EXT2_FT_REG_FILE;
		break;
	case EXT2_S_IFIFO:
		file_type = EXT2_FT_FIFO;
		break;
	case EXT2_S_IFLNK:
		file_type = EXT2_FT_SYMLINK;
		break;
	case EXT2_S_IFSOCK:
		file_type = EXT2_FT_SOCK;
		break;
	}

	return	file_type;
}


// Create a directory entry
// (!) The caller is responsible for updating inode table for containing directory because here we don't know its inode number
// Directory records are aligned on 4-byte boundary and must not cross block.
// However in reality there is another rule: directory size is always a multiple of block_size and the last record holds expansion space!
// Differently written directories are not recognized by the "canonical" Linux ext2 implementation
// (!) `mode' may be already a file_type if (mode & EXT2_S_IFMT) == 0
int	ext2_dirent_create(struct fs *this, struct ext2_inode *parent_inode, char *name, unsigned inode_num, unsigned mode)
{
	struct	ext2_dir_entry_2	dirent, *last_dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs;
	unsigned	real_rec_len;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = ENOTDIR;
		return	-1;
	}

	dirent.file_type = mode_to_file_type(mode);
	if (0 == dirent.file_type)
		dirent.file_type = mode;
	dirent.inode = inode_num;
	dirent.name_len = strlen(name);
	memcpy(dirent.name, name, dirent.name_len);

	dirent.rec_len = offsetof(struct ext2_dir_entry_2, name) + dirent.name_len;
	if (dirent.rec_len & 3)
		dirent.rec_len += 4 - (dirent.rec_len & 3);

	// Find first dirent which has enough extra space
	for (block_offs = 0; block_offs < parent_inode->i_size; block_offs += block_size)
	{
		ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		do
		{
			last_dirent = (struct ext2_dir_entry_2*)(buf + offs);
			real_rec_len = last_dirent->name_len + offsetof(struct ext2_dir_entry_2, name);
			if (real_rec_len & 3)
				real_rec_len += (4 - (real_rec_len & 3));

			if (real_rec_len + dirent.rec_len <= last_dirent->rec_len)
			{
				dirent.rec_len = last_dirent->rec_len - real_rec_len;
				last_dirent->rec_len = real_rec_len;
				offs += real_rec_len;
				memcpy(buf + offs, &dirent, dirent.name_len + offsetof(struct ext2_dir_entry_2, name));
				//debug_ext2_inode_io = 1;
				ext2_inode_write(this, parent_inode, block_offs, buf, block_size);
				//debug_ext2_inode_io = 0;
				return	0;
			}

			offs += last_dirent->rec_len;
		} while (offs + last_dirent->rec_len < block_size);
	}

	// Space not found, allocate a new block
	ext2_inode_alloc_block(this, parent_inode);
	dirent.rec_len = block_size;
	offs = parent_inode->i_size - block_size;

	memcpy(buf, &dirent, sizeof(struct ext2_dir_entry_2));
	ext2_inode_write(this, parent_inode, offs, buf, block_size);

	return	0;
}

// Delete directory entry for a given name
int	ext2_dirent_delete(struct fs *this, struct ext2_inode *parent_inode, char *name)
{
	struct	ext2_dir_entry_2	*dirent, *last_dirent, *next_dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs;
	unsigned	real_rec_len, new_rec_len;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = ENOTDIR;
		return	-1;
	}

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	{
		errno = EINVAL;
		return	-1;
	}

	for (block_offs = 0; block_offs < parent_inode->i_size; block_offs += block_size)
	{
		ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		last_dirent = NULL;
		for (offs = 0; offs < block_size; )
		{
			dirent = (struct ext2_dir_entry_2*)(buf + offs);
			if (memcmp(name, dirent->name, dirent->name_len) == 0)
			{
				if (dirent->rec_len == block_size)
				{
					// Make it blank record and leave rec_len == block_size
					dirent->inode = 0;
					dirent->name_len = 0;
					dirent->file_type = 0;
				}
				else if (NULL == last_dirent)
				{
					// First entry is deleted, can it be so? First entries must be '.' and '..'
					next_dirent = (struct ext2_dir_entry_2*)(buf + offs + dirent->rec_len);
					new_rec_len = dirent->rec_len + next_dirent->rec_len;
					memmove(dirent, next_dirent, next_dirent->rec_len);
					dirent->rec_len = new_rec_len;
				}
				else
				{
					new_rec_len = last_dirent->rec_len + dirent->rec_len;
					last_dirent->rec_len = new_rec_len;
				}
				ext2_inode_write(this, parent_inode, block_offs, buf, block_size);
				return	0;
			}	
			//...
			last_dirent = dirent;
			offs += dirent->rec_len;
		}
	}

	// `name' not found
	errno = ENOENT;
	return	-1;
}


// Read directory entry that matches a given name
// Returns success indicator (0 means OK, -1 means not found or read failure)
// (!) The caller must ensure that dest has sufficient length
// (!) The copied length is not dirent->rec_len, but exactly name_len + necessary fields
int	ext2_dirent_read(struct fs *this, struct ext2_inode *parent_inode, char *name, struct  ext2_dir_entry_2 *dest)
{
	struct	ext2_dir_entry_2	*dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs;
	unsigned        real_rec_len;
	int	rv;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = ENOTDIR;
		return	-1;
	}

	for (block_offs = 0; block_offs < parent_inode->i_size; block_offs += block_size)
	{
		rv = ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		for (offs = 0; offs < block_size; )
		{
			dirent = (struct ext2_dir_entry_2*)(buf + offs);
			if (memcmp(name, dirent->name, dirent->name_len) == 0)
			{
				real_rec_len = dirent->name_len + offsetof(struct ext2_dir_entry_2, name);
				memcpy(dest, dirent, real_rec_len);
				return	0;
			}
			offs += dirent->rec_len;
		}
	}

	errno = ENOENT;
	return	-1;
}


// Read directory entry with a given index. Useful for traversing directory
// Returns success indicator (0 means OK, -1 means not found or read failure)
int	ext2_dirent_read_idx(struct fs *this, struct ext2_inode *parent_inode, int idx, struct  ext2_dir_entry_2 *dest)
{
	struct	ext2_dir_entry_2	*dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs, cur_pos;
	unsigned        real_rec_len;
	int	i;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = ENOTDIR;
		return	-1;
	}

	for (block_offs = 0, i = 0; block_offs < parent_inode->i_size && i < idx; block_offs += block_size)
	{
		ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		for (offs = 0; offs < block_size; )
		{
			dirent = (struct ext2_dir_entry_2*)(buf + offs);
			if (i == idx)
			{
				real_rec_len = dirent->name_len + offsetof(struct ext2_dir_entry_2, name);
				memcpy(dest, dirent, real_rec_len);
				return	0;
			}
			offs += dirent->rec_len;
			++i;
		}
	}

	errno = ENOENT;
	return	-1;
}

/***********************************************************************************
* Generic FS interface implementation
************************************************************************************/

int	ext2_unmount(struct fs *this)
{
	struct ext2_fs_priv *priv = this->fs_priv;
#ifdef CFG_EXT2_BLOCK_CACHE_SIZE
	if (priv->blocks_cache_keys != NULL)
		free(priv->blocks_cache_keys);
	if (priv->blocks_cache != NULL)
		free(priv->blocks_cache);
#endif
	free(priv->last_blocks_bitmap);
	free(this->fs_priv);
	return	0;
}

// Opens also a directory
int	ext2_file_open(struct fs *this, const char *pathname, int flags, void **fs_entry)
{
	int	rv;
	struct ext2_fs_entry	*new_fs_entry;
	struct ext2_inode	inode;
	uint32_t	inode_num, parent_inode_num;

	rv = ext2_find_inode(this, pathname, &inode, &inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}
	new_fs_entry = calloc(1, sizeof(struct ext2_fs_entry));
	if (NULL == new_fs_entry)
	{
		errno = ENOMEM;
		return	-1;
	}
	memcpy(&new_fs_entry->inode, &inode, sizeof(struct ext2_inode));
	new_fs_entry->inode_num = inode_num;
	*fs_entry = (void*)new_fs_entry;

	return	0;
}

// Creates also a directory
int	ext2_file_creat(struct fs *this, const char *pathname, mode_t mode, void **fs_entry)
{
	int	rv;
	struct ext2_inode	dir_inode;
	struct ext2_inode	new_inode;
	struct ext2_fs_entry	*new_fs_entry;
	uint32_t	inode_num, dir_inode_num;
	char	path[256];
	char	file[256];
	char	*p;

	// Find parent directory
	p = strrchr(pathname, '/');
	if (p != NULL)
	{
		memcpy(path, pathname, p+1-pathname);
		path[p+1-pathname] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, pathname);
	}

	rv = ext2_find_inode(this, path, &dir_inode, &dir_inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}

	// inode_num <- inode index
	inode_num = ext2_alloc_inode(this);
	++inode_num;

	// TODO: support directory creation
	memset(&new_inode, 0, sizeof(new_inode));
	new_inode.i_blocks = 0;
	new_inode.i_size = 0;
	new_inode.i_ctime = time(NULL);
	new_inode.i_atime = time(NULL);
	new_inode.i_mode = EXT2_S_IFREG | 0777;

	ext2_write_inode_tbl(this, inode_num, &new_inode);
	rv = ext2_dirent_create(this, &dir_inode, file, inode_num, new_inode.i_mode);
	if (rv != 0)
	{
		ext2_free_inode(this, inode_num);
		return	rv;
	}
	ext2_write_inode_tbl(this, dir_inode_num, &dir_inode);

	new_fs_entry = calloc(1, sizeof(struct ext2_fs_entry));
	if (NULL == new_fs_entry)
	{
		errno = ENOMEM;
		return	-1;
	}
	memcpy(&new_fs_entry->inode, &new_inode, sizeof(struct ext2_inode));
	new_fs_entry->inode_num = inode_num;
	*fs_entry = (void*)new_fs_entry;

	return	0;
}

ssize_t	ext2_file_read(struct fs *this, void *fs_entry, void *buf, off_t offs, size_t count)
{
	int	rv;
	struct ext2_fs_entry*	ext2_fs_entry = (struct ext2_fs_entry*)fs_entry;

	// TODO: error checking
	rv = ext2_inode_read(this, &ext2_fs_entry->inode, offs, buf, count);

	// Update inode table with new inode contents (access/modification time and size)
	//ext2_write_inode_tbl(this, ext2_fs_entry->inode_num, &ext2_fs_entry->inode);

	return	rv;
}

ssize_t	ext2_file_write(struct fs *this, void *fs_entry, const void *buf, off_t offs, size_t count)
{
	int	rv;
	struct ext2_fs_entry	*ext2_fs_entry = (struct ext2_fs_entry*)fs_entry;
	struct ext2_inode	*inode = &ext2_fs_entry->inode;
	uint32_t	inode_num = ext2_fs_entry->inode_num;
	unsigned	curr_blocks, needed_blocks, need_alloc_blocks;
	struct ext2_fs_priv     *priv = this->fs_priv;

	// If requested write is beyond file size, allocate additional blocks for inode
	curr_blocks = inode->i_size / priv->block_size;
	if (inode->i_size % priv->block_size != 0)
		++curr_blocks;

	needed_blocks = (offs + count) / priv->block_size;
	if ((offs + count) % priv->block_size != 0)
		++needed_blocks;

	if (curr_blocks < needed_blocks)
	{
		need_alloc_blocks = needed_blocks - curr_blocks;
		if (ext2_inode_alloc_blocks_count(this, inode, need_alloc_blocks) == 0)
		{
			errno = EIO;
			return	-1;
		}
		// Update for new data blocks structure
		ext2_write_inode_tbl(this, ext2_fs_entry->inode_num, &ext2_fs_entry->inode);
	}
	if (offs + count > inode->i_size)
		inode->i_size = offs + count;

	rv = ext2_inode_write(this, inode, offs, buf, count);

	// Update inode table with new inode contents (access/modification time and size)
	ext2_write_inode_tbl(this, ext2_fs_entry->inode_num, &ext2_fs_entry->inode);
	return	rv;
}

int	ext2_file_close(struct fs *this, void *fs_entry)
{
	struct ext2_fs_entry    *ext2_fs_entry = (struct ext2_fs_entry*)fs_entry;

	// We update inode here (with access/modification time), there's no point to do it on every read (write has its sense)
	ext2_write_inode_tbl(this, ext2_fs_entry->inode_num, &ext2_fs_entry->inode);

	// If we were buffering I/O, need to commit file here. But we don't buffer, so just free private inode structure
	free(fs_entry);
	return	0;
}

int	ext2_file_unlink(struct fs *this, char *dest)
{
	char	*p;
	char	path[256];
	char	file[256];
	struct ext2_inode	dir_inode;
	uint32_t	dir_inode_num;
	int	rv;

	// Find parent directory
	p = strrchr(dest, '/');
	if (p != NULL)
	{
		memcpy(path, dest, p+1-dest);
		path[p+1-dest] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, dest);
	}

	rv = ext2_find_inode(this, path, &dir_inode, &dir_inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}

	rv = ext2_inode_unlink(this, dest);
	if (rv != 0)
	{
		return	-1;
	}
	rv = ext2_dirent_delete(this, &dir_inode, file);
	if (rv != 0)
		return	-1;

	return	0;
}

// This works, but both src and dest must be complete path names (dest can't be directory name, because then the directory will be removed!)
// Returns an error if destination exists
int	ext2_file_rename(struct fs *this, char *src, char *dest)
{
	char	*p;
	char	path[256];
	char	file[256];
	char	dest_path[256];
	char	dest_file[256];
	struct ext2_inode	dir_inode, dest_dir_inode;
	uint32_t	dir_inode_num, dest_dir_inode_num;
	struct	ext2_dir_entry_2	dirent, dest_dirent;
	int	rv;

	// Find directory from which move is requested
	p = strrchr(src, '/');
	if (p != NULL)
	{
		memcpy(path, src, p+1-src);
		path[p+1-src] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, src);
	}

	// Find directory to which move is requested
	p = strrchr(dest, '/');
	if (p != NULL)
	{
		memcpy(dest_path, dest, p+1-dest);
		dest_path[p+1-dest] = '\0';
		strcpy(dest_file, p+1);
	}
	else
	{
		strcpy(dest_path, "/");
		strcpy(dest_file, dest);
	}

	// Find source parent dir's inode
	rv = ext2_find_inode(this, path, &dir_inode, &dir_inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}
	// Find dest parent dir's inode
	rv = ext2_find_inode(this, dest_path, &dest_dir_inode, &dest_dir_inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}
	// Check that file is not moved onto itself
	if (dir_inode_num == dest_dir_inode_num && strcmp(file, dest_file) == 0)
	{
		errno = EINVAL;
		return	-1;
	}
	// If dest directory entry can be read (i.e. it exists), return an error (the caller must unlink it deliberately before calling rename)
	rv = ext2_dirent_read(this, &dest_dir_inode, dest_file, &dest_dirent);
	if (rv == 0)
	{
		errno = EEXIST;
		return	-1;
	}
	// Read source directory entry
	rv = ext2_dirent_read(this, &dir_inode, file, &dirent);
	if (rv != 0)
		return	-1;
	// Create dest directory entry
	rv = ext2_dirent_create(this, &dest_dir_inode, dest_file, dirent.inode, dirent.file_type);
	if (rv != 0)
		return	-1;
	// Delete source directory entry
	rv = ext2_dirent_delete(this, &dir_inode, file);
	if (rv != 0)
		return	-1;

	return	0;
}

ssize_t	ext2_get_file_size(struct fs *this, void *fs_entry)
{
	struct ext2_inode	*inode = &((struct ext2_fs_entry*)fs_entry)->inode;

	return	inode->i_size;
}

uint32_t	ext2_get_file_attrib(struct fs *this, void *fs_entry)
{
	struct ext2_inode	*inode = &((struct ext2_fs_entry*)fs_entry)->inode;

	return	inode->i_mode;
}

struct dirent	*ext2_read_dir(struct fs *this, void *fs_entry, off_t pos)
{
	struct ext2_inode       *parent_inode = (struct ext2_inode*)fs_entry;
	struct ext2_dir_entry_2	dest;
	static struct dirent	result; 
	struct	ext2_dir_entry_2	*dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs, cur_pos;
	unsigned        real_rec_len;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	memset(&result, 0, sizeof(result));

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = EBADF;
		return	NULL;
	}

	for (block_offs = 0, cur_pos = 0; block_offs < parent_inode->i_size && cur_pos <= pos ; block_offs += block_size)
	{
		ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		for (offs = 0; offs < block_size; )
		{
			dirent = (struct ext2_dir_entry_2*)(buf + offs);
			if (cur_pos >= pos)
			{
				real_rec_len = dirent->name_len + offsetof(struct ext2_dir_entry_2, name);
				result.d_ino = dirent->inode;
				memcpy(result.d_name, dirent->name, dirent->name_len);
				result.d_name[dirent->name_len] = '\0';
				this->dir_pos = cur_pos + dirent->rec_len;
				return	&result;
			}
			offs += dirent->rec_len;
			cur_pos += dirent->rec_len;
		}
	}

	errno = 0;
	return	NULL;
}


// Returns correct offset for given offs - to be used in further read_dir().
size_t	ext2_seek_dir(struct fs *this, void *fs_entry, off_t pos)
{
	struct ext2_inode       *parent_inode = (struct ext2_inode*)fs_entry;
	struct	ext2_dir_entry_2	*dirent;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned	offs, block_offs, cur_pos;
	unsigned        real_rec_len;
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;
	unsigned	block_size = priv->block_size;

	// Parent is not a directory
	if ((parent_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
	{
		errno = EBADF;
		return	-1;
	}

	for (block_offs = 0, cur_pos = 0; block_offs < parent_inode->i_size && cur_pos < pos ; block_offs += block_size)
	{
		ext2_inode_read(this, parent_inode, block_offs, buf, block_size); 
		offs = 0;
		for (offs = 0; offs < block_size; )
		{
			dirent = (struct ext2_dir_entry_2*)(buf + offs);
			if (cur_pos >= pos)
			{
				real_rec_len = dirent->name_len + offsetof(struct ext2_dir_entry_2, name);
				this->dir_pos = cur_pos;
				return	cur_pos;
			}
			offs += dirent->rec_len;
			cur_pos += real_rec_len;
		}
	}

	errno = ENOENT;
	return	-1;
}

static int file_stat(struct fs *this, struct ext2_inode	*inode, uint32_t inode_num, struct stat *buf)
{
	memset(buf, 0, sizeof(buf));
	buf->st_ino = inode_num;
	buf->st_mode = inode->i_mode;
	buf->st_uid = inode->i_uid;
	buf->st_gid = inode->i_gid;
	buf->st_size = inode->i_size;
	buf->st_atime = inode->i_atime;
	buf->st_mtime = inode->i_mtime;
	buf->st_ctime = inode->i_ctime;
	return	0;
}

int     ext2_fstat(struct fs *this, void *fs_entry, struct stat *buf)
{
	struct ext2_fs_entry       *ext2_fs_entry = fs_entry;
	struct ext2_inode	*inode = &ext2_fs_entry->inode;
	uint32_t	inode_num = ext2_fs_entry->inode_num;

	return	file_stat(this, inode, inode_num, buf);
}

int     ext2_stat(struct fs *this, const char *path, struct stat *buf)
{
	int	rv;
	struct ext2_inode	inode;
	uint32_t	inode_num;

	rv = ext2_find_inode(this, path, &inode, &inode_num, 0);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}

	return	file_stat(this, &inode, inode_num, buf);
}


void	ext2_sync(struct fs *this)
{
	struct ext2_fs_priv     *priv = (struct ext2_fs_priv*)this->fs_priv;

	ext2_update_superblock(this);
	ext2_write_block(this, priv->last_blocks_bitmap_pos, priv->last_blocks_bitmap);
}


int	ext2_mount(struct fs *this, const char *mount_point, int disk_num, unsigned start_sect)
{
	struct ext2_fs_priv	*priv;
	struct ext2_super_block	super_block;
	int	rv;
	off_t	start_offs;
	off_t	group_desc_offs;

	start_offs = (off_t)start_sect * DISK_SECTOR_SIZE;
	
	rv = disks[disk_num].read(&disks[disk_num], start_offs + 0x400 /* skip boot block */, &super_block, sizeof(super_block));

	if (rv != 0)
		return	rv;

	if (super_block.s_magic != EXT2_SIGNATURE)
	{
		errno = ENODEV;
		return -1;	
	}

	priv = calloc(1, sizeof(struct ext2_fs_priv));
	if (NULL == priv)
	{
		errno = ENOMEM;
		return	-1;
	}
	// Dump superblock (debug)
	dump_super_block(&super_block);
	memcpy(&priv->super_block, &super_block, sizeof(struct ext2_super_block));
	priv->block_size = 1 << super_block.s_log_block_size + 10;

	// Calculate number of block groups
	priv->num_groups = priv->super_block.s_blocks_count / priv->super_block.s_blocks_per_group;
	priv->last_group_blocks = priv->super_block.s_blocks_per_group;
	if (priv->super_block.s_blocks_count % priv->super_block.s_blocks_per_group != 0)
	{
		++priv->num_groups;
		priv->last_group_blocks = priv->super_block.s_blocks_count % priv->super_block.s_blocks_per_group;
	}

	// Align on block boundary (boot block is exactly 1024 bytes)
	group_desc_offs = start_offs + 0x400 + sizeof(struct ext2_super_block); /* 0x400, actually */
	if (sizeof(struct ext2_super_block) + 0x400 < priv->block_size)
		group_desc_offs = start_offs + priv->block_size;

	// Get block groups descriptors
	// Allocate group descriptors block
	priv->group_desc_block = calloc(1, priv->block_size);
	if (priv->group_desc_block == NULL)
	{
err_ret1:
		free(priv);
		errno = ENOMEM;
		return	-1;
	}
	disks[disk_num].read(&disks[disk_num], group_desc_offs, priv->group_desc_block, priv->block_size);

#ifdef	CFG_EXT2_BLOCK_CACHE_SIZE
	// Allocate cache for blocks
	// If allocation failed, go on working without cache
	priv->blocks_cache_keys = NULL;
	priv->blocks_cache = NULL;
	priv->blocks_cache = calloc(1, CFG_EXT2_BLOCK_CACHE_SIZE * priv->block_size);
	if (priv->blocks_cache != NULL)
	{
		priv->blocks_cache_keys = calloc(1, CFG_EXT2_BLOCK_CACHE_SIZE * sizeof(struct blocks_cache_key));
		if (priv->blocks_cache_keys == NULL)
		{
			free(priv->blocks_cache);
			priv->blocks_cache = NULL;
		}
	}
#endif

	// Allocate blocks bitmap buffer. It will be held in memory as long as next allocated/deallocated block goes into the same group.
	// When another group is involveld, the buffer will be flushed and refilled
	priv->last_blocks_bitmap = calloc(1, priv->block_size);
	if (priv->last_blocks_bitmap == NULL)
	{
		serial_printf("%s(): Can't allocate buffer for most recently used blocks bitmap (block_size=%u)\n", __func__, priv->block_size);
		goto	err_ret1;
	}
	priv->last_blocks_bitmap_pos = 0;				// Never a valid value, serves as indicator that there's no bitmap currently loaded into buffer

	// Fill FS'es structure
	this->fs_priv = priv;
	this->disk_num = disk_num;
	this->start_offs = (off_t)start_sect * DISK_SECTOR_SIZE;
	this->mount_point = calloc(1, strlen(mount_point) + 1);
	strcpy(this->mount_point, mount_point);
	
	this->file_open = ext2_file_open;
	this->file_creat = ext2_file_creat;
	this->file_read = ext2_file_read;
	this->file_write = ext2_file_write;
	this->file_close = ext2_file_close;
	this->file_unlink = ext2_file_unlink;
	this->file_rename = ext2_file_rename;
	this->unmount = ext2_unmount;
	this->get_file_size = ext2_get_file_size;
	this->get_file_attrib = ext2_get_file_attrib;
	this->read_dir = ext2_read_dir;
	this->seek_dir = ext2_seek_dir;
	this->stat = ext2_stat;
	this->fstat = ext2_fstat;
	this->sync = ext2_sync;

	this->fs_type = FS_EXT2;
	this->dir_pos = 0;
	
	// Test FS with commands
//
// Below commands work (VmWare)
//
//	printf("# ls <<%s>>\r\n", "/");
//	ext2_cmd_ls(this, "/");
//	printf("# ls /testdir/\r\n");
//	ext2_cmd_ls(this, "/testdir/");
//	printf("# cat /bbb.txt\r\n");
//	ext2_cmd_cat(this, "/bbb.txt");
//	printf("# cat /testdir/aaa.txt\r\n");
//	ext2_cmd_cat(this, "/testdir/aaa.txt");
//
	
	return	0;
}


