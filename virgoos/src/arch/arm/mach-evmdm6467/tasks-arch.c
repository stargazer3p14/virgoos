/*
 *	ARM architecture-specific taskman helper
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"

extern int	in_task_switch;
extern TASK_Q	*running_task;
extern TASK_Q	*task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];
extern int	no_time_share[NUM_PRIORITY_LEVELS];
extern dword	current_stack_top;

//#define	DEBUG_SWITCH_TO	1
//#define	DEBUG_INIT_TASK	1

/*
 *	Switches to the task pointed by pq.
 */
void	switch_to(TASK_Q *pq)
{

#ifdef	DEBUG_SWITCH_TO
	static int cnt;
#endif

//	if (after_terminate)
#ifdef	DEBUG_SWITCH_TO
{
	serial_printf("switch_to(%08X) [from running_task=%08X] cnt=%d; cpsr=%08X r15=%08X r13=%08X\n", pq, running_task, ++cnt, pq->task.reg_state.cpsr, pq->task.reg_state.r15, pq->task.reg_state.r13);
}
#endif

	if (!pq)
		return;

	in_task_switch = 1;
//	disable_irqs();

	running_task = pq;

//	if (running_task->task.priv)
//		call_sig_handlers();

	if (running_task->task.state & OPT_FP)
	{
	}

	//	Restore register context.
	__asm__ __volatile__ ("msr cpsr_cxsf, %0\n" : : "r"(running_task->task.reg_state.cpsr));
	__asm__ __volatile__ ("mov %%r0, %0\n" : : "r"(&running_task->task.reg_state.r0));		// don't save r0 and don't care, because it will be loaded with the following
	__asm__ __volatile__ ("str %0, [%1]\n" : : "r"(0), "r"(&in_task_switch));
	__asm__ __volatile__ ("ldmia r0, {r0 - r15}\n");
}


/*
 *	Swithes to the task pointed by pq, saving current context if necessary.
 */
void	switch_task(TASK_Q *pq)
{
//	dword	caller_eip, caller_esp;
	void	*p;

	if (!pq)
	{
		serial_printf("switch_task(): TASK_Q = NULL" );
		return;
	}

#if DEBUG_SWITCH_TO
	serial_printf("%s(): running_task = %08X\n", __func__, running_task);
#endif
	// Don't nest task switch!
	if (in_task_switch)
		return;

	in_task_switch = 1;
//	disable_irqs();

	if (running_task)
	{
		__asm__ __volatile__ ("ldr %0, [%%r11]\n" : "=r" (running_task->task.reg_state.r15));

		//	Save register context.
		__asm__ __volatile__ ("str r0, [r13, #-4]\n");
		__asm__ __volatile__ ("mov %%r0, %0\n" : : "r"(&running_task->task.reg_state.r1));
		__asm__ __volatile__ ("stmia r0, {r1 - r14}\n");
//		__asm__ __volatile__ ("str %%r13, [%0]\n" : : "r"(&running_task->task.reg_state.r13));
		__asm__ __volatile__ ("ldr r0, [r13, #-4]\n"
					"str %%r0, [%0]\n" : : "r"(&running_task->task.reg_state.r0));
		__asm__ __volatile__ ("mrs r0, cpsr\n"
					"str %%r0, [%0]\n" : : "r"(&running_task->task.reg_state.cpsr));
		__asm__ __volatile__ ("add %0, %%r11, #4\n" : "=r" (running_task->task.reg_state.r13));
		__asm__ __volatile__ ("ldr %0, [%%r11, #-4]\n" : "=r" (running_task->task.reg_state.r11));
		if (running_task -> task.state & OPT_FP)
		{
		}
	}
#if DEBUG_SWITCH_TO
	serial_printf("%s(): calling switch_to()\n", __func__);
#endif
	switch_to(pq);
}

/*
 *	Initialize new task structure (machine-dependent part of task's creation
 */
void    init_new_task_struct(TASK_Q *pq, TASK_ENTRY task_entry, dword param)
{
#define	STACK_WATERMARK	2048
	pq->task.reg_state.r13 = pq->task.stack_base + pq->task.stack_size - STACK_WATERMARK;
#if DEBUG_INIT_TASK
	{
		static int c1;
		serial_printf( "[%d] %s(): pq=%08X reg_state.r13=%08X\r\n", ++c1, __func__, pq, pq->task.reg_state.r13);
	}
#endif
	
	// Inject parameter on stack
	pq->task.reg_state.r0 = (dword)param;			// ARM ABI defines first parameter to come in r0

	// Inject return address of terminate() - so that if task entry returns, it goes to terminate()
	pq->task.reg_state.r14 = (dword)terminate;		// Into LR
	
	pq->task.reg_state.r15 = (dword)task_entry;		// Entry point
	pq->task.reg_state.cpsr = CPRS_MODE_SUP;		// Flags: supervisor mode, IRQ, FIQ, ABT enabled, the rest of flags 0s
}

//---------------------------- Mutex ------------------------
void	lock_mutex(MUTEX *mutex)
{
}

void	unlock_mutex(MUTEX *mutex)
{
}
//-------------------------------------------------------------

