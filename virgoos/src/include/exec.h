/*
 *	exec.h
 *
 *	Header file for process emulation
 *	The client code must '#define PROGRAM	xxx' in order to use present definitions
 */

#ifndef EXEC__H
 #define EXEC__H

#ifdef	PROGRAM
#define	main	PROGRAM##main
#endif // PROGRAM
#define	environ	(((struct exec_task_priv*)(running_task->task.priv))->environ)

#endif

