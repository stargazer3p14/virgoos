/******************************************
 *	Implementation of FAT fs for SeptOS
 *****************************************/

#include "errno.h"
#include <stddef.h>
#include "config.h"
#include "sosdef.h"
#include "io.h"

extern struct disk	disks[MAX_DISKS];

#define	FAT_SECTOR_SIZE	512
#define	FAT_MAX_CLUSTER_SIZE	0x8000

// FAT12 definitions
#define	FAT12_FREE_CLUSTER	0
#define	FAT12_FIRST_USED_CLUSTER	2
#define	FAT12_LAST_USED_CLUSTER	0xFEF
#define	FAT12_BAD_CLUSTER	0xFF7
#define	FAT12_LAST_CLUSTER_MIN	0xFF8
#define	FAT12_LAST_CLUSTER_MAX	0xFFF

// FAT16 definitions
#define	FAT16_FREE_CLUSTER	0
#define	FAT16_FIRST_USED_CLUSTER	2
#define	FAT16_LAST_USED_CLUSTER	0xFFEF
#define	FAT16_BAD_CLUSTER	0xFFF7
#define	FAT16_LAST_CLUSTER_MIN	0xFFF8
#define	FAT16_LAST_CLUSTER_MAX	0xFFFF

// File attributes
#define	FAT_FILE_ATTR_RDONLY	0x1
#define	FAT_FILE_ATTR_HIDDEN	0x2
#define	FAT_FILE_ATTR_SYSTEM	0x4
#define	FAT_FILE_ATTR_VOL	0x8
#define	FAT_FILE_ATTR_SUBDIR	0x10
#define	FAT_FILE_ATTR_ARCHIVE	0x20
#define	FAT_FILE_ATTR_DEVICE	0x40			// Device (internal use only, never found on disk)
#define	FAT_FILE_ATTR_LFNENTRY	(FAT_FILE_ATTR_RDONLY | FAT_FILE_ATTR_HIDDEN | FAT_FILE_ATTR_SYSTEM | FAT_FILE_ATTR_VOL)

#define	FAT_ERASED_DIR_ENTRY	0xE5
#define	FAT_AVAIL_DIR_ENTRY	0x0

enum	{FAT_TYPE_FAT12 = 1, FAT_TYPE_FAT16, FAT_TYPE_FAT32};

#define	FAT_EXT_BOOT_SIG	0x29

// BIOS Parameter Block structure, which plays role of a kind of superblock in FAT
struct fat_boot_sect 
{
	uint8_t		jmp_bootcode[3];		// x86 JMP instruction, unused as a parameter
	uint8_t		oem_name[8];
	uint16_t	bytes_per_sector;		// First parameter of BPB (512)
	uint8_t		sectors_per_cluster;		// Power of 2 in range [1..128]. Must not define clusters bigger than 32K (so actually, in [1..64]
	uint16_t	num_reserved_sectors;		// 1 for FAT12/FAT16, usually 32 for FAT32
	uint8_t		num_fats;			// Should be 2
	uint16_t	root_dir_max_entries;		// FAT12/FAT16. Should be 0 for FAT32. Should be such that the root directory ends on a sector boundary
							// (i.e. such that its size becomes a multiple of the sector size). 224 is typical for floppy disks.
	uint16_t	sectors_total;			// if zero, use 4 byte value at offset 0x20
	uint8_t		media_desc;			// Same value of media descriptor should be repeated as first byte of each copy of FAT
	uint16_t	sectors_per_fat;		// FAT12/FAT16
	uint16_t	sectors_per_track;
	uint16_t	num_heads;
	uint32_t	hidden_sectors;			// Count of hidden sectors preceding the partition that contains this FAT volume.
							// This field should always be zero on media that are not partitioned.
	uint32_t	sectors_total_long;		// Total sectors (if greater than 65535; otherwise, see offset 0x13)

	uint8_t		phys_drive_no;			// Physical drive number (0x00 for removable media, 0x80 for hard disks)
	uint8_t		reserved1;
	uint8_t		ext_boot_sig;			// Extended boot signature. (Should be 0x29. Indicates that the following 3 entries exist.)
	uint32_t	id;				// ID (serial number)
	unsigned char	volume_label[11];		// Volume Label, padded with blanks (0x20)
	unsigned char	fat_type[8];			// FAT file system type, padded with blanks (0x20), e.g.: "FAT12   ", "FAT16   "
} __attribute__ ((__packed__));


struct	fat_dir_entry 
{
	unsigned char	name[8];			// Upper-case, space-padded. First character '\0' means this is the last entry
							// '\x05' means that the real character is '\xE5'. '.' means that it's "." or ".." entry
							// '\xE5' means that the entry is deleted, but available for undelete
	unsigned char	ext[3];				// Upper-case, space-padded.
#define	ROOT_DIR_ATTRIB	0xFF
	unsigned char	attrib;				// Attributes
	unsigned char	reserved1;
	unsigned char	ctime_10ms;			// Create time, fine resolution: 10ms units, values from 0 to 199.
	uint16_t	ctime;				// Create time. The hour, minute and second are encoded according to the following bitmap:
							// [0..4] = seconds/2 (0-29); [5-10] = minutes (0-59); [11-15] = hours (0-23)
	uint16_t	cdate;				// Create date. The year, month and day are encoded according to the following bitmap:
							// [0..4] = day (1-31); [5-8] = month (1-12); [9-15] = year since 1980 (0 = 1980, 127 = 2107)
	uint16_t	adate;				// Last access date; same format as `cdate'
	uint16_t	ea_index;			// EA-Index (used by OS/2 and NT) in FAT12 and FAT16, High 2 bytes of first cluster number in FAT32
	uint16_t	mtime;				// Last modified time; same format as `ctime'
	uint16_t	mdate;				// Last modified date; same format as `cdate'
#define ROOT_DIR_FIRST_CLUSTER	0
	uint16_t	first_cluster;			// First cluster in FAT12 and FAT16. Low 2 bytes of first cluster in FAT32.
							// Entries with the Volume Label flag, subdirectory ".." pointing to root, and empty files with size 0 
							// should have first cluster 0.
	uint32_t	file_size;			// File size in bytes. Entries with the Volume Label or Subdirectory flag set should have a size of 0.
} __attribute__ ((__packed__));


struct	fat_time
{
	int	hour;
	int	minute;
	int	second;
	int	ms;
};

struct	fat_date
{
	int	day;
	int	month;
	int	year;
};


// Per-partition definitions
struct fat_fs_priv
{
	unsigned	free_cluster_val;
	unsigned	first_used_cluster_val;
	uint8_t	fat_type;
	unsigned	last_cluster_min_val;
	unsigned	bad_cluster_val;
	unsigned	last_used_cluster_val;
	struct fat_boot_sect	boot_sector;
	unsigned	cluster_size;
	unsigned	total_sectors;
	off_t	data_start;					// Start of data area
	off_t	root_dir_region;				// Start of root directory region
};

// Convenience functions
static void	dump_fat_info(struct fs *this, struct fat_boot_sect *boot_sector);
void	fat_ctime_to_time(unsigned ctime, unsigned char ms, struct fat_time *ftime);
void	fat_cdate_to_date(unsigned ctime, struct fat_date *fdate);
ssize_t	fat_file_read(struct fs *this, void *fs_entry, void *buffer, off_t offs, size_t size);
ssize_t	fat_file_write(struct fs *this, void *fs_entry, const void *buffer, off_t offs, size_t size);


unsigned char	buf[FAT_MAX_CLUSTER_SIZE];

static void	dump_fat_info(struct fs *this, struct fat_boot_sect *boot_sector)
{
	int	i;
	char	buf[256];
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;

	serial_printf("FAT filesystem summary:\n");
	serial_printf("------------------------------------------------------------------------\n");
	serial_printf("JMP _bootcode: 0x%02X 0x%02X 0x%02X\n", (unsigned)boot_sector->jmp_bootcode[0], (unsigned)boot_sector->jmp_bootcode[1], (unsigned)boot_sector->jmp_bootcode[2]);
	memcpy(buf, boot_sector->oem_name, 8);
	buf[8] = '\0';
	serial_printf("OEM name: %s\n", buf);
	serial_printf("Bytes per sector: %u\n", (unsigned)boot_sector->bytes_per_sector);
	serial_printf("Sectors per cluster: %u\n", (unsigned)boot_sector->sectors_per_cluster);
	serial_printf("Number of reserved sectors: %u\n", (unsigned)boot_sector->num_reserved_sectors);
	serial_printf("Number of FATs: %u\n", (unsigned)boot_sector->num_fats);
	serial_printf("Maximum number of root directory entries: %u\n", (unsigned)boot_sector->root_dir_max_entries);
	serial_printf("Total number of sectors: %u\n", (unsigned)boot_sector->sectors_total != 0 ?  (unsigned)boot_sector->sectors_total : boot_sector->sectors_total_long);
	serial_printf("Media descriptor: 0x%X\n",  (unsigned)boot_sector->media_desc);
	serial_printf("Sectors per FAT: %u\n",  (unsigned)boot_sector->sectors_per_fat);
	serial_printf("Sectors per track: %u\n",  (unsigned)boot_sector->sectors_per_track);
	serial_printf("Number of heads: %u\n",  (unsigned)boot_sector->num_heads);
	serial_printf("Number of hidden sectors: %u\n",  (unsigned)boot_sector->hidden_sectors);
	serial_printf("Physical driver number (BIOS): %u\n", (unsigned)boot_sector->phys_drive_no);
	serial_printf("Extended boot signature: 0x%X\n", (unsigned)boot_sector->ext_boot_sig);
	serial_printf("ID: 0x%08X\n", (unsigned)boot_sector->id);
	memcpy(buf, boot_sector->volume_label, 11);
	buf[11] = '\0';
	serial_printf("Volume label: %s\n", buf);
	memcpy(buf, boot_sector->fat_type, 8);
	buf[8] = '\0';
	serial_printf("FAT type: %s\n", buf);
	serial_printf("Cluster size: %u\n", priv->cluster_size);
	serial_printf("Root directory region start in bytes (sectors): %u (%u)\n", (unsigned)priv->root_dir_region, (unsigned)priv->root_dir_region / FAT_SECTOR_SIZE);
	serial_printf("Data area start in bytes (sectors): %u (%u)\n", (unsigned)priv->data_start, (unsigned)priv->data_start / FAT_SECTOR_SIZE);
}


static void	dump_dir_entry(struct fat_dir_entry *dir_entry)
{
	char	fname[9];
	char	fext[4];
	struct fat_date	fdate;
	struct fat_time	ftime;

	serial_printf("Directory entry dump:\n"
		"---------------------\n");
	memcpy(fname, dir_entry->name, 8);
	fname[8] = '\0';
	memcpy(fext, dir_entry->ext, 3);
	fext[3] = '\0';
	serial_printf("Name: %s.%s\n", fname, fext);
	serial_printf("Attributes: %02X ( ", dir_entry->attrib);
	if (FAT_FILE_ATTR_RDONLY & dir_entry->attrib)
		serial_printf("read-only ");
	if (FAT_FILE_ATTR_HIDDEN & dir_entry->attrib)
		serial_printf("hidden ");
	if (FAT_FILE_ATTR_SYSTEM & dir_entry->attrib)
		serial_printf("system ");
	if (FAT_FILE_ATTR_VOL & dir_entry->attrib)
		serial_printf("volume-label ");
	if (FAT_FILE_ATTR_SUBDIR & dir_entry->attrib)
		serial_printf("sub-dir ");
	if (FAT_FILE_ATTR_ARCHIVE & dir_entry->attrib)
		serial_printf("archived ");
	if (FAT_FILE_ATTR_DEVICE & dir_entry->attrib)
		serial_printf("device ");
	if (FAT_FILE_ATTR_LFNENTRY == (dir_entry->attrib & FAT_FILE_ATTR_LFNENTRY))
		serial_printf(" -- LFN entry -- ");
	serial_printf(")\n");
	fat_ctime_to_time(dir_entry->ctime, dir_entry->ctime_10ms, &ftime);
	serial_printf("Creation time: %u:%u:%u.%u\n", ftime.hour, ftime.minute, ftime.second, ftime.ms);
	fat_cdate_to_date(dir_entry->cdate, &fdate);
	serial_printf("Creation date: %u/%u/%u\n", fdate.day, fdate.month, fdate.year);
	fat_cdate_to_date(dir_entry->adate, &fdate);
	serial_printf("Access date: %u/%u/%u\n", fdate.day, fdate.month, fdate.year);
	fat_ctime_to_time(dir_entry->ctime, 0, &ftime);
	serial_printf("Modification time: %u:%u:%u.%u\n", ftime.hour, ftime.minute, ftime.second, ftime.ms);
	fat_cdate_to_date(dir_entry->mdate, &fdate);
	serial_printf("Modification date: %u/%u/%u\n", fdate.day, fdate.month, fdate.year);
	serial_printf("File size: %u\n", (unsigned)dir_entry->file_size);
	serial_printf("First cluster: %u\n", (unsigned)dir_entry->first_cluster);
	serial_printf("\n");
}


// Returns next cluster correct for current FAT type
// For unknown FAT type return bad cluster value
// (!) Accepts cluster *index* (0-based) and returns cluster *index* (0-based), and not cluster number! (biased by 2)
unsigned	fat_next_cluster(struct fs *this, unsigned curr_cluster)
{
	unsigned char	buf[FAT_SECTOR_SIZE];
	off_t	offs, sect_offs;
	unsigned	factor;
	struct fat_fs_priv      *priv = (struct fat_fs_priv*)this->fs_priv;

	if (!(curr_cluster >= priv->first_used_cluster_val - 2 && curr_cluster <= priv->last_used_cluster_val - 2))
		return	curr_cluster;
	if (FAT_TYPE_FAT16 == priv->fat_type)
		offs = (off_t) FAT_SECTOR_SIZE * priv->boot_sector.num_reserved_sectors + (curr_cluster + 2) * 2;
	else if (FAT_TYPE_FAT12 == priv->fat_type)
		offs = (off_t) FAT_SECTOR_SIZE * priv->boot_sector.num_reserved_sectors + (curr_cluster + 2) * 3 / 2;
	sect_offs = offs;
	offs = offs % FAT_SECTOR_SIZE;
	if (sect_offs % FAT_SECTOR_SIZE != 0)
		sect_offs -= offs;

	disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + sect_offs, buf, FAT_SECTOR_SIZE);

	if (FAT_TYPE_FAT16 == priv->fat_type)
	{
		curr_cluster = (unsigned)*(uint16_t*)(buf + offs);
		return	curr_cluster-2;
	}
	else if (FAT_TYPE_FAT12 == priv->fat_type)
	{
		curr_cluster = (unsigned)*(uint16_t*)(buf + offs);
		if ((curr_cluster-2) & 1)
			curr_cluster = (unsigned)*(uint16_t*)(buf + offs) >> 4;
		curr_cluster &= 0xFFF;
		return	curr_cluster-2;
	}
	
	return	priv->bad_cluster_val;
}


// Returns 1 if there are `count' available clusters, 0 otherwise
int	fat_check_avail_clusters(struct fs *this, unsigned count)
{
	unsigned	curr_sector;
	unsigned char	buf[FAT_SECTOR_SIZE];
	int	i;
	unsigned	carried_val;
	int	carried_bytes = 0;
	unsigned	free_count = 0;
	unsigned	test_val;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	
	for (curr_sector = 0; curr_sector < priv->boot_sector.sectors_per_fat; ++curr_sector)
	{
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (curr_sector + priv->boot_sector.num_reserved_sectors) * FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE);
		
		for (i = 0; i < FAT_SECTOR_SIZE && free_count < count;)
		{
			if (FAT_TYPE_FAT16 == priv->fat_type)
			{
				if (priv->free_cluster_val == *(uint16_t*)(buf+i))
					++free_count;
				if (free_count == count)
					return	1;
				i += 2;
			}
			else if (FAT_TYPE_FAT12 == priv->fat_type)
			{
				if (0 == i && carried_bytes != 0)
				{
					if (1 == carried_bytes)
						test_val = carried_val + ((*(uint16_t*)(buf) & 0xFF) << 4);
					else if (2 == carried_bytes)
						test_val = carried_val + ((*(uint16_t*)(buf) & 0xF) << 8);
					
					if (test_val == priv->free_cluster_val)
						++free_count;
					carried_bytes = 0;
					carried_val = 0;

					if (free_count == count)
						return	1;
					continue;
				}

				if (priv->free_cluster_val == (*(uint16_t*)(buf+i) & 0xFFF))
					++free_count;
				if (free_count == count)
					return	1;
				if (priv->free_cluster_val == ((*(uint16_t*)(buf+i) >> 12) + (*(uint16_t*)(buf+i+2) << 4) & 0xFFF))
					++free_count;
				if (free_count == count)
					return	1;
				i += 3;

				if (i >= FAT_SECTOR_SIZE)
				{
					carried_bytes = i - FAT_SECTOR_SIZE;
					if (1 == carried_bytes)
						carried_val = *(unsigned char*)(buf+FAT_SECTOR_SIZE-1);
					else if (2 == carried_bytes)
						carried_val = *(uint16_t*)((buf+FAT_SECTOR_SIZE-1));
				}
			}
		}
	}

	return	0;
}

// Writes to disk FAT entry for a given cluster, with given value
// (!) 'cluster' is a 2-based FAT cluster value, not a 0-based index!
// And 'value' is a 2-based FAT cluster value! (necessary because it may 'last cluster marker', 'bad cluster marker' or 'free cluster marker'
// Returns 0 for success, -1 for error (invalid cluster number etc.)
int	fat_commit_entry(struct fs *this, unsigned cluster, unsigned value)
{
	unsigned char	buf[FAT_SECTOR_SIZE*2];
	off_t	sect_offs = 0;
	unsigned	offs = 0;
	unsigned	curr_fat;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	
	if (cluster >= priv->last_used_cluster_val-2)
		return	-1;
	if (cluster >= priv->total_sectors / priv->boot_sector.sectors_per_cluster)
		return	-1;

	if (FAT_TYPE_FAT16 == priv->fat_type)
	{
		sect_offs = priv->boot_sector.num_reserved_sectors * FAT_SECTOR_SIZE + cluster * 2;
		offs = sect_offs % FAT_SECTOR_SIZE;
		if (offs != 0)
			sect_offs -= offs;

		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + sect_offs, buf, FAT_SECTOR_SIZE);
		*(uint16_t*)(buf + offs) = value;
		for (curr_fat = 0; curr_fat < priv->boot_sector.num_fats; ++curr_fat)
		{
			disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + sect_offs + (off_t)curr_fat * priv->boot_sector.sectors_per_fat * FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE);
		}
	}
	else if (FAT_TYPE_FAT12 == priv->fat_type)
	{
		sect_offs = priv->boot_sector.num_reserved_sectors * FAT_SECTOR_SIZE + cluster * 3 / 2;
		offs = sect_offs % FAT_SECTOR_SIZE;
		if (offs != 0)
			sect_offs -= offs;

		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + sect_offs, buf, FAT_SECTOR_SIZE * 2);
		if (!(cluster & 1))
			*(uint16_t*)(buf + offs) = *(uint16_t*)(buf + offs) & 0xFFF | value;
		else
		{
			*(uint16_t*)(buf + offs) = *(uint16_t*)(buf + offs) & 0xF000 | value << 12 & 0xF000;
			*(uint16_t*)(buf + offs + 2) = *(uint16_t*)(buf + offs + 2) & 0xFF | value >> 4 & 0xFF;
		}
		
		for (curr_fat = 0; curr_fat < priv->boot_sector.num_fats; ++curr_fat)
		{
			disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + sect_offs + (off_t)curr_fat * priv->boot_sector.sectors_per_fat * FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE * 2);
		}
	}
}


// Allocate a list of clusters, `count' number
// Return first cluster index or (unsigned)-1 on error
// (!) Returns *index* (0-based), not cluster number (2-based) 
unsigned	fat_alloc_clusters_count(struct fs *this, unsigned count)
{
	unsigned	ret_cluster = 0;			// First allocated cluster
	unsigned	curr_sector;
	unsigned char	buf[FAT_SECTOR_SIZE*3];
	int	i;
	unsigned	alloc_count = 0;
	unsigned	prev_cluster;
	unsigned	curr_cluster;
	off_t	curr_sect_offs;
	unsigned	curr_offs;
	int	nsectors;
	struct fat_fs_priv	*priv = (struct fat_fs_priv *)this->fs_priv;

	if (!fat_check_avail_clusters(this, count))
		return	(unsigned)-1;

	for (curr_sector = 0; curr_sector < priv->boot_sector.sectors_per_fat; curr_sector += 3)
	{
		nsectors = priv->boot_sector.sectors_per_fat - curr_sector;
		if (nsectors > 3)
			nsectors = 3;
			
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + (curr_sector + priv->boot_sector.num_reserved_sectors) * FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE * nsectors);

		// A whole mess seeking and updating FAT12 entries is eliminated by reading and processing
		// 3 adjacent FAT sectors at a time
		for (i = 0; i < FAT_SECTOR_SIZE * nsectors && alloc_count < count;)
		{

			if (FAT_TYPE_FAT16 == priv->fat_type)
			{
				// Reserve first "available" entry - don't touch! (needed?)
				if (0 == curr_sector && 0 == i)
					i = 6;

				if (priv->free_cluster_val == *(uint16_t*)(buf+i))
				{
					curr_cluster = (curr_sector * FAT_SECTOR_SIZE + i) / 2;
					++alloc_count;
					// Update previous cluster's entry with value of this cluster
					if (prev_cluster != 0)
						fat_commit_entry(this, prev_cluster, curr_cluster);
					prev_cluster = curr_cluster;
					if (0 == ret_cluster)
						ret_cluster = curr_cluster;
					if (alloc_count == count)
					{
						// Commit current cluster with 'last cluster' marker
						fat_commit_entry(this, curr_cluster, priv->last_cluster_min_val);
						return	ret_cluster-2;
					}
				}
				i += 2;
			}
			else if (FAT_TYPE_FAT12 == priv->fat_type)
			{
				if (priv->free_cluster_val == (*(uint16_t*)(buf+i) & 0xFFF))
				{
					curr_cluster = (curr_sector * FAT_SECTOR_SIZE + i) * 2 / 3;
					if (0 == ret_cluster)
						ret_cluster = curr_cluster;
					++alloc_count;
					// Update previous cluster's entry with value of this cluster
					if (prev_cluster != 0)
						fat_commit_entry(this, prev_cluster, curr_cluster);
					prev_cluster = curr_cluster;
					if (0 == ret_cluster)
						ret_cluster = curr_cluster;
					if (alloc_count == count)
					{
						fat_commit_entry(this, curr_cluster, priv->last_cluster_min_val);
						return	ret_cluster;
					}
				}
				if (priv->free_cluster_val == ((*(uint16_t*)(buf+i) >> 12) + (*(uint16_t*)(buf+i+2) << 4) & 0xFFF))
				{
					curr_cluster = (curr_sector * FAT_SECTOR_SIZE + i) * 2 / 3 + 1;
					++alloc_count;
					// Update previous cluster's entry with value of this cluster
					if (prev_cluster != 0)
						fat_commit_entry(this, prev_cluster, curr_cluster);
					prev_cluster = curr_cluster;
					if (alloc_count == count)
					{
						fat_commit_entry(this, curr_cluster, priv->last_cluster_min_val);
						return	ret_cluster-2;
					}
				}
				i += 3;
			}
		}
	}	

	return	(unsigned)-1;
}


// Compares 8.3 name found in directory entry with search_name
int	fat_cmp_fnames(struct fat_dir_entry *dir_entry, char *search_name)
{
	char	fname[256];
	char	fext[256];
	int	k;

	memcpy(fname, dir_entry->name, 8);
	fname[8] = '\0';
	for (k = 7; k >= 0 && ' ' == fname[k]; --k)
		fname[k] = '\0';
	memcpy(fext, dir_entry->ext, 3);
	fext[3] = '\0';
	for (k = 2; k >= 0 && ' ' == fext[k]; --k)
		fext[k] = '\0';
	if (fext[0] != '\0')
		sprintf(fname + strlen(fname), ".%s", fext);
	return	strcasecmp(fname, search_name);
}

// Convert file name to 8.3 space-stuffed format
// (!) name must already be in 8.3 dotted format
// (!) fname and fext must be at least enough for 9.4 chars: we store terminating 0, too
void	fat_name_to_fname83(char *name, char *fname, char *fext)
{
	int	i, j;
	int	dot;
	char	*p;

	memset(fname, ' ', 8);
	memset(fext, ' ', 3);
	fname[8] = '\0';
	fext[3] = '\0';

	p = strchr(name, '.');
	if (p != NULL)
		dot = p - name;
	else
		dot = 100;

	for (i = 0; i < dot && i < strlen(name) && i < 8; ++i)
		fname[i] = toupper(name[i]);

	if (dot <= 8)
		for (j = 0, i = dot + 1; i < strlen(name) && i < 8+3+1 && j < 3; ++i, ++j)
			fext[j] = toupper(name[i]);
}

void	fat_ctime_to_time(unsigned ctime, unsigned char ms, struct fat_time *ftime)
{
	ftime->hour = ctime >> 11 & 0x1F;
	ftime->minute = ctime >> 5 & 0x3F;
	ftime->second = ctime & 0x1F;
	if (ms > 100)
	{
		ms -= 100;
		++ftime->second;
	}
	ftime->ms = ms * 10;
}

void	fat_time_to_ctime(struct fat_time *ftime, uint16_t *ctime, unsigned char *ms)
{
	*ctime = (ftime->hour & 0x1F) << 11 | (ftime->minute & 0x3F) << 5 | (ftime->second >> 1 & 0x1F);
	if (NULL == ms)
		return;
	*ms = ftime->ms / 10;
	if (ftime->second & 1)
		*ms += 100;
}

void	fat_cdate_to_date(unsigned cdate, struct fat_date *fdate)
{
	fdate->day = cdate & 0x1F;
	fdate->month = cdate >> 5 & 0xF;
	fdate->year = (cdate >> 9 & 0x7F) + 1980;
}

void	fat_date_to_cdate(struct fat_date *fdate, uint16_t *cdate)
{
	*cdate = (fdate->year - 1980 & 0x1F) << 9 | (fdate->month & 0xF) << 5 | (fdate->day & 0xF);
}


// Get real file size where it can't be read from directory entry (for sub-directories and volume labels)
ssize_t	fat_get_file_size(struct fs *this, void *fs_entry)
{
	unsigned	curr_cluster;
	unsigned	size;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	struct fat_dir_entry *dir_entry = fs_entry;

	// For regular files, read size from directory entry
	if ((dir_entry->attrib & (FAT_FILE_ATTR_SUBDIR | FAT_FILE_ATTR_VOL)) == 0)
		return	dir_entry->file_size;

	size = priv->cluster_size;		// File is at least 1 cluster long
	for (curr_cluster = dir_entry->first_cluster; curr_cluster <= priv->last_used_cluster_val - 2; curr_cluster = fat_next_cluster(this, curr_cluster))
		size += priv->cluster_size;
	return	size;
}

uint32_t	fat_get_file_attrib(struct fs *this, void *fs_entry)
{
	struct fat_dir_entry *dir_entry = fs_entry;
	uint32_t	fat_attrib, septos_attrib;

	fat_attrib = dir_entry->attrib;
	septos_attrib = 0;
	septos_attrib |= (S_IRUSR | S_IRGRP | S_IROTH) | (S_IXUSR | S_IXGRP | S_IXOTH);
	// Translate FAT attributes to SeptemberOS UNIX-like attributes
	if (!(FAT_FILE_ATTR_RDONLY & fat_attrib))
		septos_attrib |= (S_IWUSR | S_IWGRP | S_IWOTH);
	if (FAT_FILE_ATTR_SUBDIR & fat_attrib)
		septos_attrib |= S_IFDIR;
	if (FAT_FILE_ATTR_DEVICE & fat_attrib)
		septos_attrib |= S_IFCHR;
	return	septos_attrib;
}

//////////////////////////////////////////////////////////
// Because of its special structure, root directory has distinct read/update/write functions
//////////////////////////////////////////////////////////

// Read directory entry that matches a given name
int	fat_root_dir_entry_read(struct fs *this, char *name, struct fat_dir_entry *dest)
{
	unsigned char	buf[FAT_SECTOR_SIZE];
	int	i, j;
	struct fat_dir_entry	*d;
	unsigned	curr_sector_offs = 0;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	int	rv;

	// If name == "", "read" the root directory itself by placing special values in 'dest'
	if (name[0] == '\0')
	{
		dest->attrib = ROOT_DIR_ATTRIB;
		dest->first_cluster = ROOT_DIR_FIRST_CLUSTER;
		return	0;
	}

	for (i = 0; i < priv->boot_sector.root_dir_max_entries;)
	{
		rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);

		for (j = 0; j < FAT_SECTOR_SIZE; j += sizeof(struct fat_dir_entry))
		{
			d = (struct fat_dir_entry*)(buf + j);

			// First character in dir. entry name '\0' - means this is the last entry (and empty)
			if (d->name[0] == '\0')
				return	-1;

//			if ((d->attrib & FAT_FILE_ATTR_LFNENTRY) != FAT_FILE_ATTR_LFNENTRY)
			{
				if (fat_cmp_fnames(d, name) == 0)
				{
					memcpy(dest, d, sizeof(struct fat_dir_entry));
					return	0;
				}
			}
			++i;
		}
		curr_sector_offs += FAT_SECTOR_SIZE;
	}

	return	-1;
}

// Read directory entry that matches a given name
int	fat_root_dir_entry_update(struct fs *this, char *name, struct fat_dir_entry *src)
{
	unsigned char	buf[FAT_SECTOR_SIZE];
	int	i, j;
	struct fat_dir_entry	*d;
	unsigned	curr_sector_offs = 0;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;

	for (i = 0; i < priv->boot_sector.root_dir_max_entries; i += FAT_SECTOR_SIZE / sizeof(struct fat_dir_entry))
	{
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);

		for (j = 0; j < FAT_SECTOR_SIZE; j += sizeof(struct fat_dir_entry))
		{
			d = (struct fat_dir_entry*)(buf + j);
			if (fat_cmp_fnames(d, name) == 0)
			{
				memcpy(d, src, sizeof(struct fat_dir_entry));
				disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);
				return	0;
			}
		}
		curr_sector_offs += FAT_SECTOR_SIZE;
	}

	return	-1;
}

// Read directory entry that matches a given name
int	fat_root_dir_entry_write(struct fs *this, struct fat_dir_entry *src)
{
	unsigned char	buf[FAT_SECTOR_SIZE];
	int	i, j;
	struct fat_dir_entry	*d;
	unsigned	curr_sector_offs = 0;
	int	was_avail = 0;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;

	for (i = 0; i < priv->boot_sector.root_dir_max_entries; i += FAT_SECTOR_SIZE / sizeof(struct fat_dir_entry))
	{
		disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);

		for (j = 0; j < FAT_SECTOR_SIZE; j += sizeof(struct fat_dir_entry))
		{
			d = (struct fat_dir_entry*)(buf + j);
			if (FAT_ERASED_DIR_ENTRY == d->name[0] || FAT_AVAIL_DIR_ENTRY == d->name[0])
			{
				if (FAT_AVAIL_DIR_ENTRY == d->name[0])
					was_avail = 1;
				memcpy(d, src, sizeof(struct fat_dir_entry));
				disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);

				// Update the next directory entry as available (if used available and not erased)
				if (was_avail)
				{
					if (j < FAT_SECTOR_SIZE - sizeof(struct fat_dir_entry))
					{
						d = (struct fat_dir_entry*)(buf + j + sizeof(struct fat_dir_entry));
						memset(d, 0, sizeof(struct fat_dir_entry));
						disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs, buf, FAT_SECTOR_SIZE);
					}
					else if (i < priv->boot_sector.root_dir_max_entries - FAT_SECTOR_SIZE / sizeof(struct fat_dir_entry))
					{
						disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs + FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE);
						memset(buf, 0, sizeof(struct fat_dir_entry));
						disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->root_dir_region + curr_sector_offs + FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE);
					}
				}
				return	0;
			}
		}
		curr_sector_offs += FAT_SECTOR_SIZE;
	}

	return	-1;
}

// Read directory entry that matches a given name
int	fat_dir_entry_read(struct fs *this, struct fat_dir_entry *parent_dir_entry, char *name, struct fat_dir_entry *dest)
{
	unsigned	offs;
	unsigned	parent_dir_size;
	struct	fat_dir_entry	temp_dir_entry;

	parent_dir_size = fat_get_file_size(this, parent_dir_entry);

	for (offs = 0; offs < parent_dir_size; offs += sizeof(struct fat_dir_entry))
	{
		fat_file_read(this, parent_dir_entry, (void*)&temp_dir_entry, offs, sizeof(struct fat_dir_entry));

		// First character in dir. entry name '\0' - means this is the last entry (and empty)
		if (temp_dir_entry.name[0] == '\0')
			return	-1;
		

//		if ((temp_dir_entry.attrib & FAT_FILE_ATTR_LFNENTRY) != FAT_FILE_ATTR_LFNENTRY)
		{
			if (fat_cmp_fnames(&temp_dir_entry, name) == 0)
			{
				memcpy(dest, &temp_dir_entry, sizeof(struct fat_dir_entry));
				return	0;
			}
		}
	}

	return	-1;
}

// Update existing directory entry that matches a given name
int	fat_dir_entry_update(struct fs *this, struct fat_dir_entry *parent_dir_entry, char *name, struct fat_dir_entry *src)
{
	unsigned	offs;
	unsigned	parent_dir_size;
	struct	fat_dir_entry	temp_dir_entry;

	parent_dir_size = fat_get_file_size(this, parent_dir_entry);

	for (offs = 0; offs < parent_dir_size; offs += sizeof(struct fat_dir_entry))
	{
		fat_file_read(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs, sizeof(struct fat_dir_entry));
		if (fat_cmp_fnames(&temp_dir_entry, name) == 0)
		{
			memcpy(&temp_dir_entry, src, sizeof(struct fat_dir_entry));
			fat_file_write(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs, sizeof(struct fat_dir_entry));
			return	0;
		}
	}
	return	-1;

}

// Writes a new directory entry (create)
int	fat_dir_entry_write(struct fs *this, struct fat_dir_entry *parent_dir_entry, struct fat_dir_entry *src)
{
	unsigned	offs;
	unsigned	parent_dir_size;
	struct	fat_dir_entry	temp_dir_entry;
	int	was_avail = 0;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;

	parent_dir_size = fat_get_file_size(this, parent_dir_entry);

	for (offs = 0; offs < parent_dir_size; offs += sizeof(struct fat_dir_entry))
	{
		fat_file_read(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs, sizeof(struct fat_dir_entry));
		if (FAT_ERASED_DIR_ENTRY == temp_dir_entry.name[0] || FAT_AVAIL_DIR_ENTRY == temp_dir_entry.name[0])
		{
			if (FAT_AVAIL_DIR_ENTRY == temp_dir_entry.name[0])
				was_avail = 1;
			memcpy(&temp_dir_entry, src, sizeof(struct fat_dir_entry));
			fat_file_write(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs, sizeof(struct  fat_dir_entry));
			// If it was available directory entry, mark the next as available
			if (was_avail && offs % priv->cluster_size < priv->cluster_size - sizeof(struct fat_dir_entry));
			{
				memset(&temp_dir_entry, 0, sizeof(struct fat_dir_entry));
				fat_file_write(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs + sizeof(struct fat_dir_entry), sizeof(struct fat_dir_entry));
			}

			return	0;
		}
	}
	// 'offs' offsets exactly to the end of file
	memcpy(&temp_dir_entry, src, sizeof(struct fat_dir_entry));
	if (fat_file_write(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs, sizeof(struct fat_dir_entry)) == (unsigned)-1)
		return	-1;

	// Mark the next as available
	memset(&temp_dir_entry, 0, sizeof(struct fat_dir_entry));
	fat_file_write(this, parent_dir_entry, (unsigned char*)&temp_dir_entry, offs + sizeof(struct fat_dir_entry), sizeof(struct fat_dir_entry));
	return	0;
}


// Finds a directory entry (file or another directory, which is stored in result parameter
// Returns success indicator (0 = success)
int	fat_find_dir_entry(struct fs *this, const char *path, struct fat_dir_entry *dir_entry)
{
	char	search_name[256];
	char	*p = NULL;
	int	i, j;
	struct fat_dir_entry	*d;
	unsigned	curr_cluster;
	unsigned char	buf[FAT_MAX_CLUSTER_SIZE];
	int	root_dir = 0;
	struct fat_dir_entry	parent_dir_entry;

	// Root directory
	if ('/' == path[0])
	{
		++path;
		root_dir = 1;
	}
	p = strchr(path, '/');
	if (NULL == p)
		strcpy(search_name, path);
	else
	{
		memcpy(search_name, path, p-path);
		search_name[p-path] = '\0';
	}
	if (root_dir)
	{
		// Read root directory entry
		if (fat_root_dir_entry_read(this, search_name, dir_entry) == -1)
			return	-1;
	}
	// Sub-sirectory
	else
	{
		memcpy(&parent_dir_entry, dir_entry, sizeof(struct fat_dir_entry));
		// If name is "", then we're interested in entry of sub-directory where we are
		if (search_name[0] != '\0')
			if (fat_dir_entry_read(this, &parent_dir_entry, search_name, dir_entry) == -1)
				return	-1;
	}
	if (NULL == p)
	{
		return	0;
	}
	return fat_find_dir_entry(this, ++p, dir_entry);	
}


// Generic FS interface implementation
int	fat_unmount(struct fs *this)
{
	free(this->fs_priv);
	return	0;
}

int	fat_file_open(struct fs *this, const char *pathname, int flags, void **fs_entry)
{
	char	*p;
	int	rv;
	struct fat_dir_entry dir_entry, *new_dir_entry;

	rv = fat_find_dir_entry(this, pathname, &dir_entry);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	new_dir_entry = calloc(1, sizeof(struct fat_dir_entry));
	if (NULL == new_dir_entry)
	{
		errno = ENOMEM;
		return	-1;
	}
	memcpy(new_dir_entry, &dir_entry, sizeof(struct fat_dir_entry));
	*fs_entry = (void*)new_dir_entry;
	return	0;
}

int	fat_file_creat(struct fs *this, const char *pathname, mode_t mode, void **fs_entry)
{
	char	*p;
	char	path[256];
	char	file[256];
	int	rv;
	struct fat_dir_entry dir_entry, *new_dir_entry;
	struct fat_date	fdate;
	struct fat_time	ftime;
	char	fname[9];
	char	fext[4];

	// Find parent directory
	p = strrchr(pathname, '/');
	if (p != NULL)
	{
		memcpy(path, pathname, p-pathname);
		path[p-pathname] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, pathname);
	}

	rv = fat_find_dir_entry(this, path, &dir_entry);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	new_dir_entry = calloc(1, sizeof(struct fat_dir_entry));
	if (NULL == new_dir_entry)
	{
		errno = ENOMEM;
		return	-1;
	}

	new_dir_entry->file_size = 0;
	new_dir_entry->attrib = FAT_FILE_ATTR_ARCHIVE;

	fdate.year = 2010;
	fdate.month = 2;
	fdate.day = 27;
	fat_date_to_cdate(&fdate, &new_dir_entry->cdate);
	fat_date_to_cdate(&fdate, &new_dir_entry->adate);
	fat_date_to_cdate(&fdate, &new_dir_entry->mdate);

	ftime.hour = 14;
	ftime.minute = 49;
	ftime.second = 0;
	ftime.ms = 0;
	fat_time_to_ctime(&ftime, &new_dir_entry->ctime, &new_dir_entry->ctime_10ms);
	fat_time_to_ctime(&ftime, &new_dir_entry->mtime, NULL);

	fat_name_to_fname83(file, fname, fext);
	memcpy(new_dir_entry->name, fname, 8);
	memcpy(new_dir_entry->ext, fext, 3);

	if (0 == strcmp(path, "/"))
		fat_root_dir_entry_write(this, new_dir_entry);
	else
		fat_dir_entry_write(this, &dir_entry, new_dir_entry);
	*fs_entry = (void*)new_dir_entry;
	return	0;
}

//
// Read `size' bytes from `dir_entry' starting at data offset `offs'
// Returns number of bytes read (may be less than `size')
//
ssize_t	fat_file_read(struct fs *this, void *fs_entry, void *buffer, off_t offs, size_t size)
{
	unsigned	curr_cluster;
	off_t	curr_offs = 0, prev_offs = 0;
	unsigned char	buf[FAT_MAX_CLUSTER_SIZE];
	unsigned	sz;
	unsigned	count = 0;
	unsigned	file_size;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	unsigned char	*dest = buffer;
	struct fat_dir_entry *dir_entry = fs_entry;

	file_size = fat_get_file_size(this, dir_entry);
	if (offs >= file_size)
		return	0;
		
	for (curr_cluster = dir_entry->first_cluster - 2; curr_cluster <= priv->last_used_cluster_val - 2; curr_cluster = fat_next_cluster(this, curr_cluster))
	{
		curr_offs += priv->cluster_size;
		if (prev_offs <= offs && curr_offs > offs)
		{
			disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->data_start + (off_t)priv->cluster_size * curr_cluster, buf, priv->cluster_size);
			sz = curr_offs - offs;
			if (sz > size - count)
				sz = size - count;
			if (sz > file_size - offs)
				sz = file_size - offs;
			memcpy(dest, buf + offs - prev_offs, sz);
			count += sz;
			dest += sz;
			if (count == size)
				return	count;
			offs = curr_offs;
		}
		prev_offs = curr_offs;
	}
	
	return	count;
}

//
// Write `size' bytes from `dir_entry' starting at data offset `offs'
// Returns number of bytes written (normally equal to `size').
// Allocates data clusters if necessary
// (!) Returns an error if not enough free space on disk to write ALL data
//
ssize_t	fat_file_write(struct fs *this, void *fs_entry, const void *buffer, off_t offs, size_t size)
{
	unsigned	first_new_cluster = (unsigned)-1;
	unsigned	nclusters;
	unsigned	curr_cluster, last_cluster;
	unsigned char	buf[FAT_MAX_CLUSTER_SIZE];
	off_t	curr_offs = 0, prev_offs = 0;
	unsigned	sz;
	unsigned	count = 0;
	unsigned	file_size;
	struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;
	const unsigned char *src = buffer;
	struct fat_dir_entry *dir_entry = fs_entry;

	file_size = fat_get_file_size(this, dir_entry);

	// If necessary to allocate additional clusters, do it now
	if (offs + size > file_size)
	{
		nclusters = (offs + size - file_size) / priv->cluster_size;
		if ((offs + size - file_size) % priv->cluster_size != 0)
			++nclusters;
		first_new_cluster = fat_alloc_clusters_count(this, nclusters);
		// Return (unsigned)-1 if not enough free space
		if ((unsigned)-1 == first_new_cluster)
			return	(unsigned)-1;

		if (0 == dir_entry->first_cluster)
			dir_entry->first_cluster = first_new_cluster + 2;

		// first_new_cluster will be chained to the last cluster below
		// File size is not updated in case that sub-directory or volume label are written, for which size in directory entry is always 0
	}
	
	// First write up to original file size
	for (curr_cluster = dir_entry->first_cluster - 2; curr_cluster <= priv->last_used_cluster_val - 2; curr_cluster = fat_next_cluster(this, curr_cluster))
	{
next_loop:
		curr_offs += priv->cluster_size;
		if (prev_offs <= offs && curr_offs > offs)
		{
			sz = curr_offs - offs;
			if (sz > size - count)
				sz = size - count;
			memcpy(buf + offs - prev_offs, src, sz);
			disks[this->disk_num].write(&disks[this->disk_num], this->start_offs + priv->data_start + (off_t)priv->cluster_size * curr_cluster, buf, priv->cluster_size);
			count += sz;
			src += sz;
			
			// This is main exit from the function
			if (count == size)
				return	count;
			offs = curr_offs;
		}
		prev_offs = curr_offs;
		last_cluster = curr_cluster;
	}
	
	// Now last_cluster holds the last cluster index. Update it
	fat_commit_entry(this, last_cluster + 2, first_new_cluster + 2);
	
	// Now a little trick: set up a different start value and goto in the previous loop.
	// This will work because the allocated chain already contains last cluster marker, so loop condition is correct
	curr_cluster = first_new_cluster;
	goto	next_loop;
}

int	fat_file_close(struct fs *this, void *fs_entry)
{
	// If we were buffering I/O, need to commit file here. But we don't buffer, so just free private inode structure
	free(fs_entry);
	return	0;
}

int	fat_file_unlink(struct fs *this, char *dest)
{
	char	path[256];
	char	file[256];
	struct fat_dir_entry	parent_dir_entry, dir_entry;
	char	*p;
	int	rv;

	// Find parent directory
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

	rv = fat_find_dir_entry(this, path, &parent_dir_entry);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	rv = fat_dir_entry_read(this, &parent_dir_entry, file, &dir_entry);
	if (rv < 0)
	{
		errno = ENOENT;
		return	-1;
	}
	dir_entry.name[0] = FAT_ERASED_DIR_ENTRY;
	rv = fat_dir_entry_update(this, &parent_dir_entry, file, &dir_entry);
	return	0;
}

struct dirent	*fat_read_dir(struct fs *this, void *fs_entry, off_t pos)
{
	static struct dirent	result; 
	int	rv;
	struct fat_dir_entry	*d = fs_entry;
	unsigned char	buf[FAT_SECTOR_SIZE];
	char	*p, *q;

	memset(&result, 0, sizeof(result));
	pos -= pos % sizeof(struct  fat_dir_entry);
	// If reading root directory
	if (d->attrib == ROOT_DIR_ATTRIB && d->first_cluster == ROOT_DIR_FIRST_CLUSTER)
	{
		struct fat_fs_priv	*priv = (struct fat_fs_priv*)this->fs_priv;

		if (pos >= priv->boot_sector.root_dir_max_entries * sizeof(struct fat_dir_entry))
		{
			errno = EINVAL;
			return	NULL;
		}

		rv = disks[this->disk_num].read(&disks[this->disk_num], this->start_offs + priv->root_dir_region + pos - pos % FAT_SECTOR_SIZE, buf, FAT_SECTOR_SIZE);
		if (rv)
			return	NULL;
		d = (struct fat_dir_entry*)(buf + pos % FAT_SECTOR_SIZE);
	}

	// Reading non-root directory
	else
	{
		rv = fat_file_read(this, fs_entry, buf, pos, sizeof(struct dirent));
		if (!rv)
		{
			errno = ENOENT;
			return	NULL;
		}
		d = (struct fat_dir_entry*)buf;
	}

	// Instead of LFN entries, return the next record (their standard name counterpart)
	if ((d->attrib & FAT_FILE_ATTR_LFNENTRY) == FAT_FILE_ATTR_LFNENTRY)
		return fat_read_dir(this, fs_entry, pos + sizeof(struct fat_dir_entry));

	// Fill result
	result.d_ino = d->first_cluster;
	for (p = result.d_name, q = d->name; q < d->name + 8 && *q != ' '; ++p, ++q)
		*p = *q;
	if (d->ext[0] != ' ')
	{
		*p++ = '.';
		for (q = d->ext; q < d->ext + 3 && *q != ' '; ++p, ++q)
			*p = *q;
	}
	*p = '\0';
	
	this->dir_pos = pos + sizeof(struct fat_dir_entry);
	// If past last entry, return NULL
	if (!result.d_name[0])
		return	NULL;
	return	&result;
}

size_t	fat_seek_dir(struct fs *this, void *fs_entry, off_t pos)
{
	ssize_t	dir_size;
	struct fat_dir_entry    *d = fs_entry;
	struct fat_fs_priv      *priv = (struct fat_fs_priv*)this->fs_priv;

	// If root directory
	if (d->attrib == ROOT_DIR_ATTRIB && d->first_cluster == ROOT_DIR_FIRST_CLUSTER)
		dir_size = (off_t)priv->boot_sector.root_dir_max_entries * sizeof(struct fat_dir_entry);
	else
		dir_size = fat_get_file_size(this, fs_entry);
	if (pos >= dir_size)
	{
		errno = ENOENT;
		return	-1;
	}
	pos %= sizeof(struct  fat_dir_entry);
	this->dir_pos = pos;
	return	pos;
}

int     fat_fstat(struct fs *this, void *fs_entry, struct stat *buf)
{
	struct fat_dir_entry    *dir_entry = fs_entry;

	memset(buf, 0, sizeof(buf));
	buf->st_ino = dir_entry->first_cluster;
	buf->st_mode = fat_get_file_attrib(this, fs_entry);
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = fat_get_file_size(this, fs_entry);
	buf->st_atime = (uint32_t)dir_entry->adate << 16;
	buf->st_mtime = dir_entry->mtime | (uint32_t)dir_entry->mdate << 16;
	buf->st_ctime = dir_entry->ctime | (uint32_t)dir_entry->cdate << 16;
	return	0;
}

int     fat_stat(struct fs *this, const char *path, struct stat *buf)
{
	int	rv;
	struct fat_dir_entry    dir_entry;

	rv = fat_find_dir_entry(this, path, &dir_entry);
	if (rv != 0)
	{
		errno = ENOENT;
		return	-1;
	}
	return	fat_fstat(this, &dir_entry, buf);
}

void	fat_sync(struct fs *this)
{
	return;
}

int	fat_file_rename(struct fs *this, char *src, char *dest)
{
	char	path[256];
	char	file[256];
	char	dest_path[256];
	char	dest_file[256];
	struct fat_dir_entry	src_parent_dir_entry, src_dir_entry, dest_parent_dir_entry, dest_dir_entry;
	char	*p;
	int	rv;
	char	dest_fname[9], dest_fext[4];

	// Find parent directory
	p = strrchr(src, '/');
	if (p != NULL)
	{
		memcpy(path, src, p-dest);
		path[p-src] = '\0';
		strcpy(file, p+1);
	}
	else
	{
		strcpy(path, "/");
		strcpy(file, dest);
	}
	rv = fat_find_dir_entry(this, path, &src_parent_dir_entry);
	if (rv != 0)
	{
		errno = ENOENT;
		return	rv;
	}

	// Find dest parent directory
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
	rv = fat_find_dir_entry(this, dest_path, &dest_parent_dir_entry);
	if (rv != 0)
	{
		errno = ENOENT;
		return	rv;
	}

	rv = fat_dir_entry_read(this, &src_parent_dir_entry, file, &src_dir_entry);
	memcpy(&dest_dir_entry, &src_dir_entry, sizeof(struct fat_dir_entry));
	memset(&src_dir_entry, 0, sizeof(struct fat_dir_entry));
	rv = fat_dir_entry_update(this, &src_parent_dir_entry, file, &src_dir_entry);
	if (rv != 0)
	{
		// TODO: set error (may be it will be set by fat_dir_entry_update()?
		return	rv;
	}

	fat_name_to_fname83(dest_file, dest_fname, dest_fext);
	memcpy(dest_dir_entry.name, dest_fname, 8);
	memcpy(dest_dir_entry.ext, dest_fext, 3);

	// Create a new directory entry at dest
	rv = fat_dir_entry_write(this, &dest_parent_dir_entry, &dest_dir_entry);
	return	0;
}

int	fat_mount(struct fs *this, const char *mount_point, int disk_num, unsigned start_sect)
{
	struct fat_fs_priv	*priv;
	int	rv;
	off_t	start_offs;
	unsigned char	buf[FAT_SECTOR_SIZE];
	int	i;

	start_offs = (off_t)start_sect * DISK_SECTOR_SIZE;

	// Read boot sector
	rv = disks[disk_num].read(&disks[disk_num], start_offs, buf, FAT_SECTOR_SIZE);
	if (rv != 0)
		return	rv;
	
	priv = calloc(1, sizeof(struct fat_fs_priv));
	if (NULL == priv)
	{
		errno = ENOMEM;
		return	-1;
	}

	// Const values
	priv->free_cluster_val = 0;
	priv->first_used_cluster_val = 2;
	priv->fat_type = FAT_TYPE_FAT16;

	memcpy(&priv->boot_sector, buf, sizeof(struct fat_boot_sect));

	priv->cluster_size = priv->boot_sector.sectors_per_cluster * FAT_SECTOR_SIZE;
	priv->root_dir_region = ((off_t)priv->boot_sector.num_reserved_sectors + (off_t)priv->boot_sector.num_fats * priv->boot_sector.sectors_per_fat) * FAT_SECTOR_SIZE;
	priv->data_start = priv->root_dir_region + (off_t)priv->boot_sector.root_dir_max_entries * sizeof(struct fat_dir_entry) /* 32 */;

	if (FAT_EXT_BOOT_SIG == priv->boot_sector.ext_boot_sig)
	{
		if (memcmp(priv->boot_sector.fat_type, "FAT16   ", 8) == 0)
		{
			priv->fat_type = FAT_TYPE_FAT16;
			priv->last_cluster_min_val = FAT16_LAST_CLUSTER_MIN;
			priv->bad_cluster_val = FAT16_BAD_CLUSTER;
			priv->last_used_cluster_val = FAT16_LAST_USED_CLUSTER;
		}
		else if (memcmp(priv->boot_sector.fat_type, "FAT12   ", 8) == 0)
		{
			priv->fat_type = FAT_TYPE_FAT12;
			priv->last_cluster_min_val = FAT12_LAST_CLUSTER_MIN;
			priv->bad_cluster_val = FAT12_BAD_CLUSTER;
			priv->last_used_cluster_val = FAT12_LAST_USED_CLUSTER;
		}
		else if (memcmp(priv->boot_sector.fat_type, "FAT32   ", 8) == 0)
			priv->fat_type = FAT_TYPE_FAT32;
		else
		{
			// It's not a FAT partition
			free(priv);
			errno = ENODEV;
			return	-1;
		}
	}
	else
	{
		// It's not a FAT partition
		free(priv);
		errno = ENODEV;
		return	-1;
	}

	priv->total_sectors = priv->boot_sector.sectors_total;
	if (0 == priv->total_sectors)
		priv->total_sectors = priv->boot_sector.sectors_total_long;

	// Fill FS'es structure
	this->fs_priv = priv;
	this->disk_num = disk_num;
	this->start_offs = (off_t)start_offs; 
	this->mount_point = calloc(1, strlen(mount_point) + 1);
	strcpy(this->mount_point, mount_point);
	
	this->file_open = fat_file_open;
	this->file_creat = fat_file_creat;
	this->file_read = fat_file_read;
	this->file_write = fat_file_write;
	this->file_close = fat_file_close;
	this->file_unlink = fat_file_unlink;
	this->file_rename = fat_file_rename;
	this->unmount = fat_unmount;
	this->get_file_size = fat_get_file_size;
	this->get_file_attrib = fat_get_file_attrib;
	this->read_dir = fat_read_dir;
	this->seek_dir = fat_seek_dir;
	this->stat = fat_stat;
	this->fstat = fat_fstat;
	this->sync = fat_sync;
	
	this->fs_type = FS_FAT;

dump_fat_info(this, &priv->boot_sector);
	return	0;
}


