/*
 *	terminal.c	
 *
 *	A sample terminal device driver.
 *
 *	This driver implements several output terminals using VGA 80*25 color text mode with
 *	video pages.
 *
 *	Any number of terminals may be defined, For each terminal it is defined on which video
 *	page it resides. This way, several terminals may share the same video window and
 *	overlap.
 *
 *	An active terminal is set through ioctl(). It defines an active video page and an
 *	active terminal, which will be diplayed "on top" of the terminals competing for the same
 *	video page.
 *
 *	Up to defined number of terminals may be opened.
 *
 *	Terminal is an output-only device, reads return EINVAL.
 *
 *	For every terminal its window position is defined (terminal may be less than 80*25 and
 *	may top-left not at (0,0) ).
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "errno.h"
#include "terminal.h"


#define		init_term( n )	\
	case	n - 1:	\
		term[ n - 1 ].left = term[ n - 1 ].cur_x = TERM##n##_LEFT;	\
		term[ n - 1 ].top = term[ n - 1 ].cur_y = TERM##n##_TOP;	\
		term[ n - 1 ].width = TERM##n##_WIDTH;	\
		term[ n - 1 ].height = TERM##n##_HEIGHT;	\
		term[ n - 1 ].attrib = DEF_ATTRIB;	\
		term[ n - 1 ].buf = ( byte* ) 0xB8000 + TERM##n##_PAGE * 4096;	\
		term[ n - 1 ].flags = TERM_MODE_UNIX;	\
		break;

term_t	term[NUM_TERMINALS];

int		active_term = 0;

// Prepend output '\n' with '\r' (UNIX mode). Default


static void	set_cursor(int term_num, int x, int y);

/*
 *	Init terminal driver.
 */
int	terminal_init(unsigned drv_id)
{
	return	0;
}

/*
 *	Deinit terminal.
 */
int	terminal_deinit(void)
{
	return	0;
}

/*
 *	Open a terminal.
 */
int	terminal_open(unsigned subdev_id)
{
	int	c, r;

	if (subdev_id >= NUM_TERMINALS)
		return	-ENODEV;

	if (term[ subdev_id ].busy < 1)
	{
		switch(subdev_id)
		{
		default:
			break;
		init_term( 1 )
		init_term( 2 )
		init_term( 3 )
		init_term( 4 )
		init_term( 5 )
		init_term( 6 )
		init_term( 7 )
		init_term( 8 )
		}

		// Clear opened terminal with spaces
		for (r = term[subdev_id].top; r < term[subdev_id].top + term[subdev_id].height; ++r)
			for (c = term[subdev_id].left; c < term[subdev_id].left + term[subdev_id].width; ++c)
				term[subdev_id].buf[(r * SCREEN_WIDTH + c) * 2] = ' ';

		term[subdev_id].busy = 1;
	}

	set_cursor(subdev_id, term[subdev_id].cur_x, term[subdev_id].cur_y);

	return	0;
}

/*
 *	Terminal is output only. Read returns EINVAL.
 */
int terminal_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	-EINVAL;
}


static	void	term_down( int term_num )
{
	++term[ term_num ].cur_y;

	if ( term[ term_num ].cur_y == term[ term_num ].top + term[ term_num ].height )
	{
		int	j;

		for ( j = term[ term_num ].top; j < term[ term_num ].top + term[ term_num ].height - 1; ++j )
			memmove( term[ term_num ].buf + ( j * SCREEN_WIDTH + term[ term_num ].left ) * 2,
				term[ term_num ].buf + ( ( j + 1 ) * SCREEN_WIDTH  + term[ term_num ].left ) * 2,
				term[ term_num ].width * 2 );

		// Fill bottom line with spaces, leave attributes byte
		for (j = term[term_num].left; j < term[term_num].left + term[term_num].width; ++j)
			*(term[term_num].buf + (j + SCREEN_WIDTH * (term[term_num].top + term[term_num].height - 1)) * 2) = ' ';
				
		--term[term_num].cur_y;
	}
}

static void	set_cursor(int term_num, int x, int y)
{
	unsigned pos;

	if (x <  term[term_num].left || x >= term[term_num].left + term[term_num].width ||
		y < term[term_num].top || y >= term[term_num].top + term[term_num].height)
		return;

	pos = y * SCREEN_WIDTH + x;
	outb(TERM_CRT_INDEX, CRT_CURSOR_LOW);
	outb(TERM_CRT_DATA, pos & 0xFF);
	outb(TERM_CRT_INDEX, CRT_CURSOR_HIGH);
	outb(TERM_CRT_DATA, pos >> 8 & 0xFF);
}

/*
 *	Write to a terminal.
 */
int terminal_write(unsigned subdev_id, const void	*buffer, unsigned long length)
{
	int	i;

	if ( subdev_id >= NUM_TERMINALS )
		return	-ENODEV;

	if ( term[ subdev_id ].busy < 1 )
		return	-EBADF;

	for ( i = 0; i < length; ++i )
	{
		if ( term[ subdev_id ].cur_x == term[ subdev_id ].left + term[ subdev_id ].width )
		{
			term[ subdev_id ].cur_x = term[ subdev_id ].left;
			term_down( subdev_id );
		}

		switch ( ( ( byte* )buffer )[ i ] )
		{
		case '\n':
			term_down( subdev_id );
			if (!(term[subdev_id].flags & TERM_MODE_UNIX))
				break;
			// If TERM_FLAG_ASSUME_CR is set, fall through (UNIX mode)
		case '\r':
			term[ subdev_id ].cur_x = term[ subdev_id ].left;
			break;
		case '\t':
			term[ subdev_id ].cur_x += 8 - ( term[ subdev_id ].cur_x & 7 );
			if ( term[ subdev_id ].cur_x >= term[ subdev_id ].left + term[ subdev_id ].width )
			{
				term[ subdev_id ].cur_x -= term[ subdev_id ].width;
				term_down( subdev_id );
			}
			break;
		default:
			term[ subdev_id ].buf[ ( term[ subdev_id ].cur_y * SCREEN_WIDTH +
				term[ subdev_id ].cur_x ) * 2 ] = ( ( byte* )buffer )[ i ];
			term[ subdev_id ].buf[ ( ( term[ subdev_id ].cur_y * SCREEN_WIDTH +
				term[ subdev_id ].cur_x ) * 2 ) + 1 ] = term[ subdev_id ].attrib;
			++term[ subdev_id ].cur_x;
			break;
		}
	}

	set_cursor(subdev_id, term[subdev_id].cur_x, term[subdev_id].cur_y);

	return	length;
}

/*
 *	IOCTL - set configuration parameters.
 */
int terminal_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	if ( subdev_id >= NUM_TERMINALS )
		return	-ENODEV;

	return	0;
}

/*
 *	Close a terminal.
 */
int terminal_close(unsigned subdev_id)
{
	if ( subdev_id >= NUM_TERMINALS )
		return	-ENODEV;

	return	0;
}

drv_entry	terminal = {terminal_init, terminal_deinit, terminal_open, terminal_read, 
	terminal_write, terminal_ioctl, terminal_close};




