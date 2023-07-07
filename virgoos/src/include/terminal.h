/*
 *	TERMINAL.H
 *
 *	Sample terminal driver header.
 */

//	Ports definitions.
#define	TERM_CRT_INDEX	0x3D4
#define	TERM_CRT_DATA	0x3D5

//	CRT registers.
#define	CRT_START_HIGH	0xC
#define	CRT_START_LOW	0xD
#define	CRT_CURSOR_HIGH	0xE
#define	CRT_CURSOR_LOW	0xF

//	Config definitions, will move to `config.h' (?)
#define	NUM_TERMINALS	8			// Number of sub-devices.

#define	SCREEN_WIDTH	80
#define	SCREEN_HEIGHT	25

#define	TERM1_PAGE		0
#define	TERM2_PAGE		1
#define	TERM3_PAGE		2
#define	TERM4_PAGE		3
#define	TERM5_PAGE		4
#define	TERM6_PAGE		5
#define	TERM7_PAGE		6
#define	TERM8_PAGE		7

#define	TERM1_LEFT		10
#define	TERM2_LEFT		0
#define	TERM3_LEFT		0
#define	TERM4_LEFT		0
#define	TERM5_LEFT		0
#define	TERM6_LEFT		0
#define	TERM7_LEFT		0
#define	TERM8_LEFT		0

#define	TERM1_TOP		2	
#define	TERM2_TOP		0
#define	TERM3_TOP		0
#define	TERM4_TOP		0
#define	TERM5_TOP		0
#define	TERM6_TOP		0
#define	TERM7_TOP		0
#define	TERM8_TOP		0

#define	TERM1_WIDTH		60
#define	TERM2_WIDTH		80
#define	TERM3_WIDTH		80
#define	TERM4_WIDTH		80
#define	TERM5_WIDTH		80
#define	TERM6_WIDTH		80
#define	TERM7_WIDTH		80
#define	TERM8_WIDTH		80

#define	TERM1_HEIGHT	21
#define	TERM2_HEIGHT	25
#define	TERM3_HEIGHT	25
#define	TERM4_HEIGHT	25
#define	TERM5_HEIGHT	25
#define	TERM6_HEIGHT	25
#define	TERM7_HEIGHT	25
#define	TERM8_HEIGHT	25

#define	DEF_ATTRIB		0x07

// Add CR (\r) to each LF (\n) output
#define	TERM_MODE_UNIX	1

typedef	struct
{
	byte	*buf;
	byte	attrib;
	int		left, top, width, height;
	int		cur_x, cur_y;
	int		busy;
	unsigned	flags;
}	term_t;

