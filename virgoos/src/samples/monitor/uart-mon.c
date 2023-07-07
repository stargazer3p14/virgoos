/*
 *	uart-mon.c
 *
 *	Source for UART-based monitor program
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"


#define	MAX_MONITOR_CMD_SIZE	256
#define	MAX_MONITOR_LINE	1024

static char monitor_prompt[] = "SeptOS:\\>";

void	run_cmd(char *cmdline);

int	monitor_printf(char *fmt, ...)
{
	char	buf[MAX_MONITOR_LINE];
	va_list	arg;

	va_start(arg, fmt);
	vsprintf(buf, fmt, arg);
	serial_printf(buf);
	va_end(arg);
}


void	app_entry(void)
{
	int	i;
	int	ch = 0;
	char	buf[256];

	monitor_printf(monitor_prompt);
	i = 0;
	while (1)
	{
		ch = 0;
		read_drv(DEV_ID(UART_DEV_ID, 0), &ch, sizeof(unsigned char));
		if (ch < 1)
			continue;

		write_drv(DEV_ID(UART_DEV_ID, 0), &ch, 1);

		buf[i++] = ch;
		// CR
		if ('\r' == ch)
		{
			ch = '\n';
			buf[i++] = ch;
			write_drv(DEV_ID(UART_DEV_ID, 0), &ch, 1);			// Echo character back
			run_cmd(buf);
			monitor_printf(monitor_prompt);
			i = 0;
		}
		// BSpace
		else if (ch == 8)
		{
			if (--i)		// Remove BSpace itself
			{
				ch = ' ';
				write_drv(DEV_ID(UART_DEV_ID, 0), &ch, 1);
				ch = 8;
				write_drv(DEV_ID(UART_DEV_ID, 0), &ch, 1);
				--i;	// Remove previous symbol if it was entered
			}
		}
	}

//	terminate();

// Terminating app_entry() enters infinite loop. Consider giving build-time (run-time?) choice between infinite loop and reboot
}


