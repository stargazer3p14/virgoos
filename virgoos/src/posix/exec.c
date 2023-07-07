/*****************************************
*
*	exec.c
*
*	POSIX execve() and friends implementation
*
*	SeptemberOS'es execve() differs from UNIX/Linux: there are no address space separations. Therefore, execve() doesn't replace the currently running program, but instead just runs a specified task with argv[] array
*
*	The OS keeps a list of "program" names with their entry points; whenever a program is executed, a task for it is created, which runs its entry point with the specified parameters. Parameters are allocated dynamically
*	(it doesn't matter if the original parameters are local) and destroyed when task entry "stub" is returned to (the "process" "exits").
*
*	In general the process resembles executing programs under DOS and uCLinux (Linux for CPUs without MMU); primary difference is that "programs" are not loaded from anywhere, not relocated and not dynamically linked.
*	Rationale for this implementation is easier porting and easier programming for people accustomed to UNIX/Linux/POSIX programming.
*	Its inclusion in build is still questionable (but may get "residence" if found convenient and doesn't weigh much). execve() is an approximation of fork() + execve() on UNIX (it runs in a separate thread)
*	One more difference of this execve() implementation from UNIX/Linux is that in SeptemberOS execve() always returns. On success, 0 is returned
*	execle(), execlp(), execvp() etc. functions are implemented. Functions with suffix 'p' are aliases for functions without such suffix - since there are
*	no program files loaded and no path search
*
*	fork() is not implemented in any, even dummy, way, since its behavior is not compatible with SeptemberOS and doesn't make any sense within the OS
*
*	SeptemberOS doesn't have a parent-child hierarchy of tasks; therefore normally traditional UNIX/Linux waiting for children doesn't make sense. However, within execve() implementation it may be done, too (only for tasks that were started via execve()).
*
*****************************************/

#define	EXEC__C

#include "config.h"
#include "sosdef.h"
#include "taskman.h"
#include "errno.h"
#include "signal.h"

extern struct task_q	*running_task;
extern TASK_Q	*task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];

typedef	int (*MAIN_FUNC)(int, char**);

int	ls_main(int argc, char **argv);
int	testapp_main(int argc, char **argv);

struct program
{
	char	*name;
	int (*main)(int, char**);
} programs[] =
{
	{"/bin/ls", ls_main},
	{"./testapp", testapp_main},
};
#define	NUM_PROGRAMS	(sizeof(programs) / sizeof(programs[0]))

struct execve_task_entry_prm
{
	int	(*main)(int, char**);
	int	argc;
	char	**argv;
	char	**environ;
	void	*parent;
};

#define	SIGNALS_QUEUE_SIZE	32

struct exec_task_priv
{
	char	**environ;
	void	*parent;
	int	*pexit_code;			// Needed?
	void	(*atexit_func)(void);
	TASK_Q	*waitpid_q;
	pid_t	waited_pid;
	int	child_exit_code;
	sighandler_t	sig_handlers[_NSIG];
	int	pending_signals[SIGNALS_QUEUE_SIZE];
	int	pending_sigs_head;
	int	pending_sigs_tail;
};

// Two simple sample processes to run
int     ls_main(int argc, char **argv)
{
	pid_t	ppid;

	printf("%s(): hello, world!\n", __func__);
	ppid = getppid();
	printf("%s(): ppid=%08X\n", __func__, ppid);
	printf("%s(): seding signal 16 to parent\n", __func__);
	kill(ppid, 16);
	printf("%s(): returning 12\n", __func__);
	return	12;
}


void	testapp_sig_handler(int sig)
{
	printf("%s(): sig=%d\n", __func__, sig);
}


int     testapp_main(int argc, char **argv)
{
	int	i;
	char	**e;
	int	child_ret_code;
	char	filename[256] = "/bin/ls";
	char	**argv1, **envp;
	int	rv;

	argv1 = malloc(sizeof(char*) * 3);
	argv1[0] = filename;
	argv1[1] = "-l";
	argv1[2] = NULL;

	envp = malloc(sizeof(char*) * 3);
	envp[0] = "PATH=/bin";
	envp[1] = "HELLO=world";
	envp[2] = NULL;

	set_running_task_options(OPT_TIMESHARE);

	printf("%s(): argc=%d\n", __func__, argc);
	for (i = 0; i < argc; ++i)
		printf("\targv[%d]='%s'\n", i, argv[i] ? argv[i] : "NULL");
	printf("Environment:\n");
	for (e = ((struct exec_task_priv*)(running_task->task.priv))->environ; *e; ++e)
		printf("\t%s\n", *e);

	printf("%s(): installing signal 16\n", __func__);
	signal(16, testapp_sig_handler);
	printf("%s(): signal 16 installed\n", __func__);

	printf("%s(): Running %s\n", __func__, filename);
	rv = execve(filename, argv1, envp);
	printf("%s(): execve() returned %d, calling waitpid()\n", __func__, rv);
	rv = waitpid(-1, &child_ret_code, 0);
	printf("%s(): waitpid() returned %08X, child_ret_code=%d\n", __func__, rv, child_ret_code);

	return	0;
}
///////////////////////////////////////

// Task entry translation entry point for execve creation
static void	execve_task_entry(void *prm)
{
	struct execve_task_entry_prm	*execve_task_entry_prm;
	int	(*main)(int argc, char **argv);
	int	exit_code;
	int	argc;
	char	**argv;
	char	**envp;
	struct task_q	*parent;
	struct exec_task_priv	*exec_task_priv, *pexec_task_priv;
	int	i;
	int	wake_parent;

	execve_task_entry_prm = prm;
	main = execve_task_entry_prm->main;
	argc = execve_task_entry_prm->argc;
	argv = execve_task_entry_prm->argv;
	envp = execve_task_entry_prm->environ;
	parent = execve_task_entry_prm->parent;
	free(execve_task_entry_prm);

	exec_task_priv = malloc(sizeof(struct exec_task_priv));
	if (exec_task_priv == NULL)
	{
		exit_code = -1;
		goto	ret;
	}
	exec_task_priv->environ = envp;
	exec_task_priv->parent = parent;
	exec_task_priv->pexit_code = NULL;
	exec_task_priv->atexit_func = NULL;
	exec_task_priv->waited_pid = 0;
	exec_task_priv->waitpid_q = NULL;
	exec_task_priv->child_exit_code = 0;
	for (i = 0; i < _NSIG; ++i)
		exec_task_priv->sig_handlers[i] = NULL;
	exec_task_priv->pending_sigs_head = 0;
	exec_task_priv->pending_sigs_tail = 0;

	running_task->task.priv = exec_task_priv;

	exit_code = main(argc, argv);
ret:
	// Return value is ignored, unless consumed by some task that called wait() / waitpid()
//	if (exec_task_priv->pexit_code != NULL)
//		*exec_task_priv->pexit_code = exit_code;

	pexec_task_priv = parent->task.priv;
	if (pexec_task_priv && (pexec_task_priv->waited_pid == -1 || pexec_task_priv->waited_pid == (pid_t)running_task))
	{
		pexec_task_priv->child_exit_code = exit_code;
		pexec_task_priv->waited_pid = (pid_t)running_task;
		wake(&pexec_task_priv->waitpid_q);
	}

	for (i = 0; argv[i]; ++i)
		free(argv[i]);
	free(argv);
	for (i = 0; envp[i]; ++i)
		free(envp[i]);
	free(envp);

	free(running_task->task.priv);

	// Return from this function actually terminates the task, behavior compatible to return of main() (or call to exit) on UNIX
}


// Aid for system() implementation
int	cmdline_to_argv(char *cmdline, char ***argv)
{
	int	i;
	int	c;
	char	*v[MAX_CMD_PARAMS];

	for (c = 0; c < MAX_CMD_PARAMS; ++c)
	{
		while (isspace(*cmdline))
			++cmdline;
		for (i = 0; !isspace(cmdline[i]) && cmdline[i] != '\0'; ++i)
			;
		if (i)
		{
			v[c] = malloc(i+1);
			if (v[c] == NULL)
			{
err_ret:
				for (i = 0; i < c; ++i)
					free(v[i]);
				errno = ENOMEM;
				return	-1;
			}
			memcpy(v[c], cmdline, i);
			v[c][i] = '\0';
		}
		if (cmdline[i] == '\0')
			break;
	}
	*argv = malloc((c + 1) * sizeof(char*));
	if (*argv == NULL)
		goto	err_ret;
	memcpy(*argv, v, c * sizeof(char*));
	(*argv)[c] = NULL;

	return	0;
}


static MAIN_FUNC	find_prog_main(const char *name)
{
	int	i;

	for (i = 0; i < NUM_PROGRAMS; ++i)
		if (strcmp(programs[i].name, name) == 0)
			return	programs[i].main;

	return	NULL;
}


int execve(const char *name, char *const argv[], char *const envp[])
{
	int	c;
	char	**v;
	char	**p;
	int	rv;
	struct execve_task_entry_prm	*prm;
	int	i;
	unsigned	priority;
	int	options;
	int	(*main)(int, char**);	
	int	j, k;

	// Allocate and copy argv[]
	for (c = 0; argv[c] != NULL; ++c)
		;

	v = malloc((c+1) * sizeof(char*));	// argv[] is 2 more than args num: argv[argc] is NULL and argv[argc+1] is envp
	if (v == NULL)
	{
		errno = ENOMEM;
err_ret1:
		return	-1;
	}

	for (j = 0; j < c; ++j)
	{
		v[j] = malloc(strlen(argv[j]) + 1);
		if (!v[j])
		{
		errno = ENOMEM;
err_ret2:
			for (k = 0; k < j; ++k)
				free(v[k]);
			goto	err_ret1;
		}
		strcpy(v[j], argv[j]);
	}

	// Allocate and copy envp[]
	for (i = 0; envp[i] != NULL; ++i)
		;

	p = malloc((i+1) * sizeof(char*));
	if (p == NULL)
	{
		errno = ENOMEM;
err_ret3:
		free(v);
		goto	err_ret2;
	}

	for (j = 0; j < i; ++j)
	{
		p[j] = malloc(strlen(envp[j]) + 1);
		if (!p[j])
		{
		errno = ENOMEM;
err_ret4:
			for (k = 0; k < j; ++k)
				free(p[k]);
			goto	err_ret3;
		}
		strcpy(p[j], envp[j]);
	}

	// Allocate execve() parameters for new task
	prm = malloc(sizeof(struct execve_task_entry_prm));
	if (prm == NULL)
	{
		errno = ENOMEM;
err_ret5:
		goto	err_ret4;
	}

	main = find_prog_main(name);
	if (main == NULL)
	{
		errno = ENOENT;
err_ret6:
		free(prm);
		goto	err_ret5;
	}

	prm->main = main;
	prm->argc = c;
	prm->argv = v;
	prm->environ = p;
	prm->parent = running_task;

	priority = running_task->task.priority;
	options = running_task->task.options;

	errno = 0;
	rv = start_task_ex(execve_task_entry, priority, options, 0, 0, NULL, prm);
	if (rv)
		goto	err_ret6;
	return	0;
}

int execvp(const char *file, char *const argv[])
{
	char	**envp = NULL;
	struct exec_task_priv   *exec_task_priv;	

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv != NULL)
		envp = exec_task_priv->environ;
	return	execve(file, argv, envp);
}

int execv(const char *path, char *const argv[])
{
	return	execvp(path, argv);
}

int execle(const char *path, const char *arg, ...)
{
	int	c = 0;
	char	*v[MAX_CMD_PARAMS];
	char	**p;
	va_list	argp;

	va_start(argp, arg);
	do
	{
		v[c++] = (char*)va_arg(argp, char*);	
		if (v[c-1] == NULL)
		{
			p = (char**)va_arg(argp, char**);
			break;
		}
		if (c == MAX_CMD_PARAMS - 1)
		{
			v[c++] = NULL;
			p = NULL;
			break;
		}
	} while (1);

	va_end(argp);
	return	execve(path, v, p);
}

int execlp(const char *file, const char *arg, ...)
{
	int	c = 0;
	char	*v[MAX_CMD_PARAMS];
	char	**p = NULL;
	va_list	argp;
	struct exec_task_priv   *exec_task_priv;	

	va_start(argp, arg);
	do
	{
		v[c++] = (char*)va_arg(argp, char*);	
		if (v[c-1] == NULL)
			break;
		if (c == MAX_CMD_PARAMS - 1)
		{
			v[c++] = NULL;
			break;
		}
	} while (1);

	va_end(argp);

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv != NULL)
		p = exec_task_priv->environ;
	return	execve(file, v, p);
}

void exit(int status)
{
	struct exec_task_priv   *exec_task_priv;
	struct exec_task_priv	*parent_task_priv;
	TASK_Q	*parent;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv != NULL)
	{
		if (exec_task_priv->pexit_code != NULL)
			*exec_task_priv->pexit_code = status;
		if (exec_task_priv->atexit_func)
			exec_task_priv->atexit_func();

		// If parent process waits for us or for any child, wake it up
		parent = exec_task_priv->parent;
		if (parent != NULL)
		{
			parent_task_priv = parent->task.priv;
			if (parent_task_priv != NULL)
			{
				if (parent_task_priv->waited_pid == (pid_t)running_task || parent_task_priv->waited_pid == -1)
				{
					// Set current task's priority to highest so that woken parent won't take over CPU and suspend freeing
					// resources of the exitting task. We are exitting anyway, left only a call to terminate()
					// Set PID with which parent's wait() will return in case that parent was waiting for any PID
					set_running_task_priority(0);
					parent_task_priv->waited_pid = (pid_t)running_task;
					wake(&parent_task_priv->waitpid_q);
				}
			}
		}
	}

	terminate();
}

int atexit(void (*function)(void))
{
	struct exec_task_priv   *exec_task_priv;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv != NULL)
	{
		exec_task_priv->atexit_func = function;
		return	0;
	}

	errno = EINVAL;
	return	-1;
}

pid_t wait(int *status)
{
	return	waitpid(-1, status, 0);
}

// Options are ignored
pid_t waitpid(pid_t pid, int *status, int options)
{
	struct exec_task_priv   *exec_task_priv;
	int	child_pid;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv == NULL)
	{
		errno = EINVAL;
		return	-1;
	}

	exec_task_priv->waited_pid = pid;			// In case of -1 will be corrected by terminating child in-place
	nap(&exec_task_priv->waitpid_q);
	child_pid = exec_task_priv->waited_pid;
	exec_task_priv->waited_pid = 0;
	*status = exec_task_priv->child_exit_code;
	return	child_pid;
}

sighandler_t signal(int signum, sighandler_t handler)
{
	sighandler_t	prev;
	struct exec_task_priv   *exec_task_priv;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv == NULL || signum < 0 || signum > _NSIG)
	{
		errno = EINVAL;
		return	(sighandler_t)-1;
	}

	prev = exec_task_priv->sig_handlers[signum];
	exec_task_priv->sig_handlers[signum] = handler;
	return	prev;
}

// Meanwhile only SIGKILL is handled, other signal are faked and not called
// POSIX sending signals to all tasks by pid == -1 is not supported
int kill(pid_t pid, int sig)
{
	TASK_Q	*target;
	struct exec_task_priv   *task_priv;

	target = (TASK_Q*)pid;
	task_priv = target->task.priv;

	if (task_priv == NULL || sig < 0 || sig > _NSIG)
	{
		errno = EINVAL;
		return	-1;
	}

	// If signals queue is full, signal is lost
	if ((task_priv->pending_sigs_tail + 1) % SIGNALS_QUEUE_SIZE == task_priv->pending_sigs_head)
		return	0;

	task_priv->pending_signals[task_priv->pending_sigs_tail] = sig;
	task_priv->pending_sigs_tail = (task_priv->pending_sigs_tail + 1) % SIGNALS_QUEUE_SIZE;

	// If sending to self, call signal handlers
	if (target == running_task)
	{
		running_task->task.state |= STATE_IN_SIGHANDLER;
		call_sig_handlers();
		running_task->task.state &= ~STATE_IN_SIGHANDLER;
	}

	return	0;
}

pid_t getpid(void)
{
	struct exec_task_priv   *exec_task_priv;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv == NULL)
	{
		errno = EINVAL;
		return	-1;
	}

	return	(pid_t)running_task;
}

pid_t getppid(void)
{
	struct exec_task_priv   *exec_task_priv;

	exec_task_priv = running_task->task.priv;
	if (exec_task_priv == NULL)
	{
		errno = EINVAL;
		return	-1;
	}

	return	(pid_t)exec_task_priv->parent;
}

int raise(int sig)
{
	return	kill(getpid(), sig);
}

// Calls all pending signal handlers for 'running_task'
// Is called by task manager when switching to a new task right after old task's state was saved, but the new task's state is not loaded
// (and no need to save it additionally)


// Interrupts are disabled (system is in task switching state). This requirement and state makes for a less correct implementation (but easier).
// More correct implementation would make an artificial call to call_sig_handlers() and then restore the task's state again
void	call_sig_handlers(void)
{
	struct exec_task_priv   *task_priv;
	int	sig;

	task_priv = running_task->task.priv;
	if (task_priv == NULL)
		return;

	while (task_priv->pending_sigs_head != task_priv->pending_sigs_tail)
	{
		sig = task_priv->pending_signals[task_priv->pending_sigs_head];
		if (sig == SIGKILL)
{
serial_printf("%s(): SIGKILL, %08X is terminated\n", __func__, running_task);
			terminate();			// Terminate self - doesn't return
}
		if (sig < 0 || sig >= _NSIG)
			continue;
		if (task_priv->sig_handlers[sig] != NULL)
			task_priv->sig_handlers[sig](sig);

		task_priv->pending_sigs_head = (task_priv->pending_sigs_head + 1) % SIGNALS_QUEUE_SIZE;
	}
}


