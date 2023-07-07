/*
 *	KEYBOARD.H
 *
 *	Sample keyboard driver header.
 */

//	Ports numbers
#define	KBD_DATA	0x60
#define	KBD_CONTROL	0x64
#define	KBD_STATUS	0x64

//	Status 
#define	KBD_DATA_READY	1
#define	KBD_INPUT_DATA	2
#define	KBD_SYS			4
#define	KBD_INPUT_CMD	8
#define	KBD_ENABLED		0x10
#define	KBD_TX_TIMEOUT	0x20
#define	KBD_RX_TIMEOUT	0x40
#define	KBD_EVEN_PARITY	0x80

#define	KBD_BUF_LEN		256

typedef	struct
{
	byte	scan;
	byte	ascii;
}	key_t;

#define	KBD_SHIFT_LSHIFT	1
#define	KBD_SHIFT_LALT		2
#define	KBD_SHIFT_LCTL		4
#define	KBD_SHIFT_RSHIFT	8
#define	KBD_SHIFT_RALT		0x10
#define	KBD_SHIFT_RCTL		0x20
#define	KBD_STATUS_NUMS		0x40
#define	KBD_STATUS_CAPS		0x80
#define	KBD_STATUS_SCROLL	0x100
#define	KBD_STATUS_PAUSE	0x200
#define	KBD_STATUS_INSERT	0x400

//	Input modes (IOCTL)
#define	KBD_MODE_NORMAL		1			// Normal mode - return 1 byte MAKE scan code + ASCII
#define	KBD_MODE_SCAN		2			// Scan mode - return full scan code
#define	KBD_MODE_MAKE_AND_BREAK	0x80	// Flag to use with a mode, return BREAK codes also.

#define	KBD_IOCTL_SET_BLOCKING	1		// Set blocking mode
#define	KBD_IOCTL_SET_MODE	2			// Set mode
