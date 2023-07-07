/*
 *	sosbasic.c
 *
 *	This is the source file for very basic interfaces to SeptOS.
 *
 *	TODO: change set_int_callback() interface to include parameter for IRQ. This will allow passing pointer (or just base address) to IRQ handler and having
 *	one IRQ handler for multiple identical adapters
 *	TODO: change system-defined typedefs to more self-explanatory (e.g isr -> isr_func_t)
 *	TODO: change name "sosbasic.c" (and respective arch/ names) to something more descriptive
 */


#define	SOSBASIC__C

#include "config.h"
#include "sosdef.h"
#include "taskman.h"
#include "drvint.h"
#include "inet.h"

/*
 *	callbacks is a bitmask which is set by set_int_callback() API call. If a particular bit
 *	is set, call_int_callbacks() is called from the main interrupt/exception handler. The later
 *	is responsible for calling the callbacks chains.
 */

extern	int	running_irq;

extern	TASK_Q	*task_pending;

extern	int	pending_reschedule;
extern	int	pending_reschedule_priority;

extern	int	pending_start_task;
extern	TASK_ENTRY	pending_task_entry;
extern	int	pending_task_priority;
extern	int	pending_task_options;
extern	void *pending_task_param;

uint32_t	int_callbacks, int_callbacks2, exc_callbacks;

isr	isr_proc[MAX_IRQS][MAX_IRQ_HANDLERS];
int	last_irq_proc[MAX_IRQS];

void	reschedule(int bound_priority);

// Global variable
int	errno;

#ifdef CALIBRATE_UDELAY
//volatile unsigned       calibrated_usec;
volatile unsigned	calibrated_udelay_count1 = 0;
volatile unsigned	calibrated_udelay_count2 = 0;
#else
//volatile unsigned	calibrated_usec = CALIBRATED_USEC_VAL;
volatile unsigned	calibrated_udelay_count1 = CALIBRATED_UDELAY_COUNT1;
volatile unsigned	calibrated_udelay_count2 = CALIBRATED_UDELAY_COUNT2;
#endif


/*
 *	set_int_callback( dword irq_no, isr proc )
 *
 *		irq_no - IRQ number to handle
 *		proc - address of interrupt handler
 *
 *	Returns:
 *		1 - success
 *		0 - failure (MAX_IRQ_HANDLERS is reached)
 *
 *	Sets an application-supplied IRQ handler
 */
int	set_int_callback(dword irq_no, isr proc)
{
	if (last_irq_proc[irq_no] == MAX_IRQ_HANDLERS)
		return	0;

	if (irq_no < 32)
		int_callbacks |= 1 << irq_no;
	else
		int_callbacks2 |= 1 << irq_no - 32;

	isr_proc[irq_no][last_irq_proc[irq_no]++] = proc;
	return	1;
}


/*
 *	void	call_callbacks( dword int_no )
 *
 *		int_no - interrupt number
 *
 *	Calls application-supplied IRQ handler(s)
 */
void	call_int_callbacks(dword int_no)
{
	int	i = 0;

	// Speed up processing of interrupts for which no handlers are installed.
	// (?) Is it needed? Processing is fast enough - while () loop below won't be even entered.
	// And not masking interrupts for which no handler is installed is bad enough anyway (device
	// may report an interrupt until its source condition is cleared, which will never happen, thus
	// flooding system with interrupts)
	// Production system should mask interrupts based on reverse of installed handlers mask
	//
	// (!!!) This check is finally disabled - it triggers a very funny bug. Once an interrupt for which
	// no handler is installed occurs, the EOI code below is not executed because of this "speed up".
	// This yields interrupt controller in a state of servicing that interrupt. Even worse, when an interrupt
	// from the slave 8259 occurs, cascading interrupt (IRQ 2) remains also in service, actually disabling
	// all further interrupts except IRQ 0 and 1.
//	if ((int_callbacks & 1 << int_no) == 0)
//		return;
	
	// DEBUGDEBUGDEBUG
//	if (int_no > 0)
//		serial_printf("%s(): int_no = %u int_callbacks = 0x%08X, int_callbacks2 = 0x%08X, (x) = 0x%08X\r\n", __func__, int_no, int_callbacks, int_callbacks2, int_callbacks & 1 << int_no);
	//////////////////

	
#if 1
	++running_irq;
	// (!) There was a BUG in handling shared interrupts. Following, test for valid pointer
	// is done here
	while (isr_proc[int_no][i] != NULL && i < last_irq_proc[int_no])
	{
		if (isr_proc[int_no][i++]() != 0)
			break;
	}

	--running_irq;
#endif
	// (!) Below is handling of pending reschedule and pending start task.
	// Pending start task is initiated by ISR (an interrupt "bottom-half" processing task)
	// Pending reschedule is initiated by ISR waking a higher priority task than the current.
	// An interesting situation may occur when both are active: an interrupt handler starts a bottom half and wakes up a process.
	// Normally this shouldn't occur, if ISR triggers two events then it's probably bad design (?)
	// However, it's not prohibited and should be handled properly.
	// If pending_task_priority <= pending_reschedule_priority, then reschedule() is not run (postponed). If priorities are
	// equal, it's just arbitrary to give priority to starting task over waking

	// EOI - do it here because we may invoke task switch and need interruts working in the new task
	arch_eoi(int_no);
#if 0
	// An IRQ callback handler called start_task_pending()
	if (pending_start_task)
	{
		pending_reschedule = 0;	// (!)
		pending_start_task = 0;
		start_task(pending_task_entry, pending_task_priority, pending_task_options, pending_task_param);
	}
#endif
	// An IRQ callback handler called reschedule() -- probably through wake()
//if (int_no != 0)
//serial_printf("%s(): int_no=%d pending_reschedule=%d\n", __func__, int_no, pending_reschedule);
	if (pending_reschedule)
	{
		pending_reschedule = 0;
		reschedule(pending_reschedule_priority);
	}
}


void	init_memman();
void	init_timers();
void	init_sys_time(void);
void	init_devman();
void	init_taskman();
#ifdef	POSIX_IO
void	init_io();
#ifdef	SOCKETS
void	init_sockets();
#endif
#endif
#ifdef	TCPIP
void	init_ip();
#endif
void	init_libc();


extern	void	app_entry();

/*
 *	Convenience functions for debug printouts
 */
int	serial_printf(const char *fmt, ...)
{
	int len;
	int	i;
	char	str[4096];
 	va_list	argp;

	va_start( argp, fmt );
	len = vsprintf(str, fmt, argp);
	va_end( argp );

	for ( i = 0; i < len; ++i )
		write_drv(DEV_ID(UART_DEV_ID, 0), str + i, sizeof(unsigned char));

	return len;
}

#ifdef START_APP_IN_TASK
void	init_task(void *unused)
{
	int	i;

//serial_printf("%s(): hello\n", __func__);
	start_task(idle_task, IDLE_PRIORITY_LEVEL, 0, NULL);
serial_printf("%s(): hello, idle task\n", __func__);
#ifdef SYSTEM_TASKS
	for (i = 0; i < num_sys_tasks; ++i)
	{
serial_printf("%s(): starting task %08X of priority %d\n", __func__, sys_tasks[i].entry, sys_tasks[i].priority);
		start_task(sys_tasks[i].entry, sys_tasks[i].priority, 0, NULL);
	}
#endif
	app_entry();
}
#endif


/*
 *	entry()
 *
 *	This function is used as September OS C library initialization entry point.
 *	Therefore the application entry point changes to be "void app_entry()".
 *	On entry interrupts are enabled
 *
 *	(!) order of init_xxx() OS components initializations is mostly required, since many components make assumption that another component was already loaded
 */
void	entry(void)
{
	disable_irqs();
	init_platform();
#ifdef CFG_MEMMAN
	init_memman();
#endif
#ifdef CFG_TIMERS
	init_timers();
#endif
	// taskman is now started before devman, because drivers may launch tasks (e.g. IRQ bottom-halves)
#ifdef CFG_TASKMAN
	init_taskman();
#endif
#ifdef CFG_DEVMAN
	init_devman();
#endif
	init_sys_time();
#ifdef POSIX_IO
	serial_printf("calling init_io!\n");
	init_io();
#ifdef SOCKETS
	serial_printf("calling init_sockets!\n");
	init_sockets();
#endif
#endif
#ifdef TCPIP
	serial_printf("calling init_ip!\n");
	init_ip();
#endif
#ifdef CFG_LIBC
	init_libc();
#endif

#if MASK_UNHANDLED_INTR
	plat_mask_unhandled_int();
#endif
#ifdef mips
	serial_printf("%s(): *CPU_CONFIG=%08X *PCI0_CMD=%08X\n", __func__, *(volatile uint32_t*)(GT64120_CPU_CONFIG), *(volatile uint32_t*)(GT64120_PCI0_CMD));
#endif

	serial_printf("%s(): after all init_xxx() int_callbacks=0x%08X int_callbacks2=0x%08X\n", __func__, int_callbacks, int_callbacks2);
	enable_irqs();
//for (;;)
//	;
#ifdef START_APP_IN_TASK
	// By default, start init task without round-robin option
	start_task(init_task, INIT_TASK_PRIORITY, INIT_TASK_OPTIONS, NULL);
#else
	// Call application's entry point with interrupts enabled.
	app_entry();
#endif

	// Application's entry point shouldn't return. If it does, then halt/reboot
	plat_halt();
}



#if 0
/*
 *	Functions to shut up the compiler
 */
void	_chkstk()
{
}

void	_fltused()
{
}
#endif


