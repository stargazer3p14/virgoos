/*
 *	timers.c
 *
 *	Provides timers services for SeptemberOS
 */

#define	TIMERS_C

#include "sosdef.h"
#include "timers.h"
#include "config.h"
#include "queues.h"
#include "taskman.h"


//#define	DEBUG_TIMERS

list_t	*tl_head, *tl_tail;

uint32_t	timer_counter;
uint32_t	uptime;					// In seconds
time_t	system_time;

#ifdef DEBUG_TIMERS
static void	dump_timers(void)
{
	list_t	*l = tl_head;

	while (l)
	{
		serial_printf("[%08X]%08X->", l, l->datum);
		l = l->next;
		if (l == tl_head)
		{
			serial_printf("[LIST LOOP!!!]->");
			break;
		}
	}
	serial_printf("NULL\n");
}
#endif

static	long	cmp_timers(void *tm1, void *tm2)
{
	timer_t	*t1 = tm1, *t2 = tm2;

	return	t1->timeout - t2->timeout;
}


int	install_timer(timer_t *tm)
{
	list_t	*new_timer;
	dword	intfl;
	
	intfl = get_irq_state();
	disable_irqs();
#ifdef DEBUG_TIMERS
	serial_printf("%s(): installing timer %08X\n", __func__, tm);
#endif
	new_timer = malloc(sizeof(list_t));
	new_timer->datum = tm;
	tm->latch = tm->timeout * TICKS_PER_SEC / tm->resolution;
	list_sorted_insert(&tl_head, &tl_tail, new_timer, cmp_timers);
#ifdef DEBUG_TIMERS
	serial_printf("%s(): installed timer %08X\n", __func__, tm);
	serial_printf("%s(): dumping timers\n", __func__);
	dump_timers();
#endif
	restore_irq_state(intfl);
	return	1;
}


int	remove_timer(timer_t *tm)
{
	list_t	*l;
	dword	intfl;

	intfl = get_irq_state();
	disable_irqs();
#ifdef DEBUG_TIMERS
	serial_printf("%s(): removing timer %08X\n", __func__, tm);
#endif
	if (l = list_find(tl_head, tm, cmp_list_entries))
	{
#ifdef DEBUG_TIMERS
		serial_printf("%s(): found timer %08X, deleting\n", __func__, tm);
		serial_printf("%s(): dumping timers BEFORE list_delete\n", __func__);
		dump_timers();
#endif
		list_delete(&tl_head, &tl_tail, l);
		free(l);
	}

#ifdef DEBUG_TIMERS
	serial_printf("%s(): dumping timers\n", __func__);
	dump_timers();
#endif
	restore_irq_state(intfl);
	return	1;
}

// Convert struct timeval to timeout.
// Timeout is number of ticks, except for special values:
// 0 - means polling (immediate timeout)
// UINT_MAX means NEVER time out
unsigned long	timeval_to_ticks(const struct timeval *tv)
{
	unsigned long	rv;
	
	if (NULL == tv)
		return	TIMEOUT_INFINITE;
	rv = tv->tv_sec * TICKS_PER_SEC + tv->tv_usec / (1000000 / TICKS_PER_SEC);
	return	rv;
}

extern	TASK_Q	*running_task;

int	timer_isr(void)
{
	list_t	*l;
	timer_t	*t;

//#define	TEST_INT_RESPONSE

#ifdef	TEST_INT_RESPONSE
	extern	dword	int_handler_start[2];
	extern	dword	int_received[2];
#endif

#ifdef	TEST_INT_RESPONSE
	__asm__ __volatile__ ("rdtsc" : "=a"(int_handler_start[0]), "=d"(int_handler_start[1]));
	if (int_handler_start[0] < int_received[0])
		--int_handler_start[1];
	printfxy(0, 0, "     Timer interrupt latency is %u_%u (cycles)     ", int_handler_start[1] - int_received[1], 
		int_handler_start[0] - int_received[0]);
#endif

	plat_timer_eoi();
	++timer_counter;

	if (timer_counter % TICKS_PER_SEC == 0)
	{
		++uptime;
		++system_time;
#ifdef DEBUG_TIMERS
		if (timer_counter % (TICKS_PER_SEC * 5) == 0)
			serial_printf("%s(): 5 seconds\n", __func__);
#endif
	}

	for (l = tl_head; l; l = l->next)
	{
		t = (timer_t*)(l->datum);

		//
		//	A known MSVC bug: if ( --t -> latch ) compiles into an unconditional branch,
		//	equivalent to if ( 1 ).
		//	Known from MSVC 4.0 this bug happily lives in MSVC 6.0
		//
		if (--t->latch >= 1)
			continue;

		list_delete(&tl_head, &tl_tail, l);
		if (t->flags & TF_PERIODIC)
		{
			t->latch = t->timeout * TICKS_PER_SEC / t->resolution;
			list_sorted_insert(&tl_head, &tl_tail, l, cmp_timers);
		}
		else
			free(l);

		// Timer routing is called AFTER checking of periodic timer and re-inserting it.
		// Otherwise, if callback chose to remove timer from list and free it (!), it would be re-inserted
		if (running_task == NULL || running_task->task.priority >= t->task_priority)
			t->callback(t->prm);

	}

	return	1;
}


void	init_timers(void)
{
	plat_init_timers();
}


void	init_sys_time(void)
{
	plat_init_sys_time();
}

