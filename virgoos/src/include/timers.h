/*
 *	TIMERS.H
 *
 *	Timers module header.
 */

#ifndef	TIMERS__H
 #define	TIMERS__H

#include "sosdef.h"
#include "timers-arch.h"

typedef	void	(*timer_proc)(void *arg);

#define	TF_PERIODIC	1

typedef	struct timer
{
	long	timeout;			// Number of timer's ticks (this timer's ticks, see 'resolution').
	unsigned long	latch;			// Actual tick count.
	unsigned long	resolution;		// Timer's ticks per second. Cannot be more than system timer's resolution (TICKS_PER_SEC).
	unsigned	flags;			// Periodic or one-shot
	unsigned task_priority;			// Task priority of a timer (tasks with greater priority will block this timer. Set to 0
						// in order to not allow any tasks mask out the timer's reporting
	timer_proc	callback;		// Callback (timer) function
	void	*prm;				// Parameter that will be transferred to callback
}	timer_t;

#ifndef	TIMERS_C
//extern	dword	timer_counter;
#endif

struct	timestamp
{
	unsigned long	sec;
	unsigned long	ticks;
};

#endif	//	TIMERS__H

