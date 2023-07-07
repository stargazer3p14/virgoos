/*
 *	uart-sample.c
 *
 *	Source code of a very minimalistic sample program that is used to test bring-up of SeptemberOS on new platforms
 *
 *	It verifies working of (directly):
 *		+ Basic task management (and indirectly memory allocation/management)
 *		+ Time-sharing multitasking (and indirectly timers working)
 *		+ UART interface (and indirectly some of libc - sprintf() etc.)
 *
 *	And indirectly - if this works then:
 *		+ interrupt support (and all what it takes to) works
 *		+ assembly bootstrapping works
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "drvint.h"

extern dword   timer_counter;

void	task2(void *unused)
{
	int	j = 0;
	dword	last_time = 0;

	serial_printf("%s(): hello\n", __func__);

	while(1)
	{
		if (timer_counter - last_time < TICKS_PER_SEC * 5)
			continue;
		serial_printf( "Task2: %d +++++++++++++++\r\n", ++j );
		last_time = timer_counter;
	}
}


void	task1(void *unused)
{
	int	j = 0;
	dword	last_time = 0;

	serial_printf("%s(): hello\n", __func__);

	// Start second task
	serial_printf("%s(): starting second task\n", __func__);
	start_task(task2, DEF_PRIORITY_LEVEL, OPT_TIMESHARE, NULL);
	
	while (1)
	{
		if (timer_counter - last_time < TICKS_PER_SEC * 5)
			continue;
		serial_printf( "Task1: %d +++++++++++++++\r\n", ++j );
		last_time = timer_counter;
	}

	terminate();
}


void	app_entry()
{
#ifdef START_APP_IN_TASK
	task1(NULL);
#else
	serial_printf("%s(): starting first task \n", __func__);
	start_task(task1, DEF_PRIORITY_LEVEL, OPT_TIMESHARE, (void*)"HELLO, WOLRD");
#endif
//	After start_task() this code will never be reached.
	for (;;)
		;
}

