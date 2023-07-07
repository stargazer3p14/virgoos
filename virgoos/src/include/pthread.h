#ifndef	PTHREAD__H
#define PTHREAD__H

// Types
//
struct sched_param
{
	int	sched_priority;
};

// SCHED_OTHER is the same as SCHED_FIFO
// SCHED_RR affects `OPT_TIMESHARE' task option
enum {SCHED_FIFO, SCHED_RR, SCHED_OTHER};

typedef unsigned long	pthread_t;
typedef unsigned int pthread_key_t;
typedef struct pthread_attr
{
	struct sched_param	sched_param;
	int	sched_policy;
	int	detach_state;
	int	inherit;
	int	scope;
	void	*stack_addr;
	size_t	stack_size;
} pthread_attr_t;

typedef struct pthread_mutex
{
} pthread_mutex_t;

typedef struct pthread_mutex_attr
{
} pthread_mutexattr_t;

typedef struct pthread_cond
{
} pthread_cond_t;

struct timespec
{
};

typedef struct pthread_condattr_t
{
} pthread_condattr_t;

typedef struct pthread_spinlock_t
{
} pthread_spinlock_t;

typedef struct pthread_barrier_t
{
} pthread_barrier_t;

typedef struct pthread_barrierattr_t
{
} pthread_barrierattr_t;

// Functions
int pthread_create(pthread_t *restrict tid, const pthread_attr_t *restrict attr, void *(*start_routine)(void*), void *restrict arg);
void pthread_exit(void *value_ptr);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_detach(pthread_t tid);
int pthread_kill(pthread_t thread, int sig);
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_equal(pthread_t tid1, pthread_t tid2);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getschedparam(const pthread_attr_t *restrict attr, struct sched_param *restrict param);
int pthread_attr_setschedparam(pthread_attr_t *restrict attr, const struct sched_param *restrict param);
int pthread_attr_getschedpolicy(const pthread_attr_t *restrict attr, int *restrict policy);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getinheritsched(const pthread_attr_t *restrict attr, int *restrict inherit);
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit);
int pthread_attr_getscope(const pthread_attr_t *restrict attr, int *restrict scope);
int pthread_attr_setscope(pthread_attr_t *attr, int scope);
int pthread_attr_getstack(const pthread_attr_t *restrict attr, void **restrict stackaddr, size_t *restrict stacksize);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *restrict attr, size_t *restrict stacksize);
int pthread_attr_getstackaddr(const pthread_attr_t *restrict attr, void **restrict stackaddr);
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);
int pthread_setschedprio(pthread_t target_thread, int prio);

/* Mutex handling.  */

/* Initialize a mutex.  */
int pthread_mutex_init (pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* Destroy a mutex.  */
int pthread_mutex_destroy (pthread_mutex_t *mutex);

/* Try locking a mutex.  */
int pthread_mutex_trylock (pthread_mutex_t *mutex);
  
/* Lock a mutex.  */
int pthread_mutex_lock (pthread_mutex_t *mutex);

/* Wait until lock becomes available, or specified time passes. */
int pthread_mutex_timedlock (pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);
/* Unlock a mutex.  */
int pthread_mutex_unlock (pthread_mutex_t *mutex);

/* Destroy condition variable COND.  */
int pthread_cond_destroy (pthread_cond_t *cond);

/* Wake up one thread waiting for condition variable COND.  */
int pthread_cond_signal (pthread_cond_t *cond);

/* Wake up all threads waiting for condition variables COND.  */
int pthread_cond_broadcast (pthread_cond_t *cond);

/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.

   This function is a cancellation point and therefore not marked with
   THROW.  */
int pthread_cond_wait (pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);

/* Wait for condition variable COND to be signaled or broadcast until
   ABSTIME.  MUTEX is assumed to be locked before.  ABSTIME is an
   absolute time specification; zero is the beginning of the epoch
   (00:00:00 GMT, January 1, 1970).

   This function is a cancellation point and therefore not marked with
   THROW.  */
int pthread_cond_timedwait (pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);

/* Functions for handling condition variable attributes.  */

/* Initialize condition variable attribute ATTR.  */
int pthread_condattr_init (pthread_condattr_t *attr);

/* Destroy condition variable attribute ATTR.  */
int pthread_condattr_destroy (pthread_condattr_t *attr);

/* Get the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_getpshared (const pthread_condattr_t * restrict attr, int *restrict pshared);

/* Set the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_setpshared (pthread_condattr_t *attr, int pshared);

/* Functions to handle spinlocks.  */

/* Initialize the spinlock LOCK.  If PSHARED is nonzero the spinlock can
   be shared between different processes.  */
int pthread_spin_init (pthread_spinlock_t *lock, int pshared);

/* Destroy the spinlock LOCK.  */
int pthread_spin_destroy (pthread_spinlock_t *lock);

/* Wait until spinlock LOCK is retrieved.  */
int pthread_spin_lock (pthread_spinlock_t *lock);

/* Try to lock spinlock LOCK.  */
int pthread_spin_trylock (pthread_spinlock_t *lock);

/* Release spinlock LOCK.  */
int pthread_spin_unlock (pthread_spinlock_t *lock);

/* Functions to handle barriers.  */

/* Initialize BARRIER with the attributes in ATTR.  The barrier is
   opened when COUNT waiters arrived.  */
int pthread_barrier_init (pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned int count);

/* Destroy a previously dynamically initialized barrier BARRIER.  */
int pthread_barrier_destroy (pthread_barrier_t *barrier);

/* Wait on barrier BARRIER.  */
int pthread_barrier_wait (pthread_barrier_t *barrier);

/* Initialize barrier attribute ATTR.  */
int pthread_barrierattr_init (pthread_barrierattr_t *attr);

/* Destroy previously dynamically initialized barrier attribute ATTR.  */
int pthread_barrierattr_destroy (pthread_barrierattr_t *attr);
  
/* Get the process-shared flag of the barrier attribute ATTR.  */
int pthread_barrierattr_getpshared (const pthread_barrierattr_t *restrict attr, int *restrict pshared);

/* Set the process-shared flag of the barrier attribute ATTR.  */
int pthread_barrierattr_setpshared (pthread_barrierattr_t *attr, int pshared);

#endif	// PTHREAD__H

