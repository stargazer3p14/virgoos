/*
 *	MIPS architecture-specific taskman helper
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"

//#define DEBUG_SWITCH_TO

extern int	in_task_switch;
extern TASK_Q	*running_task;
extern TASK_Q	*task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];
extern int	no_time_share[NUM_PRIORITY_LEVELS];
extern dword	current_stack_top;

/*
 *	Switches to the task pointed by pq.
 */
void	switch_to(TASK_Q *pq)
{

#ifdef	DEBUG_SWITCH_TO
	static int cnt;
#endif

#ifdef	DEBUG_SWITCH_TO
	{
		serial_printf("switch_to(%08X) cnt=%d; status=%08X pc=%08X\n", pq, ++cnt, pq->task.reg_state.status, pq->task.reg_state.pc);
	}
#endif

	if (!pq)
		return;

	in_task_switch = 1;
//	disable_irqs();

	running_task = pq;

	if (running_task->task.priv)
		call_sig_handlers();

	if (running_task->task.state & OPT_FP)
	{
	}

	// We're just hoping that GCC takes temporaries from range {$8-15, $24, $25}. TODO: check that!
	// (!) We don't bother saving temporary regs and use them for store/load tasks.
	// If task switch occurred in ISR, then they will be saved on ISR stack. Otherwise, task switch occurs as result of blocking syscall and the caller
	// doesn't expect them to be saved anyway!
	__asm__ __volatile__ ("\tlw $1, 0(%0)\n": : "r"(&running_task->task.reg_state.r1));
	__asm__ __volatile__ ("\tlw $2, 0(%0)\n": : "r"(&running_task->task.reg_state.r2));
	__asm__ __volatile__ ("\tlw $3, 0(%0)\n": : "r"(&running_task->task.reg_state.r3));
	__asm__ __volatile__ ("\tlw $4, 0(%0)\n": : "r"(&running_task->task.reg_state.r4));
	__asm__ __volatile__ ("\tlw $5, 0(%0)\n": : "r"(&running_task->task.reg_state.r5));
	__asm__ __volatile__ ("\tlw $6, 0(%0)\n": : "r"(&running_task->task.reg_state.r6));
	__asm__ __volatile__ ("\tlw $7, 0(%0)\n": : "r"(&running_task->task.reg_state.r7));
	__asm__ __volatile__ ("\tlw $16, 0(%0)\n": : "r"(&running_task->task.reg_state.r16));
	__asm__ __volatile__ ("\tlw $17, 0(%0)\n": : "r"(&running_task->task.reg_state.r17));
	__asm__ __volatile__ ("\tlw $18, 0(%0)\n": : "r"(&running_task->task.reg_state.r18));
	__asm__ __volatile__ ("\tlw $19, 0(%0)\n": : "r"(&running_task->task.reg_state.r19));
	__asm__ __volatile__ ("\tlw $20, 0(%0)\n": : "r"(&running_task->task.reg_state.r20));
	__asm__ __volatile__ ("\tlw $21, 0(%0)\n": : "r"(&running_task->task.reg_state.r21));
	__asm__ __volatile__ ("\tlw $22, 0(%0)\n": : "r"(&running_task->task.reg_state.r22));
	__asm__ __volatile__ ("\tlw $23, 0(%0)\n": : "r"(&running_task->task.reg_state.r23));
	__asm__ __volatile__ ("\tlw $28, 0(%0)\n": : "r"(&running_task->task.reg_state.r28));
	__asm__ __volatile__ ("\tlw $29, 0(%0)\n": : "r"(&running_task->task.reg_state.r29));
	__asm__ __volatile__ ("\tlw $30, 0(%0)\n": : "r"(&running_task->task.reg_state.r30));
	__asm__ __volatile__ ("\tlw $31, 0(%0)\n": : "r"(&running_task->task.reg_state.r31));
	__asm__ __volatile__ ("\tmtc0 %0, $14, 0\n" : : "r"(running_task->task.reg_state.epc));
	__asm__ __volatile__ ("\tsw $0, 0(%0)\n" : : "r"(&in_task_switch));
	__asm__ __volatile__ ("\tmtc0 %0, $12, 0\n" : : "r"(running_task->task.reg_state.status));
	__asm__ __volatile__ ("\tnop\n"			// This goes into delay slot
				"\tjr %0\n" : : "r"(running_task->task.reg_state.pc));
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

	// Don't nest task switch!
	if (in_task_switch)
		return;

	in_task_switch = 1;
//	disable_irqs();

	if (running_task->task.state & OPT_FP)
	{
	}

	if (running_task)
	{
		__asm__ __volatile__ ("\tsw $1, 0(%0)\n": : "r"(&running_task->task.reg_state.r1));
		__asm__ __volatile__ ("\tsw $2, 0(%0)\n": : "r"(&running_task->task.reg_state.r2));
		__asm__ __volatile__ ("\tsw $3, 0(%0)\n": : "r"(&running_task->task.reg_state.r3));
		__asm__ __volatile__ ("\tsw $4, 0(%0)\n": : "r"(&running_task->task.reg_state.r4));
		__asm__ __volatile__ ("\tsw $5, 0(%0)\n": : "r"(&running_task->task.reg_state.r5));
		__asm__ __volatile__ ("\tsw $6, 0(%0)\n": : "r"(&running_task->task.reg_state.r6));
		__asm__ __volatile__ ("\tsw $7, 0(%0)\n": : "r"(&running_task->task.reg_state.r7));
		__asm__ __volatile__ ("\tsw $16, 0(%0)\n": : "r"(&running_task->task.reg_state.r16));
		__asm__ __volatile__ ("\tsw $17, 0(%0)\n": : "r"(&running_task->task.reg_state.r17));
		__asm__ __volatile__ ("\tsw $18, 0(%0)\n": : "r"(&running_task->task.reg_state.r18));
		__asm__ __volatile__ ("\tsw $19, 0(%0)\n": : "r"(&running_task->task.reg_state.r19));
		__asm__ __volatile__ ("\tsw $20, 0(%0)\n": : "r"(&running_task->task.reg_state.r20));
		__asm__ __volatile__ ("\tsw $21, 0(%0)\n": : "r"(&running_task->task.reg_state.r21));
		__asm__ __volatile__ ("\tsw $22, 0(%0)\n": : "r"(&running_task->task.reg_state.r22));
		__asm__ __volatile__ ("\tsw $23, 0(%0)\n": : "r"(&running_task->task.reg_state.r23));
		__asm__ __volatile__ ("\tsw $28, 0(%0)\n": : "r"(&running_task->task.reg_state.r28));
		__asm__ __volatile__ ("\tsw $29, 0(%0)\n": : "r"(&running_task->task.reg_state.r29));
		__asm__ __volatile__ ("\tsw $30, 0(%0)\n": : "r"(&running_task->task.reg_state.r30));
		__asm__ __volatile__ ("\tsw $31, 0(%0)\n": : "r"(&running_task->task.reg_state.r31));
		__asm__ __volatile__ ("\tmfc0 %0, $14, 0\n" : "=r"(running_task->task.reg_state.epc));
		__asm__ __volatile__ ("\tmfc0 %0, $12, 0\n" : "=r"(running_task->task.reg_state.status));

		// TODO: get return PC, $29 (SP) and $30 (BP) from stack frame and save it; adjust SP and RA
		// (!) MIPS GCC calling convention is really unfriendly toward us. It appears that there's no "fixed" stack frame structure, but rather
		// during compilation of every function GCC decides how much space (N bytes, N divides by 4 on MIPS32) to reserve - according to local
		// variables, parameters and number of registers needed to save. Then, $29 (sp) is subtracted that number N, $30 (s8) becomes the frame pointer
		// $31 (ra) is saved at N-4($30) and called $30 (caller's frame pointer) is saved at N-8($30)... and then, by the end of each function GCC
		// knows what is N, and can restore the caller's values.
		//
		// For us that means that we must find that N for switch_task() by means of disassembly every time that we change *anything* (as it may affect
		// the reserved space N). Currently N = 32.
		//
		// TODO: can't we figure out a better way?
		__asm__ __volatile__ ("\tlw $26, 28($30)\n"
					"\tsw $26, 0(%0)\n": : "r"(&running_task->task.reg_state.pc));
		__asm__ __volatile__ ("\tlw $26, 24($30)\n"
					"\tsw $26, 0(%0)\n": : "r"(&running_task->task.reg_state.r30));
		__asm__ __volatile__ ("\taddiu $26, $30, 32\n"
					"sw $26, 0(%0)\n": : "r"(&running_task->task.reg_state.r29));
	}

	switch_to(pq);
}

/*
 *	Initialize new task structure - machine-dependent part of task's creation
 */
void	init_new_task_struct(TASK_Q *pq, TASK_ENTRY task_entry, dword param)
{
#define	STACK_WATERMARK	2048
	pq->task.reg_state.r29 = pq->task.stack_base + pq->task.stack_size - STACK_WATERMARK;
#ifdef DEBUG_INIT_TASK
	{
		static int c1;
		serial_printf( "[%d] %s(): pq=%08X reg_state.r29=%08X\n", ++c1, __func__, pq, pq->task.reg_state.r29);
	}
#endif
	
	// Inject parameter on stack
	pq->task.reg_state.r4 = (dword)param;			// ARM ABI defines first parameter to come in r0

	// Inject return address of terminate() - so that if task entry returns, it goes to terminate()
	pq->task.reg_state.r31 = (dword)terminate;		// Into RA
	
	pq->task.reg_state.pc = (dword)task_entry;		// Entry point
	pq->task.reg_state.status = DEF_TASK_STATUS;		// Default task status (CP0) for new task
}



//---------------------------- Mutex ------------------------
void	lock_mutex(MUTEX *mutex)
{
}

void	unlock_mutex(MUTEX *mutex)
{
}
//-------------------------------------------------------------

