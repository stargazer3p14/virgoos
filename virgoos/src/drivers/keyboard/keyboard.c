/*
 *	KEYBOARD.C
 *
 *	A sample keyboard device driver.
 *
 *	This driver does the basic keystroke processing. Character keys scan codes are
 *	queued in kbd_q[] buffer. Buffer size is definable in header (KBD_BUF_LEN). The driver
 *	translates scan codes to ASCII, considering shift keys status.
 *
 *	This driver serves as an example for input-only devices.
 *
 *******************************************************************************************
 *
 *	This source file is part of SOS project. Copyright (c) Vadim Drubetsky, Benny Lifshitz,
 *	2002.
 *	Please see the accompanying license file for license details.
 *
 *	DISCLAIMER.
 *	This software is provided "AS IS", without warranty of any kind, including but
 *	not limited to implied warranty for merchantability or fitness for a particular purpose.
 *	In no event (except for when otherwise is stated in the applicable law) shall the authors
 *	be liable for any damage arising of use or inability to use the seftware.
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "errno.h"
#include "queues.h"
#include "keyboard.h"
#include "taskman.h"

#define	MAX_SCAN_LEN	6

byte	scan_code[ MAX_SCAN_LEN ];
int		cur_byte;
int		num_bytes;

key_t	kbd_q[ KBD_BUF_LEN ];
int		kbd_q_head, kbd_q_tail;

word	shift_status;

byte	scan_tbl[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB,
			0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
			0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
			0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C, 0x39,
			0x1C, 0x0E, 0x0C, 0x0D, 0x1A, 0x1B, 0x27, 0x28, 0x29,
			0x2B, 0x33, 0x34, 0x35 };

#define	CHAR_TBL_LEN	sizeof( scan_tbl )

byte	ascii_tbl[] = { 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
		'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
		's', 't', 'u', 'v', 'w', 'x', 'y', 'z', ' ',
		0xD, 0x08, '-', '=', '[', ']', ';', '\'', '`',
		'\\', ',', '.', '/' };

byte	shift_ascii_tbl[] = { 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
			'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
			'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' ',
			0xD, 0x08, '_', '+', '{', '}', ':', '"', '~',
			'|', '<', '>', '?' };

int	mode = KBD_MODE_NORMAL;

static int	blocking_mode = 0;

struct task_q	*kbd_wait_queue;

/*
 *	Enqueue scan code into kbd_q. Converts scan key to ASCII (if applicable) and inserts it
 *	into the keyboard queue.
 *	If the queue is full, the key is lost.
 *	Pause key is not received at all in NORMAL mode.
 */
static	void	enqueue_scan_code( void )
{
	key_t	new_key;
	int	i;

	//	Scan mode will be implemented later.
	if ( mode != KBD_MODE_NORMAL )
		return;

	if ( q_is_full( kbd_q_head, kbd_q_tail, KBD_BUF_LEN ) )
		return;

	if ( num_bytes > 1 )
		return;

	for ( i = 0; i < CHAR_TBL_LEN; ++i )
		if ( scan_code[ 0 ] == scan_tbl[ i ] )
			break;

	if ( i == CHAR_TBL_LEN )
		return;

	new_key.scan = scan_code[ 0 ];
	if ( shift_status & ( KBD_SHIFT_LSHIFT | KBD_SHIFT_RSHIFT | KBD_STATUS_CAPS ) )
		new_key.ascii = shift_ascii_tbl[ i ];
	else
		new_key.ascii = ascii_tbl[ i ];

	q_insert( kbd_q, kbd_q_head, kbd_q_tail, KBD_BUF_LEN, new_key );
}


/*
 *	Keyboard IRQ bottom-half processing task
 */
void	keyboard_isr_bh(void *unused)
{
	static int	c;

	printfxy(0, 17, "%s(): c=%d; terminating", __func__, c++);
//	terminate();
//	yield();
}


/*
 *	Keyboard IRQ handler.
 */
static	int	keyboard_isr( void )
{
	byte	stat;
	byte	code;
	static int	first_time;
	
//#define	TEST_INT_RESPONSE

#ifdef	TEST_INT_RESPONSE
	extern	dword	int_handler_start[2];
	extern	dword	int_received[2];
#endif

#ifdef	TEST_INT_RESPONSE
	__asm__ __volatile__ ("rdtsc" : "=a"(int_handler_start[0]), "=d"(int_handler_start[1]));
	if (int_handler_start[0] < int_received[0])
		--int_handler_start[1];
	printfxy(0, 7, "     Keyboard interrupt latency is %u_%u (cycles)     ", int_handler_start[1] - int_received[1], 
		int_handler_start[0] - int_received[0]);
#endif

	stat = inb( KBD_STATUS );
	if ( !stat & KBD_DATA_READY )
		return	1;
	code = inb( KBD_DATA );

	//
	//	Current scan code's order number.
	//
	switch ( cur_byte )
	{
	case	0:
		switch ( code )
		{
		case	0xE0:
			num_bytes = 2;
			break;

		case	0xE1:
			num_bytes = 3;
			break;

		default:
			if ( code >= 0xE2 )
				break;

			num_bytes = 1;
			scan_code[ cur_byte++ ] = code;
			break;
		}
		break;

	case	1:
		scan_code[ cur_byte++ ] = code;
		break;

	default:
		//	Insert MAKE codes.
		scan_code[ cur_byte++ ] = code;
		break;
	}

	if ( num_bytes >= 1 && cur_byte == num_bytes )
	{
		if ( num_bytes <= 2 )
		{
			//	Make code
			if ( ( scan_code[ num_bytes - 1 ] & 0x80 ) == 0 )
			{
				enqueue_scan_code();

				if ( num_bytes == 1 )
				{
					switch ( scan_code[ 0 ] )
					{
				 	// Nums lock
				 	case 0x45:
						shift_status ^= KBD_STATUS_NUMS;
						udelay(1);
						outb(KBD_DATA, 0xED);
						//udelay(1);
						// Wait for keyboard's ACK
						do
						{
							;
						} while (inb(KBD_DATA) != 0xFA);
						outb(KBD_DATA, 
							(shift_status & KBD_STATUS_CAPS) >> 5 |
							(shift_status & KBD_STATUS_NUMS) >> 5 |
							(shift_status & KBD_STATUS_SCROLL) >> 8);
						break;

				 	// Caps lock
				 	case 0x3A:
						shift_status ^= KBD_STATUS_CAPS;
						udelay(1);
						outb(KBD_DATA, 0xED);
						//udelay(1);
						// Wait for keyboard's ACK
						do
						{
							;
						} while (inb(KBD_DATA) != 0xFA);
						outb(KBD_DATA, 
							(shift_status & KBD_STATUS_CAPS) >> 5 |
							(shift_status & KBD_STATUS_NUMS) >> 5 |
							(shift_status & KBD_STATUS_SCROLL) >> 8);
						break;

					// Left SHIFT
					case 0x2A:
						shift_status |= KBD_SHIFT_LSHIFT;
						break;

					// Right SHIFT
					case 0x36:
						shift_status |= KBD_SHIFT_RSHIFT;
						break;

					// Left ALT
					case 0x38:
						shift_status |= KBD_SHIFT_LALT;
						break;

					// Left CTRL
					case 0x1D:
						shift_status |= KBD_SHIFT_LCTL;
						break;

					// Scroll lock
					case 0x46:
						shift_status ^= KBD_STATUS_SCROLL;
						udelay(1);
						outb(KBD_DATA, 0xED);
						//udelay(1);
						// Wait for keyboard's ACK
						do
						{
							;
						} while (inb(KBD_DATA) != 0xFA);
						outb(KBD_DATA, 
							(shift_status & KBD_STATUS_CAPS) >> 5 |
							(shift_status & KBD_STATUS_NUMS) >> 5 |
							(shift_status & KBD_STATUS_SCROLL) >> 8);
						break;

					default:
						break;
					}
				}
				else if ( num_bytes == 2 )
				{
					if ( scan_code[ 0 ] == 0xE0 )
					{
						// Right ALT
						if ( scan_code[ 1 ] == 0x38 )
							shift_status |= KBD_SHIFT_RALT;
						else if ( scan_code[ 1 ] == 0x1D )
							shift_status |= KBD_SHIFT_RCTL;
					}
				}
			}

			// Break code
			else
			{
				if ( mode == KBD_MODE_NORMAL )
				{
					if ( num_bytes == 1 )
					{
						switch ( scan_code[ 0 ] & 0x7F )
						{
						// Left SHIFT
						case 0x2A:
							shift_status &= ~KBD_SHIFT_LSHIFT;
							break;

						// Right SHIFT
						case 0x36:
							shift_status &= ~KBD_SHIFT_RSHIFT;
							break;

						// Left ALT
						case 0x38:
							shift_status &= ~KBD_SHIFT_LALT;
							break;

						// Left CTRL
						case 0x1D:
							shift_status &= ~KBD_SHIFT_LCTL;
							break;
						}
					}
					else if ( num_bytes == 2 )
					{
						if ( scan_code[ 0 ] == 0xE0 )
						{
							// Right ALT
							if ( scan_code[ 1 ] == 0x38 )
								shift_status &= ~KBD_SHIFT_RALT;
							else if ( scan_code[ 1 ] == 0x1D )
								shift_status &= ~KBD_SHIFT_RCTL;
						}
					}
				}
			}
		}
	}

	if ( num_bytes == cur_byte )
		num_bytes = cur_byte = 0;

	// TESTTESTTEST - start tasklet (once, during the first ISR) - in order to test delayed_task_start()
	//if (first_time++ == 0)
	//	start_task_pending(keyboard_isr_bh, 0, 0, NULL);

	// Wake up napping tasks (if any)
	if (kbd_wait_queue != NULL)
	{
//serial_printf("%s(): kbd_wait_queue=%08X, waking\n", __func__, kbd_wait_queue);
		wake(&kbd_wait_queue);
	}
	
	return	1;
}


/*
 *	Init keyboard driver. Init keyboard and check for ACK.
 */
int	keyboard_init(unsigned drv_id)
{
	byte	ack;

	set_int_callback(KBD_IRQ, keyboard_isr);
	return	0;
}

/*
 *	Deinit keyboard.
 */
int	keyboard_deinit(void)
{
	return	0;
}

/*
 *	Open a keyboard.
 */
int	keyboard_open(unsigned subdev_id)
{
	if ( subdev_id >= 1 )
		return	-ENODEV;

	return	0;
}

/*
 *	Extract an entry from a keyboard buffer.
 *	Return: sizeof( key_t ) if OK
 *			0 if buffer is empty
 *			error is something is wrong.
 */
int keyboard_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	if ( subdev_id >= 1 )
		return	-ENODEV;

	if ( length < sizeof( key_t ) )
		return	-EINVAL;

	if ( q_is_empty( kbd_q_head, kbd_q_tail ) )
	{
		if (0 == blocking_mode)
			return	0;
		nap(&kbd_wait_queue);
	}

	*((key_t*)buffer) = kbd_q[kbd_q_head];
	q_delete(kbd_q_head, kbd_q_tail, KBD_BUF_LEN);
	return	sizeof(key_t);
}

/*
 *	Write a keyboard.
 */
int keyboard_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	-EINVAL;
}

/*
 *	IOCTL - set configuration parameters.
 */
int keyboard_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	long	arg;

	if ( subdev_id >= 1 )
		return	-ENODEV;

	if (KBD_IOCTL_SET_BLOCKING == cmd)
	{
		arg = va_arg(argp, long);
		blocking_mode = arg;
	}
	
	return	0;
}

/*
 *	Close a keyboard.
 */
int keyboard_close(unsigned sub_id)
{
	return	0;
}


drv_entry	keyboard = {keyboard_init, keyboard_deinit, keyboard_open, keyboard_read, 
	keyboard_write, keyboard_ioctl, keyboard_close};

