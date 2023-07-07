/*
 * 	ide.c
 *
 *	IDE bus driver for September OS
 *
 *	NOTES
 *	1) Currently only PIO mode is implemented
 *	2) Currently only hard drives are interfaced
 *	3) Currently interrupts are not used
 *
 *	TODOs (+wishes)
 *	1) We already read multiple sectors (up to complete track) at once. However, most/all modern disks actually fake geometry, so we're
 *	stuffing unnecessary calculations, which disk-side controller will recompute again. May be we will have easier / more perspective life with LBA?
 */

#include "sosdef.h"
#include "ide.h"
#include "io.h"
#include "drvint.h"
#include "config.h"

#define	SECTOR_SIZE	DISK_SECTOR_SIZE

//#define	DEBUG_IDE_IO	1

int	num_ide_buses = IDE_NUM_BUSES;
#if defined (pc)
unsigned short	ide_base_ports[IDE_NUM_BUSES] = {IDE0_BASE_PORT, IDE1_BASE_PORT};
#elif defined (evmdm6467)
unsigned short	ide_base_ports[IDE_NUM_BUSES] = {IDE0_BASE_PORT};
#endif

// Configuration structure, filled from IDE "identify" results
static struct ide_priv	ide_disks_geometries[MAX_IDE_DISKS];

extern	int	exist_disk_num;
extern struct disk	disks[MAX_DISKS];

// Implementation of generic disk functions
int	ide_disk_read(struct disk *this, off_t offs, void *buf, unsigned length)
{
	off_t	/*temp_offs, */temp_offs2;
	int	rv;
	unsigned char	temp_buf[SECTOR_SIZE];
	int	bus, drive;
	int	cyl, head, sector;
	struct ide_priv	*priv = (struct ide_priv*)this->disk_priv;
	unsigned	sz;
	int	num_sectors;

// Debugging -- "forget" to check params
	// Check that parameters are OK
	if (offs + length >= priv->sect_per_track * priv->num_heads * priv->num_cyls * DISK_SECTOR_SIZE)
	{
		errno = EINVAL;
		return	-1;
	}

//serial_printf("%s(): this = %08X priv = %08X\r\n", __func__, this, priv);
//serial_printf("%s(): [1] offs = %u length = %u\r\n", __func__, (unsigned)offs, length);
	bus = priv->bus;
	drive = priv->drive;

#if DEBUG_IDE_IO
	serial_printf("%s(): called with offs=%lu length=%u\n", __func__, offs, length);
#endif
	// Read first sector's fragment (if `offs' is not sector-aligned)
	if (offs % SECTOR_SIZE != 0)
	{
		//temp_offs = offs - offs % SECTOR_SIZE;

		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;

#if DEBUG_IDE_IO
	serial_printf("%s() -- check #1: calling ide_read_sectors(%d, %d, %d, %d, %d, 1, temp_buf)\n", __func__, bus, drive, cyl, head, sector);
#endif
		rv = ide_read_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
		if (rv != 0)
		{
			errno = EIO;
			return	-1;
		}
		
		sz = SECTOR_SIZE - offs % SECTOR_SIZE;
		if (sz > length)
			sz = length;
		
		memcpy(buf, temp_buf + offs % SECTOR_SIZE, sz);
		offs += SECTOR_SIZE - offs % SECTOR_SIZE;
		length -= sz;
		buf = (char*)buf + SECTOR_SIZE - offs % SECTOR_SIZE;
	}
//serial_printf("%s(): [2] offs = %u length = %u\r\n", __func__, (unsigned)offs, length);
	while (length > SECTOR_SIZE)
	{
		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;
		

		// read until end of current track (cylinder on current head)
		// num_sectors = 1;
		num_sectors = length / SECTOR_SIZE - 1;
		if (priv->sect_per_track - sector + 1 < num_sectors)
			num_sectors = priv->sect_per_track - sector + 1;

#if DEBUG_IDE_IO
	serial_printf("%s() -- check #2: calling ide_read_sectors(%d, %d, %d, %d, %d, 1, temp_buf)\n", __func__, bus, drive, cyl, head, sector);
#endif
		rv = ide_read_sectors(bus, drive, cyl, head, sector, num_sectors, buf);
		if (rv != 0)
		{
			errno = EIO;
			return	-1;
		}
		offs += num_sectors * SECTOR_SIZE;
		length -= num_sectors * SECTOR_SIZE;
		buf = (char*)buf + num_sectors * SECTOR_SIZE;
//serial_printf("%s(): [3] offs = %u length = %u\r\n", __func__, (unsigned)offs, length);		
	}

	// Read last sector's fragment
	if (length > 0)
	{
		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;
		
#if DEBUG_IDE_IO
	serial_printf("%s() -- check #3: calling ide_read_sectors(%d, %d, %d, %d, %d, 1, temp_buf)\n", __func__, bus, drive, cyl, head, sector);
#endif
		rv = ide_read_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
		if (rv != 0)
		{
			errno = EIO;
			return	-1;
		}
		memcpy(buf, temp_buf, length);
//serial_printf("%s(): [4] offs = %u length = %u\r\n", __func__, (unsigned)offs, length);		
	}

	return	0;
}

int	ide_disk_write(struct disk *this, off_t offs, void *buf, unsigned length)
{
	off_t	/*temp_offs, */temp_offs2;
	int	rv;
	unsigned char	temp_buf[SECTOR_SIZE];
	int	bus, drive;
	int	cyl, head, sector;
	struct ide_priv	*priv = (struct ide_priv*)this->disk_priv;
	unsigned	sz;
	int	num_sectors;

//serial_printf("%s() entered\n", __func__);
#if 0
serial_printf("%s()--------------------------------\n", __func__);
serial_printf("%s(): offs=%u lenght=%u data:\n", __func__, offs, length);
{
	int	i;

	for (i = 0; i < length; ++i)
		serial_printf("%c ", ((char*)buf)[i]);
}
serial_printf("\n"
		"%s()--------------------------------\n", __func__);
#endif

#if DEBUG_IDE_IO
	serial_printf("%s(): called with offs=%lu length=%u\n", __func__, offs, length);
#endif
	// Check that parameters are OK
	if (offs + length >= priv->sect_per_track * priv->num_heads * priv->num_cyls * DISK_SECTOR_SIZE)
	{
		errno = EINVAL;
		return	-1;
	}

	bus = priv->bus;
	drive = priv->drive;

	// Read first sector's fragment (if `offs' is not sector-aligned)
	if (offs % SECTOR_SIZE != 0)
	{
		//temp_offs = offs - offs % SECTOR_SIZE;

		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;
		
#if DEBUG_IDE_IO
	serial_printf("%s() -- check #4: calling ide_read_sectors(%d, %d, %d, %d, %d, 1, temp_buf)\n", __func__, bus, drive, cyl, head, sector);
#endif
		rv = ide_read_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
		if (rv != 0)
		{
#if DEBUG_IDE_IO
	serial_printf("%s(): EIO -- check #1\n", __func__);
#endif
			errno = EIO;
			return	-1;
		}
		sz = SECTOR_SIZE - offs % SECTOR_SIZE;
		if (sz > length)
			sz = length;
		
		memcpy(temp_buf + offs % SECTOR_SIZE, buf, sz);
		rv = ide_write_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
		if (rv != 0)
		{
#if DEBUG_IDE_IO
	serial_printf("%s(): EIO -- check #2\n", __func__);
#endif
			errno = EIO;
			return	-1;
		}
		offs += SECTOR_SIZE - offs % SECTOR_SIZE;
		length -= sz;
		buf = (char*)buf + SECTOR_SIZE - offs % SECTOR_SIZE;
	}

	while (length > SECTOR_SIZE)
	{
		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;
		

		// write until end of current track (cylinder on current head)
		// num_sectors = 1;
		num_sectors = length / SECTOR_SIZE - 1;
		if (priv->sect_per_track - sector + 1 < num_sectors)
			num_sectors = priv->sect_per_track - sector + 1;

		rv = ide_write_sectors(bus, drive, cyl, head, sector, num_sectors, buf);
		if (rv != 0)
		{
#if DEBUG_IDE_IO
	serial_printf("%s(): EIO -- check #3\n", __func__);
#endif
			errno = EIO;
			return	-1;
		}
		offs += num_sectors * SECTOR_SIZE;
		length -= num_sectors * SECTOR_SIZE;
		buf = (char*)buf + num_sectors * SECTOR_SIZE;
	}

	// Read last sector's fragment
	if (length > 0)
	{
		// Translate temp_offs to cyl, head, sector, subtract everything from `temp_offs'
		temp_offs2 = offs;
		cyl = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		temp_offs2 -= cyl * (DISK_SECTOR_SIZE * priv->sect_per_track * priv->num_heads);
		head = temp_offs2 / (DISK_SECTOR_SIZE * priv->sect_per_track);
		temp_offs2 -= head * (DISK_SECTOR_SIZE * priv->sect_per_track);
		sector = temp_offs2 / DISK_SECTOR_SIZE + 1;
		
#if DEBUG_IDE_IO
	serial_printf("%s() -- check #5: calling ide_read_sectors(%d, %d, %d, %d, %d, 1, temp_buf)\n", __func__, bus, drive, cyl, head, sector);
#endif
		rv = ide_read_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
		if (rv != 0)
		{
#if DEBUG_IDE_IO
	serial_printf("%s(): EIO -- check #4\n", __func__);
#endif
			errno = EIO;
			return	-1;
		}
		memcpy(temp_buf, buf, length);
		ide_write_sectors(bus, drive, cyl, head, sector, 1, temp_buf);
	}

	return	0;
}

// Sends IDE "identify" command to bus (0 or 1) driver (0 or 1)
// Returns 0 on success, -1 on failure (return more elaborate error?)
// TODO: what to do with output structure?
// (!) Reads 256 words, as full 512-byte sector
int	ide_identify(int bus, int drive, unsigned char *buf)
{
	int	max_retries;
	int	status;
	int	i;
	word	*wbuf = (word*)buf;

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

#if 0
	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5000);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-1;
#endif

#if 0
	// try to reset first
#ifdef IDE_CONTROL_REG
	outb(ide_base_ports[bus] + IDE_CONTROL_REG, 4);
	udelay(3000000);
	outb(ide_base_ports[bus] + IDE_CONTROL_REG, 0);
#endif
#endif

	// Code updated to "take a deep breath"s from Linix driver [drivers/ide/ide-probe.c, do_probe(), try_to_identify(), actually_tr_to_identify()]

	// Select drive
	udelay(50000);
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_HEAD, (drive & 1) << 4 | 0xEF);		// Is this | 0xEF needed?
	udelay(50000);

	// Wait for BUSY to become 0 and DRDY to become 1
	for (max_retries = 2; max_retries > 0; --max_retries)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRDY) != 0)
			break;
		udelay(50000);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
	{
		serial_printf("%s(): max_retries exhausted, status=%08X\r\n", __func__, status);
		return	-2;
	}

#ifdef IDE_CONTROL_REG
	outb(ide_base_ports[bus] + IDE_CONTROL_REG, 0x0A);
#endif
	udelay(50000);

	// Write "indentify drive" command
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_COMMAND, IDE_CMD_IDENTIFY_DRIVE);

	udelay(50000);

	// Wait for completion
	for (max_retries = IDE_MAX_RETRIES;;)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);

		// I/O error
		if (status & IDE_STATUS_ERR)
			return	-3;

		// Success
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRQ) != 0)
			break;

		if (!--max_retries)
			return	-1;

		// if (timeout)
		//	return -1;
	}

	udelay(50000);

	// Read sector buffer
	for (i = 0; i < 256; ++i)
		wbuf[i] = htons(inw(ide_base_ports[bus] + IDE_CMD_REG_DATA));

	return	0;
}


// Seek to a specified cylinder
int	ide_seek(int bus, int drive, int cyl, int head)
{
	int	max_retries;
	int	status;
	int	datum;

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-3;

	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_LOW, cyl & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_HIGH, cyl >> 8 & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_HEAD, head & 0xF | (drive & 0x1) << 4 | 0xA0);

	// Wait for BUSY to become 0 and DRDY to become 1
	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRDY) != 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-4;

	// Write "seek" command
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_COMMAND, IDE_CMD_SEEK);

	// Wait for completion
	for (;;)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);

		// I/O error
		if (status & IDE_STATUS_ERR)
		{
			unsigned	error;

			error = inb(IDE_CMD_REG_ERROR);
			serial_printf("%s() failed, status=%08X error=%08X\r\n", __func__, (unsigned)status, error);
			return	-5;
		}

		// Success
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DSC) != 0)
			break;

		// if (timeout)
		//	return -1;
	}

	return	0;
}


// Read IDE disk sectors
int	ide_read_sectors(int bus, int drive, int cyl, int head, int sector, int count, unsigned char *buf)
{
	int	max_retries;
	int	status;
	int	i, j;
	word	*wbuf = (word*)buf;
	int	rv;

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

	if (count < 0 || count > 256 || sector < 0 || sector > 256)
		return	-2;

#if DEBUG_IDE_IO
	serial_printf("%s(): bus=%d drive=%d cyl=%d head=%d sector=%d count=%d\r\n", __func__, bus, drive, cyl, head, sector, count);
#endif
	
	rv = ide_seek(bus, drive, cyl, head);
#if DEBUG_IDE_IO
	serial_printf("%s(): ide_seek() - %s\r\n", __func__, 0 == rv ? "succeeded" : "failed");
#endif
	if (rv != 0)
	{
serial_printf("%s(): ide_seek() error (%d)\n", __func__, rv);
		return	rv;
	}

	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): waiting for device ready - %s\n", __func__, max_retries ? "succeeded" : "failed");
#endif
	// Drive is busy error
	if (!max_retries)
	{
serial_printf("%s(): waiting for ready error\n", __func__);
		return	-3;
	}

	if (256 == count)
		count = 0;

	// Write parameter registers
	outb(ide_base_ports[bus] + IDE_CMD_REG_SECTOR_COUNT, count); 
	outb(ide_base_ports[bus] + IDE_CMD_REG_SECTOR_NUMBER, sector); 
	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_LOW, cyl & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_HIGH, cyl >> 8 & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_HEAD, head & 0xF | (drive & 0x1) << 4 | 0xA0);

	// Wait for BUSY to become 0 and DRDY to become 1
	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRDY) != 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): waiting for device ready AND DRDY - %s\n", __func__, max_retries ? "succeeded" : "failed");
#endif
	// Drive is busy error
	if (!max_retries)
	{
serial_printf("%s(): waiting for ready and DRDY error\n", __func__);
		return	-4;
	}

	// Write "read sectors" command
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_COMMAND, IDE_CMD_READ_SECTORS);

	// Wait for completion
	for (;;)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);

		// I/O error
		if (status & IDE_STATUS_ERR)
		{
			unsigned	error;

			error = inb(IDE_CMD_REG_ERROR);
			serial_printf("%s() failed, status=%08X error=%08X\r\n", __func__, (unsigned)status, error);
			return	-5;
		}

		// Success
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRQ) != 0)
			break;

		// if (timeout)
		//	return -1;
	}

	// Read sectors to buffer
	j = 0;
	while (count > 0)
	{
		for (i = 0; i < 256; ++i, ++j)
		{
			wbuf[j] = inw(ide_base_ports[bus] + IDE_CMD_REG_DATA);
		}
		--count;
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): total success\n", __func__);
#endif
	return	0;
}


// Read IDE disk sectors
int	ide_write_sectors(int bus, int drive, int cyl, int head, int sector, int count, const unsigned char *buf)
{
	int	max_retries;
	int	status;
	int	i, j;
	word	*wbuf = (word*)buf;
	int	rv;

#if DEBUG_IDE_IO
	serial_printf("%s(): bus=%d drive=%d cyl=%d head=%d sector=%d count=%d\n", __func__, bus, drive, cyl, head, sector, count);
#endif

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

	if (count < 0 || count > 256 || sector < 0 || sector > 256)
		return	-2;

	rv = ide_seek(bus, drive, cyl, head);
#if DEBUG_IDE_IO
	serial_printf("%s(): ide_seek() - %s\r\n", __func__, 0 == rv ? "succeeded" : "failed");
#endif
	if (rv != 0)
	{
serial_printf("%s(): ide_seek() error (%d)\n", __func__, rv);
		return	rv;
	}

	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): waiting for device ready - %s\n", __func__, max_retries ? "succeeded" : "failed");
#endif
	// Drive is busy error
	if (!max_retries)
	{
serial_printf("%s(): waiting for ready error\n", __func__);
		return	-1;
	}

	if (256 == count)
		count = 0;

	// Write parameter registers
	outb(ide_base_ports[bus] + IDE_CMD_REG_SECTOR_COUNT, count); 
	outb(ide_base_ports[bus] + IDE_CMD_REG_SECTOR_NUMBER, sector); 
	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_LOW, cyl & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_CYLINDER_HIGH, cyl >> 8 & 0xFF);
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_HEAD, head & 0xF | (drive & 0x1) << 4 | 0xA0);

	// Wait for BUSY to become 0 and DRDY to become 1
	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);
		if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRDY) != 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): waiting for device ready - %s\n", __func__, max_retries ? "succeeded" : "failed");
#endif
	// Drive is busy error
	if (!max_retries)
	{
serial_printf("%s(): waiting for ready-2 error\n", __func__);
		return	-1;
	}

	// Write "write sectors" command
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_COMMAND, IDE_CMD_WRITE_SECTORS);

	// Write sectors from buffer
	j = 0;
	while (count > 0)
	{
		for (i = 0; i < 256; ++i, ++j)
		{
			// Wait for drive to become ready to accept more data in buffer.
			// Drive may become busy at any point
			for (;;)
			{
				status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);

				// I/O error
				if (status & IDE_STATUS_ERR)
				{
					unsigned	error;

					error = inb(IDE_CMD_REG_ERROR);
					serial_printf("%s() failed, status=%08X error=%08X\r\n", __func__, (unsigned)status, error);
					return	-1;
				}

				// Success
				if ((status & IDE_STATUS_BUSY) == 0 && (status & IDE_STATUS_DRQ) != 0)
					break;

				// if (timeout)
				//	return -1;
			}

			outw(ide_base_ports[bus] + IDE_CMD_REG_DATA, wbuf[j]);
		}
		--count;
	}

#if DEBUG_IDE_IO
	serial_printf("%s(): total success\n", __func__);
#endif
	return	0;
}

// Format the specified track - TODO
int	ide_format_track(int bus, int drive, int cylinder, int head)
{
	int	max_retries;
	int	status;
	int	i;

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-1;

}

// Recalibrate the specified drive (move it's heads to track 0
int	ide_recalibrate(int bus, int drive)
{
	int	max_retries;
	int	status;

	if (bus >= num_ide_buses || drive >= IDE_DRIVES_PER_BUS)
		return	-1;

	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		if ((inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS) & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-3;

	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_HEAD, (drive & 0x1) << 4 | 0xA0);

	// Wait for BUSY to become 0 and DRDY to become 1
	for (max_retries = IDE_MAX_RETRIES; max_retries > 0; --max_retries)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);
		if ((status & IDE_STATUS_BUSY) == 0)
			break;
		udelay(5);		// 5 us - the worst case of BUSY bit set
	}

	// Drive is busy error
	if (!max_retries)
		return	-4;

	// Write "seek" command
	outb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_COMMAND, IDE_CMD_RECALIBRATE);

	// Wait for completion
	for (;;)
	{
		status = inb(ide_base_ports[bus] + IDE_CMD_REG_DRIVE_STATUS);

		// I/O error
		if (status & IDE_STATUS_ERR)
		{
			unsigned	error;

			error = inb(IDE_CMD_REG_ERROR);
			serial_printf("%s() failed, status=%08X error=%08X\r\n", __func__, (unsigned)status, error);
			return	-5;
		}

		// Success
		if ((status & IDE_STATUS_BUSY) == 0)
			break;

		// if (timeout)
		//	return -1;
	}

	return	0;
}

/*
 *	Init IDE subsystem
 *	Scan IDE bus for connected devices (identify)
 */
int	ide_init(unsigned drv_id)
{
	unsigned char	ident_buf[512];
	int	rv;
	char	model_no[81], fw_ver[9], serial_no[21];
	int	bus, device;
	unsigned char	read_buf[512];
	int	disk_num;
	struct ide_ident_params	*ide_ident_params;
	struct ide_priv	*priv;
	
	serial_printf("%s()\r\n", __func__);
	
	disk_num = 0;

	// Identify IDE devices
	for (bus = 0; bus < IDE_NUM_BUSES; ++bus)
	{
		for (device = 0; device < IDE_DRIVES_PER_BUS; ++device, ++disk_num)
		{
			memset(ident_buf, 0, sizeof(ident_buf));
			rv = ide_identify(bus, device, ident_buf);
			serial_printf("%s(): bus=%d device=%d rv=%d (%s)\r\n", __func__, bus, device, rv, 0 == rv ? "disk present" : "no disk");
	
			if (0 == rv)
			{
				memset(model_no, 0, sizeof(model_no));
				memcpy(model_no, ident_buf+54, 80);
				memset(fw_ver, 0, sizeof(fw_ver));
				memcpy(fw_ver, ident_buf+46, 8);
				memset(serial_no, 0, sizeof(serial_no));
				memcpy(serial_no, ident_buf+20, 20);
				serial_printf("Identified IDE device on bus %d, device %d: model_no=`%s' fw_ver=`%s' serial_no=`%s'\r\n",
					bus, device, model_no, fw_ver, serial_no);

				// Record disk parameters
				ide_ident_params = (struct ide_ident_params*)ident_buf;
				priv = &ide_disks_geometries[disk_num];
				priv->bus = bus;
				priv->drive = device;
				priv->num_cyls = htons(ide_ident_params->num_cyls);
				priv->num_heads = htons(ide_ident_params->num_heads);
				priv->sect_per_track = htons(ide_ident_params->sect_per_track);
				priv->byte_per_sec_u = htons(ide_ident_params->byte_per_sec_u);
				priv->byte_per_tr_u = htons(ide_ident_params->byte_per_tr_u);
				serial_printf("Disk parameters: disk_num=%d num_cyls=%u num_heads=%u sect_per_track=%u byte_per_sec_u=%u byte_per_tr_u=%u\r\n",
					disk_num, (unsigned)priv->num_cyls, (unsigned)priv->num_heads,
					(unsigned)priv->sect_per_track, (unsigned)priv->byte_per_sec_u,
					(unsigned)priv->byte_per_tr_u);

				// Recalibrate detected drive (don't know yet if needed)
				rv = ide_recalibrate(bus, device);
				serial_printf("ide_recalibrate() - %s\r\n", 0 == rv ? "succeeded" : "failed");

				disks[exist_disk_num].read = ide_disk_read;
				disks[exist_disk_num].write = ide_disk_write;
				disks[exist_disk_num].disk_priv = &ide_disks_geometries[disk_num];
				
#if 0
				// Test-read 1st sector
				memset(read_buf, 0, sizeof(read_buf));
				//rv = ide_read_sectors(bus, device, 0, 0, 1, 1, read_buf);
				rv = ide_disk_read(&disks[exist_disk_num], 0, read_buf, SECTOR_SIZE);
				if (0 == rv)
				{
					int	i;

					serial_printf("Test read of sector 1 (head 0 cyl. 0) succeeded.\r\n");
					serial_printf("Sector dump:\r\n");
					serial_printf("---------------------------\r\n");
					for (i = 0; i < 512; ++i)
					{
						serial_printf("%02X ", (unsigned)read_buf[i]);
						if (i % 16 == 15)
							serial_printf("\r\n");
					}
					serial_printf("---------------------------\r\n");
				}
				else
				{
					serial_printf("Test read of sector 1 (head 0 cyl. 0) failed (rv=%d)\r\n", rv);
				}
				
				memset(read_buf, 0x96, sizeof(read_buf));
				//rv = ide_write_sectors(bus, device, 0, 0, 1, 1, read_buf);
				rv = ide_disk_write(&disks[exist_disk_num], 0, read_buf, SECTOR_SIZE);
				if (0 == rv)
				{
					serial_printf("Test write of sector 1 (head 0 cyl. 0) succeeded.\r\n");
				}
				else
				{
					serial_printf("Test write of sector 1 (head 0 cyl. 0) failed (rv=%d)\r\n", rv);
				}
#endif	// 0
				
				++exist_disk_num;
			}
		}
	}
	
	return	0;
}


int	ide_deinit(void)
{
	return	0;
}


int	ide_open(unsigned subdev_id)
{
	return	0;
}


int ide_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int ide_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int ide_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
}

int ide_close(unsigned sub_id)
{
	return	0;
}


struct drv_entry	ide = {ide_init, ide_deinit, ide_open, ide_read,
	ide_write, ide_ioctl, ide_close};

