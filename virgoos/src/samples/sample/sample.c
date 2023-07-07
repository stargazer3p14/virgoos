/*
 *	sample.c
 *
 *	Source code of sample SOS application.
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "keyboard.h"

byte *timer_ptr = (uint8_t*)0xB8000;

int	task1_attr, task2_attr;
char task1_msg[] = "***Task1***";
char task2_msg[] = "***Task2***";
int	task2_started = 0;

void	task2(void *unused)
{
	int	i;
	int	j = 0;
	char	buf[ 256 ];

	open_drv(DEV_ID(TERM_DEV_ID, 0));

	while (1)
	{
		for (i = 0; i < strlen(task2_msg); ++i)
		{
			*((byte*)(0xB8000 + (80 * 24 + i + 30) * 2)) = task2_msg[i];
			*((byte*)(0xB8000 + (80 * 24 + i + 30) * 2 + 1)) = task2_attr++;
		}

		sprintf(buf, "Task2: %d +++++++++++++++\r\n", ++j);
//		sprintf(buf, "Task2: +++++\r\n", ++j);
		write_drv(DEV_ID( TERM_DEV_ID, 0), buf, strlen(buf));
	}
}


void	task1(void *unused)
{
	int	i;
	int	ch = 0;
	int	c = 0;
	char	buf[256];

	if (!task2_started)
	{
		start_task(task2, 1, OPT_TIMESHARE, NULL);
		++task2_started;
	}

	// Set keyboard blocking mode to "blocking"
	ioctl_drv(DEV_ID(KBD_DEV_ID, 0), KBD_IOCTL_SET_BLOCKING, 1);
	
	while (1) 
	{
		ch = 0;

		for (i = 0; i < strlen(task1_msg); ++i)
		{
			*((byte*)(0xB8000 + (80 * 24 + i + 50) * 2)) = task1_msg[i];
			*((byte*)(0xB8000 + (80 * 24 + i + 50) * 2 + 1)) = task1_attr++;
		}

		read_drv(DEV_ID(KBD_DEV_ID, 0), &ch, sizeof(int));

		if ( ch >= 1 )
		{
			*(byte*)(0xB8000 + (80*5) * 2) = ((ch>>8) & 0xFF);
			*(byte*)(0xB8000 + (80*5) * 2 + 1) = (0x7);
		}

		printfxy( 2, 24, "Read from keyboard: %x, %c", ( ch & 0xFF ), ( ch >> 8 ) & 0xFF );
		printfxy(2, 23, "Task1: Incrementing a variable. * %d *", c++);

//		strcpy(buf, "Task1: Incrementing a variable. * ");
//		i = __itoa(buf+strlen(buf), c++, 10, 0, 0);
//		i = sprintf(buf+strlen(buf), "%d", c++);
//		strcpy(buf+i, " *");
//		printfxy(2, 23, buf);

//		__hex32toa(i, buf);
//		printfxy(2, 24, buf);

	}

	terminate();
}


void	app_entry()
{
	unsigned char	*p = ( unsigned char *) 0xB8000;
	int	i;

	char	*p1;
	long	*p2;
	char	buf[256];

	start_task(task1, 1, OPT_TIMESHARE, (void*)"HELLO, WOLRD");

//	After start_task() this code will never be reached.
	__asm__ __volatile__ ("label:\n");
//	__asm__("mov eax, offset label");
//	__asm__("nop");
//	__asm__("hlt");
	__asm__("jmp	label");
}
