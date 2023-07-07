/*************************************************
*
*	ide.h
*
*	Definitions for IDE disks driver
*
*************************************************/

#include "io.h"

#define	MAX_IDE_DISKS	4	

#define	IDE_AUTOSCAN	1

#define	IDE_DRIVES_PER_BUS	2

// Relative-big number of retries before giving up
#define	IDE_MAX_RETRIES		100 /*65536*/

// IDE command registers
#define	IDE_CMD_REG_DATA	0
#define	IDE_CMD_REG_ERROR	1
#define	IDE_CMD_REG_FEATURE	1
#define	IDE_CMD_REG_SECTOR_COUNT	2
#define IDE_CMD_REG_SECTOR_NUMBER	3
#define IDE_CMD_REG_CYLINDER_LOW	4
#define IDE_CMD_REG_CYLINDER_HIGH	5
#define IDE_CMD_REG_DRIVE_HEAD	6
#define IDE_CMD_REG_DRIVE_STATUS	7
#define IDE_CMD_REG_DRIVE_COMMAND	7

#if defined(evmdm6467)
#define	IDE_CONTROL_REG	0x206
#define	IDE_ALTSTATUS_REG	0x206
#endif

// IDE status codes
#define	IDE_STATUS_BUSY	0x80
#define	IDE_STATUS_DRDY	0x40
#define	IDE_STATUS_DWF	0x20
#define	IDE_STATUS_DSC	0x10
#define	IDE_STATUS_DRQ	0x8
#define	IDE_STATUS_CORR	0x4
#define	IDE_STATUS_IDX	0x2
#define	IDE_STATUS_ERR	0x1

#if defined (pc)
// Primary IDE base port
#define	IDE0_BASE_PORT	0x1F0
// Secondary IDE base port
#define	IDE1_BASE_PORT	0x170
#elif defined (evmdm6467)
// Primary IDE base port
#define	IDE0_BASE_PORT	(IDE_MODULE_BASE+0x1F0)
// DM646x doesn't have a secondary IDE controller
#endif

// ATAPI General Configuration Word
#define	ATAPI_PROTOCOL_TYPE_MASK	0xC000	// Bits 14-15
#define	ATAPI_PROTOCOL_IS_ATAPI		0x8000	// & ATAPI_PROTOCOL_TYPE_MASK

#define	ATAPI_DEVICE_TYPE_MASK		0x1F00	// Bits 8-12
#define	ATAPI_DEVICE_IS_CDROM		0x500	// & ATAPI_DEVICE_TYPE_MASK

#define	ATAPI_REMOVABLE_MASK		0x80	// Bit 7
#define	ATAPI_CMD_DRQ_TYPE_MASK		0x60	// Bits 5-6

#define	ATAPI_CMD_PKT_SIZE_MASK		0x3	// Bits 0-1
#define	ATAPI_CMD_PKT_SIZE_12		0	// & ATAPI_CMD_PKT_SIZE_MASK
#define	ATAPI_CMD_PKT_SIZE_16		1	// & ATAPI_CMD_PKT_SIZE_MASK

// ATA/IDE/ATAPI Commands
#define	IDE_CMD_IDENTIFY_DRIVE		0xEC
#define	IDE_CMD_READ_SECTORS		0x20	// Use read and write commands with vendor-specific retry count
#define	IDE_CMD_WRITE_SECTORS		0x30
#define	IDE_CMD_FORMAT_TRACK		0x50
#define	IDE_CMD_SEEK			0x70
#define	IDE_CMD_RECALIBRATE		0x10

#define	ATAPI_CMD_IDENTIFY_DRIVE	0xA1

struct ide_priv
{
	int	bus;
	int	drive;
	word	num_cyls;
	word    num_heads;
	word    sect_per_track;
	word    byte_per_sec_u;
	word    byte_per_tr_u;
};

struct ide_ident_params
{
	word	config;
	word	num_cyls;
	word	reserved1;
	word	num_heads;
	word	byte_per_tr_u;
	word	byte_per_sec_u;
	word	sect_per_track;
	unsigned char	vendor_spec1[6];
	char	serial_no[20];			// ASCII
	word	buf_type;
	word	buf_size;
	word	num_of_ecc_byte;
	char	fw_rev[8];			// ASCII
	char	model_name[40];			// ASCII
	word	mul_sect_per_int;
	word	dwio;
	word	lbadma;
	word	reserved2;
	word	pio_ti_mode;
	word	dma_ti_mode;
	word	reserved3;
	word	ap_num_cyls;
	word	ap_num_heads;
	word	ap_sect_per_track;
	dword	capacity;
	word	num_sec_per_int;
	dword	lba_sectors;
	word	sin_dma_modes;
	word	mul_dma_modes;
	unsigned char	reserved4[128];
	unsigned char	vendor_spec2[64];
	word	reserved5;			// word?
} __attribute__ ((packed));

int	ide_identify(int bus, int drive, unsigned char *buf);
int	ide_read_sectors(int bus, int drive, int cyl, int head, int sector, int count, unsigned char *buf);
int	ide_write_sectors(int bus, int drive, int cyl, int head, int sector, int count, const unsigned char *buf);
int	ide_format_track(int bus, int drive, int cylinder, int head);

// Generic disk interface functions
int	ide_disk_read(struct disk *this, off_t offs, void *buf, unsigned length);
int	ide_disk_write(struct disk *this, off_t offs, void *buf, unsigned length);

