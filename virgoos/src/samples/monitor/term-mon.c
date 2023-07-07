/*
 *	term-mon.c
 *
 *	Source for terminal-based (keyboard+screen) monitor program
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"
#include "keyboard.h"

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
	write_drv(DEV_ID(TERM_DEV_ID, 0), buf, strlen(buf));
	va_end(arg);
}


void	app_entry(void)
{
	int	i;
	int	ch = 0;
	char	buf[256];
	char	c;

	// Since we will sleep, we need an idle task in order not to confuse the task manager
//	start_task(idle_task, NUM_PRIORITY_LEVELS - 1, 0, NULL);
	
	// Open monitor (screen terminal)
	open_drv(DEV_ID(TERM_DEV_ID, 0));

	// Set keyboard blocking mode to "blocking"
	ioctl_drv(DEV_ID(KBD_DEV_ID, 0), KBD_IOCTL_SET_BLOCKING, 1);
	
	// Prepared monitor "frame"
	for (i = 9; i < 71; ++i)
	{
		cprintfxy(3, i, 1, "-");
		cprintfxy(3, i, 23, "-");
	}
	for (i = 2; i < 23; ++i)
	{
		cprintfxy(3, 9, i, "|");
		cprintfxy(3, 70, i, "|");
	}

	monitor_printf(monitor_prompt);
	i = 0;
	while (1)
	{
		ch = 0;
		read_drv(DEV_ID(KBD_DEV_ID, 0), &ch, sizeof(int));
		printfxy(2, 24, "Read from keyboard: %x, %c", (ch & 0xFF), (ch >> 8) & 0xFF);
		// Skip key releases
		c = ch >> 8 & 0xFF;
		if (0 == c)
			continue;

		write_drv(DEV_ID(TERM_DEV_ID, 0), &c, 1);

		buf[i++] = c;
		if ('\r' == c)
		{
			buf[i++] = '\n';
			buf[i] = '\0';
			c = '\n';
			write_drv(DEV_ID(TERM_DEV_ID, 0), &c, 1);			// Echo character back
			run_cmd(buf);
			monitor_printf(monitor_prompt);
			i = 0;
		}
	}

//	terminate();

// Terminating app_entry() enters infinite loop. Consider giving build-time (run-time?) choice between infinite loop and reboot
}

