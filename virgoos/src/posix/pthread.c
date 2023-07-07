/*****************************************
*
*	pthread.c
*
*	POSIX threads implementation
*
*	NOTES
*
*	POSIX threads implementation in September OS is actually made via parameters translation and calls to task management functions.
*	Tasks structure and task management and scheduling model are compatible with single-process multi-threaded environment.
*	Countrary to that, September OS multi-tasking is not compatible with multiple process model (there is only single address space),
*	and we are not intending to sacrifice real-time and performance properties of the OS for UNIX compatibility (in the manner uclinux
*	did).
*
* 	Get/set thread's stack meanwhile are fictions - eventually task management will include those, too
*
******************************************/

#include "sosdef.h"
#include "taskman.h"
#include "pthread.h"
#include "errno.h"

extern	TASK_Q	*running_task;

struct pthread_task_entry_prm
{
	void	*(*start_routine)(void*);
	void	*restrict arg;
};

// Task entry translation entry point for pthread creation
static void	pthread_task_entry(void *prm)
{
	struct pthread_task_entry_prm	*pthread_task_entry_prm;
	void	*(*start_routine)(void*);
	void	*restrict arg;

	pthread_task_entry_prm = prm;
	start_routine = pthread_task_entry_prm->start_routine;
	arg = pthread_task_entry_prm->arg;
	free(pthread_task_entry_prm);
	start_routine(arg);
	// Return value is ignored (which is what all parameters translation is about)
	// Return from this function actually terminates the task, behavior compatible to return of pthread
}

// NOTE: unlike most system functions, in case of error pthread_create() returns error code rather than -1 and errno set.
int pthread_create(pthread_t *restrict tid, const pthread_attr_t *restrict attr, void *(*start_routine)(void*), void *restrict arg)
{
	struct pthread_task_entry_prm	*prm;
	int	rv;
	unsigned	priority;
	int	options;
	TASK_Q	*pq;

	prm = malloc(sizeof(struct pthread_task_entry_prm));
	if (prm == NULL)
	{
		errno = ENOMEM;
		return	errno;
	}
	prm->start_routine = start_routine;
	prm->arg = arg;

	if (attr != NULL)
	{
		// Get priority and scheduling options from `attr'
		priority = attr->sched_param.sched_priority;
		options = 0;
		if (attr->sched_policy == SCHED_RR)
			options |= OPT_TIMESHARE;
	}
	else
	{
		// Default priority and options are inherited from the creating task.
		priority = running_task->task.priority;
		options = running_task->task.options;
	}

	if (attr == NULL || attr->stack_addr == NULL || attr->stack_size == 0)
		rv = start_task_ex(pthread_task_entry, priority, options, 0, 0, &pq, prm);
	else
		rv = start_task_ex(pthread_task_entry, priority, options, (dword)attr->stack_addr, attr->stack_size, &pq, prm);

	if (rv == 0 && tid != NULL)
		*tid = (pthread_t)pq;

	return	rv;
}

void pthread_exit(void *value_ptr)
{
	terminate();
}

int pthread_attr_init(pthread_attr_t *attr)
{
	attr->sched_param.sched_priority = running_task->task.priority;
	if (running_task->task.options & OPT_TIMESHARE)
		attr->sched_policy = SCHED_RR;
	else
		attr->sched_policy = SCHED_FIFO;
	return	0;
	attr->detach_state = 0;
	attr->stack_addr = NULL;
	attr->stack_size = 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
}

int pthread_equal(pthread_t tid1, pthread_t tid2)
{
	return	!(tid1 == tid2);
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	*detachstate = attr->detach_state;
	return	0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	attr->detach_state = detachstate;
	return	0;
}

int pthread_attr_getschedparam(const pthread_attr_t *restrict attr, struct sched_param *restrict param)
{
	*param = attr->sched_param;
	return	0;
}

int pthread_attr_setschedparam(pthread_attr_t *restrict attr, const struct sched_param *restrict param)
{
	attr->sched_param = *param;
	return	0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *restrict attr, int *restrict policy)
{
	*policy = attr->sched_policy;
	return	0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	attr->sched_policy = policy;
	return	0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *restrict attr, int *restrict inherit)
{
	*inherit = attr->inherit;
	return	0;
}

int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	attr->inherit = inherit;
	return	0;
}

int pthread_attr_getscope(const pthread_attr_t *restrict attr, int *restrict scope)
{
	*scope = attr->scope;
	return	0;
}

int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	attr->scope = scope;
	return	0;
}

int pthread_attr_getstack(const pthread_attr_t *restrict attr, void **restrict stackaddr, size_t *restrict stacksize)
{
}

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *restrict attr, size_t *restrict stacksize);
int pthread_attr_getstackaddr(const pthread_attr_t *restrict attr, void **restrict stackaddr);
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);

int pthread_setschedprio(pthread_t target_thread, int prio)
{
	TASK_Q	*taskq;

	taskq = (TASK_Q*)target_thread;
	set_task_priority(taskq, prio);
	return	0;
}


