/*
 *	SeptOS init and basic routines specific to ARM platform
 */

#include "config.h"
#include "sosdef.h"


void	init_platform(void)
{
	int	i;

	// Initialize IRQ handlers
	__asm__ __volatile__ ("b .L_relocate_exceptions\n"
				"\n"
				////////////////////// Start of exception handlers
				".L_start_reloc_exceptions:\n"
				".L_reset_exc:\n"
					"\tb .L_handle_reset\n"
				".L_ud_exc:\n"
					"\tb .L_handle_ud\n"
				".L_swi_exc:\n"
					"\tb .L_handle_swi\n"
				".L_pf_abort_exc:\n"
					"\tb .L_handle_pf_abort\n"
				".L_data_abort_exc:\n"
					"\tb .L_handle_data_abort\n"
				"\t.word 0xC0FFEE\n"
				".L_irq_exc:\n"
					"\tb .L_handle_irq\n"
				".L_fiq_exc:\n"
					"\tb .L_handle_fiq\n"
					"\n"
				".L_handle_reset:\n"
				".L_handle_ud:\n"
				".L_handle_swi:\n"
				".L_handle_pf_abort:\n"
				".L_handle_data_abort:\n"
				".L_handle_irq:\n"

					// We need to switch to SUP mode from IRQ mode so that task switch within ISR will see correct (debanked SUP-mode) r13 & r14.
					// We will also be using caller's stack.
					// There's one problem: we need to preserve SPSR and restore it later as CPSR. When changing mode to SUP we *must* disable IRQs
					// (we're not ready for nested IRQ to arrive)
					// Possible solution: (we will be using possibility to LDM {}^ (with restore CPRS from SPRS) in order to return from SUP mode to SUP mode.
					// There's nothing that prevents it, SUP mode has its SPRS (sprs_svc) - it's "exception" mode!
					// (1) Upon entry, still in IRQ mode load r13 with IRQ stack and save r14_irq (we will need it to return correctly!)
					// (2) Then, still in IRQ mode save SPSR on stack via r14_irq (it is already saved)
					// (3) Switch to SUP mode; Use r14_irq in order to copy SPRS to CPRS
					// setting "I" (IRQ-disable) bit on the way. r14_irq was already saved in step (1). Remember to switch with IRQs disabled! 
					// (caller's CPRS will be restored from saved SPRS). 
					// (4) Already in SUP mode save r0-r12 (r13 will be already holding un-banked pointer to SUP-mode stack).
					// (5) Using one of already saved regs (r0-r12), copy saved r14_irq to saved regs (to a position appropriate for loading r15 later) 
					// (6) Save r14 (it's r14_svc) and caller's SPRS (it maight have been in IRQ's SUP mode too!) on stack, 
					// as any called sub-routing would do. Now we already can be pre-empted/nested
					// (7) Do the necessary work. It may include task switching; in that case, task switch procedure is responsible for switching to proper
					// mode. We will get control back right before the next clean-up steps [...===>>>...]
					// (8) In order to return from ISR, we first peek r14 (it's r14_svc!) from stack and load SPSR from stack via one of r0-r12
					// (they will be restored in (9) -- they all were saved in step (6)
					// (9) Now everything is ready to load "ldmfd r13!, {r0-r12, r15}^" - and return from SUP mode with I=1 to SUP mode with I=0

					// Step (1)
					"\tldr	r13, .L_p_irq_stack\n"
					"\tsub r14, r14, #4\n"
					"\tstr r14, [r13, #-4]\n"
					// Step (2)
					"\tmrs r14, spsr\n"
					"\tstr r14, [r13, #-8]\n"
					// Step (3): r14 already holds SPRS
					"\tbic r14, #0x1F\n"
					"\torr r14, #0x93\n"			// SUP mode, I=1
					"\tmsr cpsr, r14\n"
					// Step (4)
					"\tstmfd r13!, {r0-r12, r14}\n"
					// Step (5)
					"\tldr r3, .L_p_irq_stack\n"
					"\tldr r2, [r3, #-4]\n"
					"\tstr r2, [r13, #52]\n"		// r14 is last offset, WORD offsets go from lower register number to higher
					"\tldr r2, [r3, #-8]\n"
					// Step (6)
					"\tstmfd r13!, {r2, r14}\n"

					// Step (7)
				".L_call_irq_loop:\n"
					"\tldr r0, .L_p_vic_base\n"
					"\tldr r1, [r0]\n"
					"\n"
					"\ttst r1, r1\n"
					"\tbeq .L_end_call_irq_loop\n"
					"\tsub r2, r1, #1\n"
					"\tmvn r2, r2\n"
					"\tand r2, r2, r1\n"
					"\tsub r3, r1, #1\n"
					"\tand r1, r1, r3\n"
					"\tsub r0, r0, r0\n"
					"\n"
				".L_irq_bit_to_num:\n"
					"\tmovs r2, r2, lsr #1\n"
					"\taddne r0, r0, #1\n"
					"\ttst r2, r2\n"
					"\tbne .L_irq_bit_to_num\n"
					"\n"
					"\tldr r14, .L_p_ret_from_call_int_callbacks\n"
					"\tldr r15, .L_p_call_int_callbacks\n"
					"\n"
				".L_p_ret_from_call_int_callbacks:\n"
					"\t.word .L_ret_from_call_int_callbacks - .L_start_reloc_exceptions"
					"\n"
				".L_ret_from_call_int_callbacks:\n"
					"\tb .L_call_irq_loop\n"
				".L_end_call_irq_loop:\n"
					// Step (8)
					"\tldmfd r13!, {r2, r14}\n"
					"\tmsr spsr, r2\n"
					// Step (9)
					"\tldmfd r13!, {r0-r12, r15}^\n"
					"\n"
				".L_p_vic_base:\n"
					"\t.word 0x10140000\n"
				".L_handle_fiq:\n"
				".L_p_call_int_callbacks:\n"
					"\t.word call_int_callbacks\n"
					"\n"
				".L_p_irq_stack:\n"
					"\t.word irq_stack_top\n"
				".L_end_reloc_exceptions:\n"
				////////////////////// End of exception handlers
				".L_reloc_exceptions_size:\n"
					"\t.word .L_end_reloc_exceptions - .L_start_reloc_exceptions\n"
				".L_p_start_reloc_exceptions:\n"
					"\t.word .L_start_reloc_exceptions\n"
					"\n"
				".L_relocate_exceptions:\n"
					"\tldr r0, .L_p_start_reloc_exceptions\n"
					"\tldr r1, .L_reloc_exceptions_size\n"
					"\n"
					"\tsub r2, r2, r2\n"
				".L_copy_exc_loop:\n"
					"\tldr r3, [r0, r2]\n"
					"\tstr r3, [r2]\n"
					"\tadd r2, r2, #4\n"
					"\tcmp r2, r1\n"
					"\tbls .L_copy_exc_loop\n"
);


	// Initialize IRQs
	// For now, all interrupts are IRQ (not FIQ), and all are non-vectored.
	// Later, some interrupts qill become FIQ and some - vectored
	outd(VIC_BASE + VICINTSELECT, 0);		// Meanwhile all interrupts are IRQ; we will make some FIQ later
	outd(VIC_BASE + VICINTENABLE, 0xFFFFFFFF);	// Enable all interrupts

	for (i = 0; i < 16; ++i)
		outd(VIC_BASE + VICVECTCNTL0 + (i<<2), 0);	// Disable vectored interrupts - meanwhile


	// Initialize some SIC interrupts
	outd(SIC_ENSET, (1<<25));		// Ethernet IRQ - enable
	outd(SIC_PICENSET, (1<<25));		// Ethernet IRQ - pass-through enable

	// That's it, timers are initialized separately
	serial_printf("%s(): VICINTSELECT=%08X(&=%08X) VICINTENABLE=%08X(&=%08X)\n", __func__, ind(VIC_BASE + VICINTSELECT), VIC_BASE + VICINTSELECT, ind(VIC_BASE + VICINTENABLE), VIC_BASE + VICINTENABLE);
}

// TODO: halt
void	plat_halt(void)
{
	for(;;)
		;
}

// ARM926 VIC doesn't have an "interrupt clear". Interrupt will remain active until cleared at the device
void	arch_eoi(int int_no)
{
}

void	plat_mask_unhandled_int(void)
{
}

