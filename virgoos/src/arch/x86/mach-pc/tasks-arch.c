/*
 *	x86 architecture-specific taskman helper
 */

#include "sosdef.h"
#include "taskman.h"
#include "config.h"

//#define	DEBUG_TASK_SWITCH
//#define	DEBUG_SWITCH_TO


extern int      in_task_switch;
extern TASK_Q   *running_task;
extern TASK_Q   *task_q[NUM_PRIORITY_LEVELS][NUM_TASK_STATES];
extern int      no_time_share[NUM_PRIORITY_LEVELS];
extern dword    current_stack_top;


#ifdef	DEBUG_TASK_SWITCH
static void	dump_task_state(TASK_Q	*task)
{
	serial_printf("%s(): task=%08X\n"
		"eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X, edi=%08X esp=%08X ebp=%08X eip=%08X eflags=%08X\n\n",
		__func__, task, task->task.reg_state.eax, task->task.reg_state.ebx, task->task.reg_state.ecx, task->task.reg_state.edx,
	task->task.reg_state.esi, task->task.reg_state.edi, task->task.reg_state.esp, task->task.reg_state.ebp, task->task.reg_state.eip, task->task.reg_state.eflags);
}
#endif

/*
 *	Switches to the task pointed by pq.
 */
void	switch_to( TASK_Q *pq )
{
	dword	intfl;
#ifdef	DEBUG_SWITCH_TO
	static int cnt;
#endif

//	if (after_terminate)
#ifdef	DEBUG_SWITCH_TO
{
	serial_printf("switch_to(%08X) cnt=%d; eflags=%08X eip=%08X\n", pq, ++cnt, pq->task.reg_state.eflags, pq->task.reg_state.eip);
}
#endif

	if (!pq)
		return;

	in_task_switch = 1;
//	disable_irqs();

	running_task = pq;

	pq->task.state |= STATE_IN_SIGHANDLER;
	if (running_task->task.priv)
		call_sig_handlers();
	pq->task.state &= ~STATE_IN_SIGHANDLER;

#ifdef  DEBUG_TASK_SWITCH
		serial_printf("%s(): saved task\n", __func__);
		dump_task_state(running_task);
#endif

	if (running_task -> task.state & OPT_FP)
	{
		register	void	*p;

		p = running_task -> task.fp_state.st7;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st6;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st5;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st4;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st3;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st2;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st1;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
		p = running_task -> task.fp_state.st0;
		__asm__ __volatile__ ("fld	tbyte ptr [ %%eax ]\n" : : "a"(p) );
	}

#if 0 
{
static int ccc;
dword tmp;
	__asm__ __volatile__ ("pushad\n");
	__asm__ __volatile__ ("add %%eax, 24\n" : : "a"(&running_task -> task.reg_state));
	__asm__ __volatile__ ("nop\n" : "=a"(tmp));
printfxy(0, 23, "%s(): saved ESP [eax+24] = %08X, ccc=%d ESP from state = %08X", __func__, *(dword*)tmp, ccc++, running_task->task.reg_state.esp);
	__asm__ __volatile__ ("popad\n");
}
#endif
	//	Restore register context.
	__asm__ __volatile__ ("mov %%edi, [%%eax+20]\n" : : "a"(&running_task -> task.reg_state));	// reg_state.edi
	__asm__ __volatile__ ("mov %esi, [%eax+16]\n");	// reg_state.esi
	__asm__ __volatile__ ("mov %ebp, [%eax+28]\n");	// reg_state.ebp
	__asm__ __volatile__ ("mov %esp, [%eax+24]\n");	// reg_state.esp
	__asm__ __volatile__ ("mov %ebx, [%eax+4]\n");	// reg_state.ebx
	__asm__ __volatile__ ("mov %ecx, [%eax+8]\n");	// reg_state.ecx

	// Enable interrupts if task switch is not running in IRQ context
	__asm__ __volatile__ ("push dword ptr [%eax+32]\n");	// reg_state.eflags
	__asm__ __volatile__ ("popfd\n");		

	//__asm__ __volatile__ ("pushfd\n");
	//__asm__ __volatile__ ("and dword ptr [%esp], 0x7FFF\n");
	//__asm__ __volatile__ ("push %cs\n");
	//__asm__ __volatile__ ("push dword ptr [%eax+32]\n");    // reg_state.eflags
	//__asm__ __volatile__ ("mov %edx, [%eax+32]\n");
	//__asm__ __volatile__ ("and %edx, 0x7FFF\n");
	//__asm__ __volatile__ ("mov dword ptr [%esp-4], %edx\n");
	__asm__ __volatile__ ("push dword ptr [%eax+36]\n");	// reg_state.eip

	__asm__ __volatile__ ("mov %edx, [%eax+12]\n");	// reg_state.edx
	__asm__ __volatile__ ("mov %eax, [%eax]\n");			// reg_state.eax	
	
//	__asm__ __volatile__ ("cmp dword ptr [_running_irq], 0\n");
//	__asm__ __volatile__ ("jnz after_sti\n");
//	__asm__ __volatile__ ("or dword ptr [esp], 0x200\n");			// IF = 1
//	__asm__ __volatile__ ("sti\n");
//	__asm__ __volatile__ ("after_sti:\n");

	__asm__ __volatile__ ("mov dword ptr [_in_task_switch], 0\n");

	// Prepare for IRET
	__asm__ __volatile__ ("ret\n");
	//__asm__ __volatile__ ("iret\n");
}


/*
 *	Swithes to the task pointed by pq, saving current context if necessary.
 */
void	switch_task( TASK_Q *pq )
{
//	dword	caller_eip, caller_esp;
	void	*p;

	if ( !pq )
	{
		serial_printf("switch_task(): TASK_Q = NULL\n");
		return;
	}

	// Don't nest task switch!
	if (in_task_switch)
		return;

	in_task_switch = 1;
//	disable_irqs();

	if (running_task)
	{
		running_task -> task.reg_state.eip = *((dword*)((dword)(&pq) - 4));

		//	Save register context.
		__asm__ __volatile__ ("pushad\n"
					"pushfd\n");

		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.eflags) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.edi) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.esi) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.ebp) );
		__asm__ __volatile__ ("add %esp, 4\n");
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.ebx) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.edx) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.ecx) );
		__asm__ __volatile__ ("pop [%%eax]\n" : : "a"(&running_task -> task.reg_state.eax) );

		running_task->task.reg_state.esp = (dword)&pq;

#ifdef  DEBUG_TASK_SWITCH
		serial_printf("%s(): saved task\n", __func__);
		dump_task_state(running_task);
#endif
		if ( running_task -> task.state & OPT_FP )
		{
			p = running_task -> task.fp_state.st0;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st1;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st2;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st3;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st4;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st5;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st6;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
			p = running_task -> task.fp_state.st7;
			__asm__ __volatile__ ("fstp	tbyte ptr [%%eax]\n" : : "a"(p) );
		}
	}

	switch_to( pq );
}


/*
 *	Initialize new task structure - machine-dependent part of task's creation
 */
void	init_new_task_struct(TASK_Q *pq, TASK_ENTRY task_entry, dword param)
{
	pq->task.reg_state.esp = pq->task.stack_base + pq->task.stack_size;
#ifdef DEBUG_INIT_TASK
	{
		static int c1;
		printfxy(0, 22, "[%d] %s(): pq=%08X reg_state.esp=%08X                    ", ++c1, __func__, pq, pq->task.reg_state.esp);
	}
#endif
	
	// Inject parameter on stack
	*(dword*)(pq->task.reg_state.esp - 4) = (dword)param;
	// Inject return address of terminate() - so that if task entry returns, it goes to terminate()
	*(dword*)(pq->task.reg_state.esp - 8) = (dword)terminate;
	pq->task.reg_state.esp -= 8;
	
	pq->task.reg_state.eip = (dword)task_entry;
	pq->task.reg_state.eflags = FL_IF;
}


//---------------------------- Mutex ------------------------
void	lock_mutex(MUTEX *mutex)
{
	__asm__ __volatile__ ("push %eax\n" "push %ecx\n");
	__asm__ __volatile__ ("mov	%%eax, 1\n" : : "c"(&mutex->lock));
	__asm__ __volatile__ ("xchg	%eax, [%ecx]\n"
		"test	%eax, %eax\n"
		"jnz	mutex_taken\n");
	nap(&mutex->wait_queue);
	__asm__ __volatile__ ("\n"
"mutex_taken:\n");
}

void	unlock_mutex(MUTEX *mutex)
{
	__asm__ __volatile__ ("push %eax\n" "push %ecx\n");
	__asm__ __volatile__ ("sub %%eax, %%eax\n" : : "c"(&mutex->lock));
	__asm__ __volatile__ ("xchg	%eax, [%ecx]\n");
	wake(&mutex->wait_queue);
}
//-------------------------------------------------------------

