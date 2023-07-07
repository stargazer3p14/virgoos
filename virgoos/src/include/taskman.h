/*
 *	taskman.h
 *
 *	Task management related definitions and declarations.
 *
 *	The task management structure allows the following inter-task communication:
 *
 *		1) Shared memory - no special support, the tasks may simply use the same memory
 *		2) Signals - the tasks may send each other signals
 *		3) Messages - the tasks may send each other messages
 */

#ifndef	TASKMAN__H
#define	TASKMAN__H

#include "sosdef.h"
#include "taskman-arch.h"

// Task manager states
#define	TASKMAN_STATE_NOSWITCH	0x1	// Preemption is disabled

typedef	struct	message
{
	int	num;			// Message code
	void	*prm;		// Parameter(s) - message specific
} MESSAGE;

struct	msg_q
{
	MESSAGE msg;
	struct	msg_q	*next;
} MSG_Q;

typedef	struct	semaphore
{
	dword	count;
	dword	max_count;
	struct task_q	*wait_queue;
} SEMAPHORE;

// It is more convenient to use separate lock()/unlock() for mutexes than use semaphores with count == 1
typedef	struct	mutex
{
	dword	lock;
	struct task_q	*wait_queue;
} MUTEX;

// Multiple events mask
typedef struct	events_mul
{
	unsigned long	events_mask;		// Events of interest
	unsigned long	current_events;		// Current events
	struct task_q	*wait_queue;	
} EVENTS_MUL;

// Events selector queue - priority queue where different tasks wait for different subsets of the same set of events
// The queue is sorted by tasks priorities
typedef	struct	events_sel_q
{
	int	max_events;			// Size of events arrays in bits
	unsigned long	*pevents;		// Current events (usually there will be only one event posted)
	unsigned long	*events_mask;		// Events of interest
	struct task_q	*task;			// This is actually a single task that waits on events selector queue
	struct	events_sel_q	*next, *prev;
} EVENTS_SEL_Q;

/*
typedef	struct	event_q
{
	int	event;
	void	*data;
	struct	event_q	*next, *prev;
}	EVENT_Q;
*/

/*
typedef	struct	wait_q
{
	struct task_list	*task_list;
	struct wait_q	*next;
}	WAIT_Q;
*/

/*
typedef	struct	res_list
{
	int	id;
	void	*res;
	struct res_list	*next;
}	RESOURCE_LIST;
*/

//	Task states
enum	{TASK_READY, TASK_WAITING};

//#define	TASK_SIGNALLED	0x80000000

#define	OPT_TIMESHARE	1			// Task is willing to participate in round robin
#define	OPT_FP		2			// Task is doing floating-point operations

#define	STATE_IN_SIGHANDLER	0x1

//	Resource types
//enum	{ RES_SEMA4, RES_MUTEX, RES_MSGQ };

#define	TASK_QUEUE_FL_NONSYSTEM		0x1		// Means that the queue is not the system's running tasks queue (priority-indexed array from where the tasks are scheduled)
#define	TASK_QUEUE_FL_MIXEDPRIO		0x2		// The queue contains tasks of mixed priority (sorted by priority). OS's running queue is not mixed-priority and non-system queues are usually mixed priority

typedef	struct	task
{
	// Why/when do we need this?
	struct  task_q	**parent_queue_head;		// Parent queue head (where the task is queued)
	// Not needed - task structure doesn't exist without TASK_Q, and the latter already has `queue_flags'.
//	unsigned	parent_queue_flags;		// What is parent queue: OS's running queue or private queue of some event
	dword	stack_size;
	dword	stack_base;
	unsigned	state;
	int	id;					// Meanwhile not used
	unsigned	priority;
	int	options;				// Options for task scheduler
	void	*priv;					// Private structure for sub-system that runs the task (native, pthreads, exec)
							// It is introduced in order to keep environment and other exec-related information
#if 0	
	struct	event_q	*event_first, *event_last;
	struct	sig_handler_q	*sig_handler_first, *sig_handler_last;
	RESOURCE_LIST	*res_list_first, *res_list_last;	// For resource guard
#endif	
	REG_STATE	reg_state;
	FP_STATE	fp_state;
} TASK;

typedef	struct	task_q
{
	TASK	task;
	struct	task_q	*next, *prev;
	unsigned	queue_flags;			// What is this queue: OS's running queue or private queue of some event
} TASK_Q;

/*
struct	task_list
{
	TASK_Q	*task;
	struct task_list *prev, *next;
};
*/

#if 0
//-----------------	Signals ---------------------
//
//	Signals are required by ISO C (hosted), but we don't have real use for them.
//	In September OS there's only a single application (process) with multiple tasks (threads)
//
enum	{ SIG_WAKEUP };

typedef	void	( *SIG_HANDLER )( int sig, void *data );

typedef	struct	sig_handler_q
{
	SIG_HANDLER	handler;
	int	event;
	struct	sig_handler_q	*next, *prev;
}	SIG_HANDLER_Q;
//------------------------------------------------
#endif

typedef	void (*TASK_ENTRY)(void* param);


//	Task management interface.
void    switch_to(TASK_Q *pq);
void	switch_task(TASK_Q *pq);
int	start_task(TASK_ENTRY task_entry, unsigned priority, unsigned options, void *param);
int	start_task_ex(TASK_ENTRY task_entry, unsigned priority, unsigned options, uintptr_t stack_base, uintptr_t stack_size, TASK_Q **ptask, void *param);
//void	start_task_pending(TASK_ENTRY task_entry, unsigned priority, int options, void *param);
void	init_new_task_struct(TASK_Q *pq, TASK_ENTRY task_entry, dword param);				// Architecture-specific task initialization
void	terminate(void);				// Terminate current task
void	end_task(TASK_Q *pq);				// End chosen task
void	enqueue_task(TASK_Q **queue, TASK_Q *task);
void	reschedule(int bound_priority);
TASK_Q	*dequeue_task(TASK_Q **queue);
void	set_running_task_priority(unsigned priority);
void	set_running_task_options(unsigned options);
void	set_task_priority(TASK_Q *task, unsigned priority);
void	set_task_options(TASK_Q *task, unsigned options);

void	call_sig_handlers(void);

void	yield(void);
int	wake(struct task_q **waking);
void	nap(struct task_q **waitq);

void	idle_task(void *unused);

//	Synchronization interface.
void	init_semaphore(SEMAPHORE *sema4, int max_count, int init_count);
void	down(SEMAPHORE *sema4);
void	up(SEMAPHORE *sema4);

void	init_mutex(struct mutex *mutex);
void	lock_mutex(MUTEX *mutex);
void	unlock_mutex(MUTEX *mutex);

//void	init_events_mul(struct events_mul *ev);
//void	wait_events_mul(EVENTS_MUL *ev);
//void	send_events_mul(EVENTS_MUL *ev, unsigned long events);

void	init_events_sel(EVENTS_SEL_Q *sel_q, unsigned long *pevents);
EVENTS_SEL_Q *new_events_sel(int max_events, unsigned long *events_mask);
void	del_events_sel(EVENTS_SEL_Q *sel);
void	remove_events_sel(EVENTS_SEL_Q **sel_q, EVENTS_SEL_Q *p);
void	wait_events_sel(EVENTS_SEL_Q **sel_q, EVENTS_SEL_Q *myself);
void	send_event_sel(EVENTS_SEL_Q **sel_q, int max_events, int event);
void	set_event(unsigned long *pevents, int max_events, int event);
void	clear_event(unsigned long *pevents, int max_events, int event);
int	is_event_set(unsigned long *pevents, int max_events, int event);

void	spin_lock(unsigned *lock_word);

void	disable_preemption(void);
void	enable_preemption(void);
int	get_preemption_state(void);
void	set_preemption_state(int state);

static void	spin_lock_irq(unsigned *lock_word, int irq, dword *irq_mask)
{
	save_irq_mask(irq_mask);
	mask_irq(irq);
	spin_lock(lock_word);
}


static void	spin_unock_irq(unsigned *lock_word, const dword irq_mask)
{
	spin_unlock(lock_word);
	restore_irq_mask(irq_mask);
}


#endif	/* TASKMAN__H */

