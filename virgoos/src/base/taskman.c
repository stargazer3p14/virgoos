/*
 *	taskman.c
 *
 *	Task manager for SeptemberOS.
 *
 *	Implementation highlights:
 *
 *	26/01/2010.
 *	10/08/2010 -- updated
 *
 *	The highlights below need to be reworked - many things changed. Signals are removed, however they may be comfortable for
 *	custom asynchronous notifications (e.g. interrupts).
 *
 *	1.	Task scheduling queues are organized in multi-dimensional arrays. There are 256 (redefineable) priority levels,
 *		and a separate task queue for each legal state for each priority level. Currently there are two possible states
 *		for every task - ready and waiting. Greater number means lower priority.
 *
 *	2.	Task queues are organized as doubly-linked cyclic buffers. The current task for each queue is pointed by its
 *		head. With such an organization the most common actions, such as insertion of a new task, deletion of a task or
 *		transfer from one queue to another perform in O(1).
 *
 *	3.	Task scheduler has hard real-time properties:
 *		a)	tasks are assigned priority level by their creation; priority changes are allowed
 *		b)	no tasks of some priority ever get CPU time if there is a task with higher priority ready to run
 *		c)	tasks run to completion or until blocked by default. Round-robin time sharing is allowed between the tasks on
 *			the same priority level. Time sharing is allowed only if all tasks on the running priority level have time-
 *			sharing option "on". Time-sharing option is supplied for task creation and may be changed during run-time. When there are
 *			time-sharing and non-time-sharing tasks on the same priority level, time sharing will be masked until all
 *			non-time-sharing tasks become not runnable (blocked or terminated). In other words, time-sharing is a convention
 *			that mut be supported by ALL runnable tasks on the same level to come into effect
 *
 *	4.	Tasks may be only in one of two possible states - `ready' and `waiting'.
 *		a)	In `ready' state tasks wait for their turn to get CPU time.
 *		b)	In `waiting state' tasks wait for an event. An event is delivered to the task in form of signal (therefore,
 *		a task must have a signal handler installed in order to receive events.
 *
 *	5.	Spinlocks are not necessary because multiprocessing (for now) is not allowed. For convenience, spinlocks are implemented with
 *		non-waiting mutexes (busy-polled mutex state).
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"
#include "errno.h"

/*
 *	Specifies that an IRQ callback handler is running and task switching activity 
 *	must be postponed until the callback returns (bitmask for each IRQ)
 */
dword	running_irq = 0;
//	Specifies that reschedule() should be called by the end of an IRQ processing
int	pending_reschedule = 0;
int	pending_reschedule_priority = NUM_PRIORITY_LEVELS;

//	A task pending switch to
TASK_Q	*task_pending = NULL;

// A task is pending to be started
int	pending_start_task = 0;
// Parameters to call start_task
TASK_ENTRY	pending_task_entry;
int	pending_task_priority;
int	pending_task_options;
void *pending_task_param;

TASK_Q	*task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];
TASK_Q	*running_task;
//int	no_time_share[NUM_PRIORITY_LEVELS];

dword	current_stack_top = STACK_START;

void	reschedule(int bound_priority);
static	void	timer_handler(void *unused);

timer_t	taskman_timer = {TICKS_PER_SLICE, 0, TICKS_PER_SEC, TF_PERIODIC, 0, timer_handler, NULL};

int	after_terminate = 0;

dword	switch_task_lock;
dword	switch_to_lock;

int	in_task_switch;
static unsigned	taskman_state;

/*
 *	Timer IRQ handler for task scheduler reschedules if round-roubin is allowed for running priority.
 */
static	void	timer_handler(void *unused)
{
	if (!running_task)
		return;

	//if (!no_time_share[running_task->task.priority])
	if (running_task->task.options & OPT_TIMESHARE)
	{
		if (task_q[running_task->task.priority][TASK_READY]->next != NULL)
			task_q[running_task->task.priority][TASK_READY] = task_q[running_task->task.priority][TASK_READY]->next;
		reschedule(running_task->task.priority);
	}
}


/*
 *	Idle task. Runs only when there are no other tasks to run (on the lowest level)
 */
void	idle_task(void *unused)
{
	extern	dword	timer_counter;
	int	trig = 1;

	while (1)
	{
		if ((timer_counter % TICKS_PER_SEC) == 0)
		{
			if (trig)
			{
				serial_printf("idle_task()\n");
				trig = 0;
			}
		}
		else
		{
			trig = 1;
		}
	}
}


void	init_taskman()
{
	install_timer(&taskman_timer);
}


/*
 *	Enqueues a task in a task queue.
 *
 *	In:	queue - double pointer to queue head.
 *		task - pointer to task item.
 *
 *	Assumption: task is not NULL.
 *	NOTE: the caller must know which queue the task is inserted to (system/not, mixed-priority/not) and set up parent_queue_flags in `task' structure appropriately
 */
void	enqueue_task(TASK_Q **queue, TASK_Q *task)
{
	int	intfl;

	intfl = get_irq_state();
	disable_irqs();

	if (*queue)
	{
		// Handle priority enqueueing - custom wait queues may include tasks with different priorities.
		// Among tasks with equal priorities the new task is inserted last
		TASK_Q	*q;
		
		task->task.parent_queue_head = queue;
		task->queue_flags = (*queue)->queue_flags;

		// All priority checking is relevant only for mixed-priority queues. So for system' scheduling queue enqueue may be optimized
		if ((*queue)->queue_flags & TASK_QUEUE_FL_MIXEDPRIO)
		{
			q = *queue;
			do
			{
				// `task' is inserted before `q'
				if (task->task.priority < q->task.priority)
				{
					task->next = q;
					task->prev = q->prev;
					q->prev = task;
					if (task->prev)
						task->prev->next = task;
					// If `task' has higher priority than `queue' (queue head), it becomes the new queue head
					if (q == *queue)
					{
						*queue = task;
						// If `queue' was single element, fix to make the queue cyclic. It is put under (q==*queue) condition because we know that at least `q' and `task' exist now in the queue,
						// and `task' is inserted before `queue'. So the only case when someone's `next' or `prev' is NULL here is that `q' was `*queue' and `task' was inserted before it
						if (!task->prev)
						{
							task->prev = (*queue);
							(*queue)->next = task;
						}
					}
					goto	restore_intfl;
				}
				q = q->next;
			} while (!(q == *queue || NULL == q));
		}
		
		// `task' was not inserted anywhere in the middle of the queue (due to its priority comparison) and is inserted as last element
		task->next = *queue;
		task->prev = (*queue)->prev;
		(*queue)->prev = task;
		if (task->prev)
			task->prev->next = task;

		// If `queue' was single element, fix to make the queue cyclic
		if (!task->prev)
		{
			task->prev = (*queue);
			(*queue)->next = task;
		}
	}
	else
	{
		// `task' is the first element in `queue', which was empty
		task->next = NULL;
		task->prev = NULL;
		*queue = task;
		(*queue)->queue_flags = task->queue_flags;
		task->task.parent_queue_head = queue;
	}

restore_intfl:
#if defined(arm) || defined(mips)
	restore_irq_state(intfl);
#endif
	// We need it to make `restore_intfl' label valid on x86
	return;
}


/*
 *	Dequeues a task from a task queue.
 *
 *	In:		queue - double pointer to the queue head (which is a task to be dequeued ).
 *	Out:	the dequeued task.
 */
TASK_Q	*dequeue_task(TASK_Q **queue)
{
	TASK_Q	*task;
	int	intfl;

	if (!(*queue))
		return	NULL;

	intfl = get_irq_state();
	disable_irqs();

	task = *queue;

	if ((*queue)->prev)
		(*queue)->prev->next = (*queue)->next;
	if ((*queue)->next)
	{
		(*queue)->next->prev = (*queue)->prev;
		*queue = (*queue)->next;
		if ((*queue)->next == *queue)
			(*queue)->next = (*queue)->prev = NULL;
	}
	else
		*queue = NULL;

	//	Update time sharing option for the priority.
	//if ( !( task -> task.options & OPT_TIMESHARE ) )
	//	--no_time_share[ task -> task.priority ];

	task->prev = task->next = NULL;
	task->task.parent_queue_head = NULL;
	task->queue_flags = 0;
	restore_irq_state(intfl);
	return	task;
}


/*
 *	Deletes a task from a task queue.
 *
 *	In:	queue - double pointer to the queue head.
 *		task - task to be deleted
 *
 *	Out:	the deleted from queue task.
 *
 *	This function deletes a given task from a queue, not necessarily from queue head (this is different from dequeue_task())
 */
TASK_Q	*delete_task(TASK_Q **queue, TASK_Q *task)
{
	TASK_Q	*q;
	int	intfl;

	if (task == *queue)
		return	dequeue_task(queue);
		
	intfl = get_irq_state();
	disable_irqs();

	q = *queue;
	do
	{
		if (q == task)
		{
			if (q->prev)
				q->prev->next = q->next;
			if (q->next)
				q->next->prev = q->prev;
			q->prev = task->next = NULL;
			q->task.parent_queue_head = NULL;
			q->queue_flags = 0;
			restore_irq_state(intfl);
			return	q;
		}
		q = q->next;
	} while (!(q == *queue || NULL == q));

	restore_irq_state(intfl);
	return	NULL;
}


/*
 *	Starts a new task.
 *
 *	In:		task_entry - entry point for task
 *			priority - task priority level
 *			options - round-robin allowed, etc.
 *
 *	Out:	0 if task was started, non-0 if failed (errno is set)
 *
 *	Remarks:	if task creation is successful, start_task() inserts the new task at front of
 *				its priority level queue.
 */
int	start_task(TASK_ENTRY task_entry, unsigned priority, unsigned options, void *param)
{
	TASK_Q	*pq;

	pq = malloc(sizeof(*pq));			// malloc() is used instead of calloc()
	if (!pq)
	{
err_ret:
		errno = ENOMEM;
		return	-1;
	}

	pq->task.priority = priority;
	pq->task.options = options;
	pq->task.stack_size = DEF_STACK_SIZE;
	pq->task.stack_base = (dword)malloc(pq->task.stack_size);			// Use malloc() instead of calloc() - we don't really need to zero the new task's stack, and we don't want to waste time for that
	if (!pq->task.stack_base)
	{
		free(pq);
		goto	err_ret;
	}
	pq->queue_flags = 0;						// System queue, no mixed priority
	pq->task.priv = NULL;

	// this is architecture-dependent part (currently machine-dependent, so that each machine even on same architecture may employ its own task management structure
	init_new_task_struct(pq, task_entry, (dword)param);

	// BUGBUGBUGBUGBUG!!!!! - generic NASTY BUG in scheduler for ALL architectures! Of course, a newly created task must be enqueued ONLY after its state was completely filled and set up
	// It sometimes showed up as using `current_stack_top' working instead of calloc() not working - simply because calloc() used more time... and interrupts were enabled, so on emulators timer interrupt occurred and triggered a task switch while
	// the new `pq' is ALREADY QUEUED on its priority level queue, but NOT FILLED!
	enqueue_task(&(task_q[priority][TASK_READY]), pq);
	//	Update time sharing option for the priority.
	//if (!(pq->task.options & OPT_TIMESHARE))
	//	++no_time_share[pq->task.priority];

	if (running_task != NULL && running_task->task.priority < priority)
		priority = running_task->task.priority;

	// Reschedule here ONLY if new task is of greater priority then the currenly running
	if (running_task == NULL || priority < running_task->task.priority)
		reschedule(priority);

	return	0;
}


/*
 *	Accepts additionally stack base address and size.
 *	The parameters are not checked, the caller must really know what he is doing
 */
int	start_task_ex(TASK_ENTRY task_entry, unsigned priority, unsigned options, uintptr_t stack_base, uintptr_t stack_size, TASK_Q **ptask, void *param)
{
	TASK_Q	*pq;

	pq = malloc(sizeof(*pq));			// malloc() is used instead of calloc()
	if (!pq)
	{
err_ret:
		errno = ENOMEM;
		return	-1;
	}

	pq->task.priority = priority;
	pq->task.options = options;
	if (stack_base != 0 && stack_size != 0)
	{
		pq->task.stack_size = stack_size;
		pq->task.stack_base = stack_base;
	}
	else
	{
		pq->task.stack_size = DEF_STACK_SIZE;
		pq->task.stack_base = (dword)malloc(pq->task.stack_size);			// Use malloc() instead of calloc() - we don't really need to zero the new task's stack, and we don't want to waste time for that
		if (!pq->task.stack_base)
		{
			free(pq);
			goto	err_ret;
		}
	}
	pq->queue_flags = 0;						// System queue, no mixed priority
	pq->task.priv = NULL;

	// Architecture-dependent task initialization
	init_new_task_struct(pq, task_entry, (dword)param);

	enqueue_task(&(task_q[priority][TASK_READY]), pq);

	if (ptask != NULL)
		*ptask = pq;

	//	Update time sharing option for the priority.
	//if (!(pq->task.options & OPT_TIMESHARE))
	//	++no_time_share[pq->task.priority];

	if (running_task != NULL && running_task->task.priority < priority)
		priority = running_task->task.priority;

	// Reschedule here ONLY if new task is of greater priority then the currenly running
	if (running_task == NULL || priority < running_task->task.priority)
		reschedule(priority);

	return	0;
}

/*
 *	Terminates the current task.
 */
void	terminate()
{
	int	bound_priority;
	TASK_Q	*task;

	// There's no enable_irq() or restore_irq() counterpart call - reschedule() will restore the new task's IF state
	disable_irqs();
	
	bound_priority = running_task->task.priority;
	task = dequeue_task(&(task_q[bound_priority][TASK_READY]));
	//	Update time sharing option for the priority.
	//if (!(task->task.options & OPT_TIMESHARE))
	//	--no_time_share[task->task.priority];
	free((void*)running_task->task.stack_base);
	free(task);
	running_task = NULL;
	reschedule(bound_priority);
}


// End chosen task
// This actually doesn't terminate task immediately, because task may be queued on "private" queue for some event; September OS doesn't keep track or notice of such queues.
// If we forced task's deletion, it will confuse the event waking code, which would have no notice on this and will try to wake and already non-existent task.
// Therefore we only mark task for deletion and it will terminate itself when scheduled to run (woken).
// We may seek and forcily delete tasks that are on ready queues for their priority level
void	end_task(TASK_Q *pq)
{
	int	intfl;

	if (pq == running_task)
		terminate();		// Doesn't return

	disable_irqs();
	
	intfl = get_irq_state();
	disable_irqs();
	delete_task(pq->task.parent_queue_head, pq);
	//	Update time sharing option for the priority.
	//if (!(pq->task.options & OPT_TIMESHARE))
	//	--no_time_share[pq->task.priority];
	free((void*)pq->task.stack_base);
	free(pq);
	restore_irq_state(intfl);
}

// Set new priority for the current task. If the task is somewhere on the scheduler's ready queue, it will be re-queued for its new priority level
void	set_running_task_priority(unsigned priority)
{
	TASK_Q *q;
	int	intfl;
	unsigned old_priority;

	if (priority == running_task->task.priority)
		return;

	old_priority = running_task->task.priority;
	intfl = get_irq_state();
	disable_irqs();
	q = dequeue_task(&task_q[old_priority][TASK_READY]);
	q->task.priority = priority;
	q->queue_flags = 0;						// System queue, no mixed priority
	enqueue_task(&task_q[priority][TASK_READY], q);

	// If the task lowered its priority, we need to reschedule.
	if (old_priority < priority)
		reschedule(old_priority);
	restore_irq_state(intfl);
}

// Set options (mainly intended for time-sharing / not) of the current task
void	set_running_task_options(unsigned options)
{
	running_task->task.options = options;
}

void	set_task_priority(TASK_Q *task, unsigned priority)
{
	int	intfl;
	unsigned	old_priority;
	unsigned	parent_queue_flags;
	
	if (task == running_task)
	{
		set_running_task_priority(priority);
		return;
	}

	old_priority = task->task.priority;
	parent_queue_flags = task->queue_flags;
	intfl = get_irq_state();
	disable_irqs();
	delete_task(task->task.parent_queue_head, task);
	task->task.priority = priority;

	// Restore `parent_queue_flags' in task's structure before calling enqueue_task() - it is now required
	task->queue_flags = parent_queue_flags;						// System queue, no mixed priority
	if (parent_queue_flags & TASK_QUEUE_FL_NONSYSTEM)
		enqueue_task(task->task.parent_queue_head, task);
	else
	{
		enqueue_task(&task_q[priority][TASK_READY], task);
		if (priority < old_priority)
			reschedule(priority);
	}

	restore_irq_state(intfl);
}

void	set_task_options(TASK_Q *task, unsigned options)
{
	task->task.options = options;
}

/*
 *	Reschedules to run another task. Normally it is necessary after termination, suspension 
 *	of a task or release of a resource.
 *	At entry running_task is not valid.
 *	reschedule() scans priorities lower (or equal to) bound_priority for any task in READY queue.
 *
 *	NOTE: reschedule() is safe to call from IRQ handler, with IRQs disabled (due to pending_reschedule flag); therefore it is safe to call start_task() from IRQ handler, too (even if not desirable due to performance issues with malloc())
 */
void	reschedule(int bound_priority)
{
	int	i;
	TASK_Q	*q;

//serial_printf("%s(): called, bound_priority=%d\n", __func__, bound_priority);
	if (running_irq)
	{
		pending_reschedule = 1;
		if (bound_priority < pending_reschedule_priority)
			pending_reschedule_priority = bound_priority;
		return;
	}
	
	if (taskman_state & TASKMAN_STATE_NOSWITCH)
		return;

	pending_reschedule = 0;
	pending_reschedule_priority = NUM_PRIORITY_LEVELS;
	
	for (i = bound_priority; i < NUM_PRIORITY_LEVELS; ++i)
	{
		q = task_q[i][TASK_READY];
		if (q)
		{
			switch_task(q);
			return;
		}
	}

	// No tasks ready to run are found, proceed with idle loop
	// NOTE: idle_loop is not handled correctly, waking tasks don't restart.
	// DON'T allow entering idle_loop(), it is for diagnostics only! If there are no tasks that are guaranteed to
	// be always running (albeit may be preempted), run idle_task() from the first task entry.
	running_task = NULL;
	serial_printf("Idle_loop: halting (IRQs enabled)\n");

//	__asm__ __volatile__ ("mov esp, eax\n" : : "a"(IDLE_STACK) );
//	__asm__ __volatile__ ("idle_loop:\n"
//		"nop\n"
//		"hlt\n"
//		"jmp	idle_loop\n");

	// Enable IRQs, so that they can be serviced and waiting tasks will get chance to wake up
	enable_irqs();
	for(;;)
		;
}


/*
 *	Preemption control - for forced preemption only (time-sharing)
 */
void	disable_preemption(void)
{
	taskman_state != TASKMAN_STATE_NOSWITCH;
}

void	enable_preemption(void)
{
	taskman_state &= ~TASKMAN_STATE_NOSWITCH;
}

int	get_preemption_state(void)
{
	return	(taskman_state & TASKMAN_STATE_NOSWITCH) == 0;
}

void	set_preemption_state(int state)
{
	if (state)
		enable_preemption();
	else
		disable_preemption();
}

/*
 *	Task yields CPU (probably waiting for some event).
 */
void	yield()
{
	nap(&task_q[running_task->task.priority][TASK_WAITING]);
}


/*
 *	Take a nap on a waiting queue
 */
void	nap(struct task_q **waitq)
{
	int	priority;
	struct	task_q	*running;

//serial_printf("%s(): called, waitq=%08X running_task=%08X (takes a nap), running priority=%d \n", __func__, waitq, running_task, running_task->task.priority);
	priority = running_task->task.priority;
	running = dequeue_task(&(task_q[priority][TASK_READY]));
	//	Update time sharing option for the priority.
	//if (!(running->task.options & OPT_TIMESHARE))
	//	--no_time_share[running->task.priority];
	
	if (*waitq)
		running->queue_flags = (*waitq)->queue_flags;						// System queue, no mixed priority
	else
		running->queue_flags = TASK_QUEUE_FL_NONSYSTEM | TASK_QUEUE_FL_MIXEDPRIO;			// Best guess: wait queue is non-system and mixed-priority
	enqueue_task(waitq, running);
	reschedule(priority);
}


/*
 *	Wakes up a task previously put to sleep
 *
 *	(!) `waking' may not be empty. It's caller's responsibility to check (reconsider?)
 */
int wake(struct task_q **waking)
{
	int	priority;
	struct task_q	*q;

	q = dequeue_task(waking);
//serial_printf("%s(): called, wakingq=%08X running_task=%08X waking task=%08X, running priority=%d waking priority=%d \n", __func__, waking, running_task, q, running_task->task.priority, q->task.priority);
	if (q == NULL)
	{
		errno = EINVAL;
		return	-1;
	}
	priority = q->task.priority;
	q->queue_flags = 0;						// Task is going to ready queue: system, not mixed-priority
	enqueue_task(&(task_q[priority][TASK_READY]), q);
	//	Update time sharing option for the priority.
	//if (!(q->task.options & OPT_TIMESHARE))
	//	++no_time_share[q->task.priority];

	// Reschedule only if the waking task's priority can compete with running tasks's priority
	if (running_task == NULL || priority <= running_task->task.priority)
		reschedule(priority);
	return	0;
}


struct misc_sleep_struct
{
	struct task_q	*waitq;
	void *timer_struct;
};


static	void	misc_sleep_timer_handler(void *prm)
{
	struct misc_sleep_struct *misc_sleep_struct = prm;
	struct task_q *waking = misc_sleep_struct->waitq;

	// The timer is one-shot, so there is no reason to delete it specially - it will be removed from timer list and not added again
	free(misc_sleep_struct->timer_struct);
	free(misc_sleep_struct);
	wake(&waking);
}


unsigned int sleep(unsigned int seconds)
{
	struct misc_sleep_struct *misc_sleep_struct;
	timer_t	*sleep_tmr;
	time_t	nap_time, wake_time;

	if (seconds == 0)
		return	0;

	sleep_tmr = calloc(1, sizeof(timer_t));
	if (sleep_tmr == NULL)
	{
		errno = ENOMEM;
		return	-1;
	}
	misc_sleep_struct = malloc(sizeof(struct misc_sleep_struct));
	if (misc_sleep_struct == NULL)
	{
		free(sleep_tmr);
		errno = ENOMEM;
		return	-1;
	}

	sleep_tmr->timeout = seconds * TICKS_PER_SEC;
	sleep_tmr->resolution = TICKS_PER_SEC;
	sleep_tmr->callback = misc_sleep_timer_handler;
	sleep_tmr->prm = misc_sleep_struct;
	misc_sleep_struct->timer_struct = sleep_tmr;
	install_timer(sleep_tmr);
	nap_time = time(NULL);
	nap(&misc_sleep_struct->waitq);
	wake_time = time(NULL);

	return	wake_time - nap_time - seconds;	
}


void	init_semaphore(SEMAPHORE *sema4, int max_count, int init_count)
{
	sema4->max_count = max_count;
	sema4->count = max_count;
	sema4->wait_queue = NULL;
}

void	init_mutex(MUTEX *mutex)
{
	mutex->lock = 0;
	mutex->wait_queue = NULL;
}

//------------------- Multiple events ----------------------------
//
//	Tasks may wait for multiple events - up to 32 at a time. Single event wait is also possible with this case
//
void	init_events_mul(struct events_mul *ev)
{
	ev->events_mask = 0;
	ev->current_events = 0;
	ev->wait_queue = NULL;
}

// Returns events mask
void	wait_events_mul(EVENTS_MUL *ev)
{
	// Lowest-bit event takes priority - this is shorter
	if (!ev->current_events)
	{
		// Sleep until there's an event - somebody wakes us up
		nap(&ev->wait_queue);
	}
}

void	send_events_mul(EVENTS_MUL *ev, unsigned long events)
{
	ev->current_events |= events & ev->events_mask;
	if (ev->current_events)
		if (ev->wait_queue != NULL)
			wake(&ev->wait_queue);
			
	// We wake the waiting task, but it's consumer's responsibility to clear the consumed events
}
//-----------------------------------------------------------------------------

//----------------------- Multiple events selector ----------------------------
//
//	Tasks may wait for any subset of a set of multiple events
//	There's a separate priority queue, on which task of the highest priority with at least one event of *its* selected mask
//	wakes up.
//
//	This mechanism is intended primarily for select() sockets multiplexer, but it may have use in other similar situations, too
//
void	init_events_sel(EVENTS_SEL_Q *sel_q, unsigned long *pevents)
{
	sel_q->pevents = pevents;
	sel_q->events_mask = 0;
	sel_q->task = NULL;
	sel_q->next = NULL;
	sel_q->prev = NULL;
}


EVENTS_SEL_Q *new_events_sel(int max_events, unsigned long *events_mask)
{
	EVENTS_SEL_Q *new;
	size_t	sz;
	int	i;

	new = calloc(1, sizeof(EVENTS_SEL_Q));
	if (new == NULL)
	{
ret_null:
		return	 NULL;
	}
	new->next = NULL;
	new->prev = NULL;

	sz = max_events / CHAR_BIT;
	if (max_events & CHAR_BIT-1)
		++sz;
	
	new->events_mask = calloc(1, sz);
	if (new->events_mask == NULL)
	{
ret_null1:
		free(new);
		goto	ret_null;
	}
	new->pevents = calloc(1, sz);
	if (new->pevents == NULL)
	{
		free(new->events_mask);
		goto	ret_null1;
	}
	memcpy(new->events_mask, events_mask, sz);
	return	new;
}


void	del_events_sel(EVENTS_SEL_Q *sel)
{
	if (!sel)
		return;

	if (sel->events_mask)
		free(sel->events_mask);
	if (sel->pevents)
		free(sel->pevents);
	free(sel);
}


void	remove_events_sel(EVENTS_SEL_Q **sel_q, EVENTS_SEL_Q *p)
{
	if (!p)
		return;

	if (NULL == p->prev)
		*sel_q = p->next;
	else
		p->prev->next = p->next;
	if (p->next != NULL)
		p->next->prev = p->prev;
	p->next = NULL;
	p->prev = NULL;
}


void	wait_events_sel(EVENTS_SEL_Q **sel_q, EVENTS_SEL_Q *myself)
{
	int	priority;
	
serial_printf("%s(): called\n", __func__);
	// Need to wait
	if (NULL == *sel_q)
		*sel_q = myself;
	else
	{
		EVENTS_SEL_Q *p;
		
		for (p = *sel_q; p->next != NULL && p->task->task.priority <= priority; p = p->next)
			;
		
		if (p->task->task.priority > priority)
		{
			// Insert before "p"
			myself->next = p;
			myself->prev = p->prev;
			if (p->prev)
				p->prev->next = myself;
			else
				*sel_q = myself;
			p->prev = myself;
		}
		else
		{
			// Insert after "p" (as last element)
			myself->next = p->next;
			p->next = myself;
			myself->prev = p;
		}
	}

	nap(&myself->task);
}


void	send_event_sel(EVENTS_SEL_Q **sel_q, int max_events, int event)
{
	EVENTS_SEL_Q	*p;
	int	i;
	
	if (NULL == *sel_q || event >= max_events)
		return;
	
	for (p = *sel_q; p != NULL; p = p->next)
		if (is_event_set(p->events_mask, max_events, event))
			break;
			
	// Nobody was interested in this event (yet)
	if (!p)
		return;
		
	// "p" is interested in this event
	remove_events_sel(sel_q, p);

	set_event(p->pevents, max_events, event);
	// Event is posted.
	wake(&p->task);
}


void	set_event(unsigned long *pevents, int max_events, int event)
{
	if (pevents && event < max_events)
		pevents[event / (sizeof(unsigned long) * CHAR_BIT)] |= 1 << event % (sizeof(unsigned long) * CHAR_BIT);
}

void	clear_event(unsigned long *pevents, int max_events, int event)
{
	if (pevents && event < max_events)
		pevents[event / (sizeof(unsigned long) * CHAR_BIT)] &= ~(1 << event % (sizeof(unsigned long) * CHAR_BIT));
}


int	is_event_set(unsigned long *pevents, int max_events, int event)
{
	if (!pevents || event >= max_events)
		return	0;

	if (pevents[event / (sizeof(unsigned long) * CHAR_BIT)] & 1 << (event % (sizeof(unsigned long) * CHAR_BIT)))
		return	1;

	return	0;
}


//-----------------------------------------------------------


//---------------------------- Spinlock ------------------------
void	spin_lock(unsigned *lock_word)
{
	_spin_lock(lock_word);
}
//-------------------------------------------------------------

