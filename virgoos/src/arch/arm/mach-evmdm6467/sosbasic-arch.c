/*
 *	SeptemberOS init and basic routines specific to ARM evmdm6467 platform
 */

#include "config.h"
#include "sosdef.h"
#include "dm646x-usb.h"
#include "dm646x-i2c.h"
#include "drvint.h"

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
					"\tadrl r13, .L_p_irq_stack+0x10000\n"
					"\tldr r13, [r13]\n"
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
					"\tadrl r0, .L_p_aintc_base+0x10000\n"
					"\tldr r0, [r0]\n"

#if 0
///////////////////// Printout AINTC_BASE as this code sees it
					"\tmov r3, r0\n"
					"\tmov r1, #0x01000000\n"
					"\tadd r1, r1, #0x00C20000\n"

					"\tmov r4, r3, lsr #28\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #24\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #20\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #16\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #12\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #8\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #4\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tand r4, r3, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #13\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #10\n"
					"\tstr r4, [r1]\n"
/////////////////////
#endif
					"\tldr r1, [r0, #8]\n"
					"\tmvn r1, r1\n"
#if 0
///////////////////// Printout IRQ0 reg as this code sees it
					"\tmov r3, r1\n"
					"\tmov r1, #0x01000000\n"
					"\tadd r1, r1, #0x00C20000\n"

					"\tmov r4, r3, lsr #28\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #24\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #20\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #16\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #12\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #8\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #4\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tand r4, r3, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #13\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #10\n"
					"\tstr r4, [r1]\n"
					"\tldr r1, [r0, #8]\n"			// Read IRQ0 reg into r1 again
					"\tmvn r1, r1\n"
/////////////////////
#endif
					"\tsub r4, r4, r4\n"			// r4 will hold "base" interrupt number for further calculation - 0 or 32 (here 0)
					"\ttst r1, r1\n"
					"\tbne .L_do_call_irq\n"
					"\n"
					"\tldr r1, [r0, #0xC]\n"
					"\tmvn r1, r1\n"
#if 0
///////////////////// Printout IRQ1 reg as this code sees it
					"\tmov r3, r1\n"
					"\tmov r1, #0x01000000\n"
					"\tadd r1, r1, #0x00C20000\n"

					"\tmov r4, r3, lsr #28\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #24\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #20\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #16\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #12\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #8\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #4\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tand r4, r3, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #13\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #10\n"
					"\tstr r4, [r1]\n"

					"\tldr r1, [r0, #0xC]\n"			// Read IRQ1 reg into r1 again
					"\tmvn r1, r1\n"
/////////////////////
#endif
					"\tmov r4, #0x20\n"			// r4 will hold "base" interrupt number for further calculation - 0 or 32 (here 32)
					"\ttst r1, r1\n"
					"\tbeq .L_end_call_irq_loop\n"
					"\n"
				".L_do_call_irq:\n"
					"\tsub r2, r1, #1\n"
					"\tbic r2, r1, r2\n"
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
					"\tadd r0, r0, r4\n"			// Add "base" interrupt number (0 or 32) to the calculated
					"\tadrl r14, .L_p_ret_from_call_int_callbacks+0x10000\n"
					"\tldr r14, [r14]\n"

#if 0
///////////////////// Printout int number reg as this code sees it
					"\tmov r3, r0\n"
					"\tmov r1, #0x01000000\n"
					"\tadd r1, r1, #0x00C20000\n"

					"\tmov r4, r3, lsr #28\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #24\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #20\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #16\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #12\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #8\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, r3, lsr #4\n"
					"\tand r4, r4, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tand r4, r3, #0xF\n"
					"\tadd r4, r4, #'A'\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #13\n"
					"\tstr r4, [r1]\n"

					"\tmov r4, #10\n"
					"\tstr r4, [r1]\n"
/////////////////////
#endif

					"\tadrl r2, L_p_call_int_callbacks+0x10000\n"
					"\tldr r15, [r2]\n"
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
				".L_p_aintc_base:\n"
					"\t.word 0x01C48000\n"
				".L_handle_fiq:\n"
				"L_p_call_int_callbacks:\n"
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
					"\tmov r4, #0x10000\n"
				".L_copy_exc_loop:\n"
					"\tldr r3, [r0, r2]\n"
					"\tstr r3, [r4, r2]\n"
					"\tadd r2, r2, #4\n"
					"\tcmp r2, r1\n"
					"\tbls .L_copy_exc_loop\n"
);


#if 0
	// Print out settings of AINTC by u-boot. NOTE: it's illegal to call serial_printf() here, but we don't care - we rely on u-boot to have initialized UART correctly. Printout is for debug only and will be later removed
	serial_printf("u-boot's AINTC settings:\n");
	serial_printf("AINTC_BASE=%08X; [AINTCEINT0]=%08X [AINTCEINT1]=%08X [AINTCINTCTL]=%08X [AINTCINTPRI0]=%08X [AINTCINTPRI1]=%08X [AINTCINTPRI2]=%08X [AINTCINTPRI3]=%08X [AINTCINTPRI4]=%08X [AINTCINTPRI5]=%08X [AINTCINTPRI6]=%08X [AINTCINTPRI7]=%08X\n",
		AINTC_BASE, ind(AINTC_BASE + AINTCEINT0), ind(AINTC_BASE + AINTCEINT1), ind(AINTC_BASE + AINTCINTCTL), ind(AINTC_BASE + AINTCINTPRI0), ind(AINTC_BASE + AINTCINTPRI1), ind(AINTC_BASE + AINTCINTPRI2), ind(AINTC_BASE + AINTCINTPRI3), ind(AINTC_BASE + AINTCINTPRI4), ind(AINTC_BASE + AINTCINTPRI5), ind(AINTC_BASE + AINTCINTPRI6), ind(AINTC_BASE + AINTCINTPRI7));
#endif

	// Enable caching - it's hard for us to work without it. u-boots starts with all caches disabled
	{
		dword	c1;
		__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(c1));
		__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 0" : : "r"(c1 | 0x1004));		// I=1, D=1
	}


	// Initialize IRQs
	// For now, all interrupts are IRQ (not FIQ), and all are non-vectored.
	// Later, some interrupts qill become FIQ and some - vectored

	// Map all interrupts to lowest IRQ level 7, except for some interrupts like timer which we care more
	// No interrupts are mapped to priority 0 or 1 (FIQ)
	outd(AINTC_BASE + AINTCINTPRI0, 0x77772222);	// VPIF0 - VPIF3 IRQs get priority 2
	outd(AINTC_BASE + AINTCINTPRI1, 0x72277777);	// USB-related interrupts get priority 2 (for test, later we will prioritize IRQs)
	outd(AINTC_BASE + AINTCINTPRI2, 0x77777777);
	outd(AINTC_BASE + AINTCINTPRI3, 0x77772222);	// EMAC-related interrupts get priority 2 (for test!)
	outd(AINTC_BASE + AINTCINTPRI4, 0x77772222);	// Timer-related interrupts get priotiry 2
	outd(AINTC_BASE + AINTCINTPRI5, 0x77777772);	// UART0INT gets priority 2
	outd(AINTC_BASE + AINTCINTPRI6, 0x77777777);
	outd(AINTC_BASE + AINTCINTPRI7, 0x77777777);

	outd(AINTC_BASE + AINTCEINT0, 0xFFFFFFFF);	// Enable all interrupts
	outd(AINTC_BASE + AINTCEINT1, 0xFFFFFFFF);	// Enable all interrupts

#if 0 
	serial_printf("SeptOS's AINTC settings:\n");
	serial_printf("AINTC_BASE=%08X; [AINTCEINT0]=%08X [AINTCEINT1]=%08X [AINTCINTCTL]=%08X [AINTCINTPRI0]=%08X [AINTCINTPRI1]=%08X [AINTCINTPRI2]=%08X [AINTCINTPRI3]=%08X [AINTCINTPRI4]=%08X [AINTCINTPRI5]=%08X [AINTCINTPRI6]=%08X [AINTCINTPRI7]=%08X\n",
		AINTC_BASE, ind(AINTC_BASE + AINTCEINT0), ind(AINTC_BASE + AINTCEINT1), ind(AINTC_BASE + AINTCINTCTL), ind(AINTC_BASE + AINTCINTPRI0), ind(AINTC_BASE + AINTCINTPRI1), ind(AINTC_BASE + AINTCINTPRI2), ind(AINTC_BASE + AINTCINTPRI3), ind(AINTC_BASE + AINTCINTPRI4), ind(AINTC_BASE + AINTCINTPRI5), ind(AINTC_BASE + AINTCINTPRI6), ind(AINTC_BASE + AINTCINTPRI7));
#endif

	// Vectored addresses are dynamic and are offered as an option by DM646x; if we don't use them we don't need to care


	// Enable all I/O via system control module's SYSVDD3P3V_PWDN register.
	// NOTE: who wants to save power and not use all peripherals, should leave respective bits in 1s
//	serial_printf("%s() -- default -- SYSVDD3P3V_PWDN=%08X\n", __func__, ind(SYS_MODULE_BASE + SYSVDD3P3V_PWDN));
	outd(SYS_MODULE_BASE + SYSVDD3P3V_PWDN, 0);
//	serial_printf("%s() -- after setting -- SYSVDD3P3V_PWDN=%08X\n", __func__, ind(SYS_MODULE_BASE + SYSVDD3P3V_PWDN));

	// Enable video, TSIF0, TSIF1, CRGEN clocks (they are disabled by reset)
	outd(SYS_MODULE_BASE + SYSVSCLKDIS, ind(SYS_MODULE_BASE + SYSVSCLKDIS) & ~0x0F00);

	// Set desired PINMUX0/PINMUX1 values
	// NOTE: VPIF-related mux bits are set to VPIF by default
	outd(SYS_MODULE_BASE + SYSPINMUX0, ind(SYS_MODULE_BASE + SYSPINMUX0) & ~(VBUSDIS | PCIEN | HPIEN));	// USB_DRVVBUS function is selected (bit 31 = 0)
	outd(SYS_MODULE_BASE + SYSPINMUX0, ind(SYS_MODULE_BASE + SYSPINMUX0) | (ATAEN));			// Enable ATA controller

	serial_printf("%s(): started with PINMUX0:PINMUX1 configuration %08X:%08X\n", __func__, ind(SYS_MODULE_BASE + SYSPINMUX0), ind(SYS_MODULE_BASE + SYSPINMUX1));

	// Clear OSCDIS and OSCPWRDN in CLKCTL. If external clock is used, set OSCDIS
	outd(SYS_MODULE_BASE + SYSCLKCTL, ind(SYS_MODULE_BASE + SYSCLKCTL) & ~0x03000000);

	// TODO: print out clock settings in Linux driver and compare them with what we get here.

	// Linux driver includes this statement: "gpio_direction_output(2, 0);" [drivers/usb/musb/davinci.c] with comment that it "powers on VBUS" and
	// that "transceiver issues make us want to charge VBUS only when the PHY PLL is not active".
	// TODO: add appropriate code (as in Linux driver)
	//
	// reset USB controller
	outd(DM646x_USB_BASE + CTRLR, 0x1);

	// Set USBCTL (part of enabling USB PHY)
	serial_printf("%s() -- default -- SYSUSBCTL=%08X\n", __func__, ind(SYS_MODULE_BASE + SYSUSBCTL));
	outd(SYS_MODULE_BASE+SYSUSBCTL, 0);				// Due to Linux driver

	// Wait for PHYPWRGD to become 1 (fully powered-up) - is my understanding correct?
	// Linux driver does this separately after writing 0 to this reg. and writes the above values separately. Does it make difference?
	serial_printf("Waiting for USBCTL.PHYPWRGD to become 1...");
	while (!(ind(SYS_MODULE_BASE+SYSUSBCTL) & 0x100))
		;
	serial_printf("done\n");
	// We assume defaults: DATAPOL=1 USBID=0 (host), PHYPLLON=0. If defaults are wrong, we will re-set them
	// Clear PHYPDWN and PHYPLLON, set VBUSVAL, following USB initialization recommendation
	//outd(SYS_MODULE_BASE+SYSUSBCTL, ind(SYS_MODULE_BASE+SYSUSBCTL) & ~0x1 | 0x00020000);
	outd(SYS_MODULE_BASE+SYSUSBCTL, ind(SYS_MODULE_BASE+SYSUSBCTL) & ~0x1 | 0x00030000);			// Due to Linux driver
	serial_printf("%s() -- after setting -- SYSUSBCTL=%08X\n", __func__, ind(SYS_MODULE_BASE + SYSUSBCTL));

	// Enable modules that need to be enabled via PSC: ATA, EMAC/MDIO, USB

	//
	// Change state of some necessary modules to enabled - 6 steps
	//
#if defined (CFG_IDE) || defined (CFG_DM646x_EMAC) || defined (CFG_DM646x_USB) || defined (CFG_DM646x_PCI) || defined (CFG_UART_16x50) || defined (CFG_GPIO) || defined (CFG_I2C) || defined (CFG_DM646x_VPIF)

	// Step 1 - wait for GOSTAT[0] to clear
	while ((ind(PSC_BASE + PSCPTSTAT)) != 0)
		;

	// Step 2 - set NEXT field in MODCTL registers for all module that need to be enabled
//	serial_printf("%s(): -- before -- IDE module ctl. in PSC: %08X, module sts.: %08X\n", __func__, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4), ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_ATA*4));
#ifdef CFG_IDE
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4, 0x103);			// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4) | 0x03);			// LRST = 1, NEXT = 3 (enable state)
#endif
//	serial_printf("%s(): -- after -- IDE module ctl. in PSC: %08X, module sts.: %08X\n", __func__, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4), ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_ATA*4));

	// EMAC is enabled by u-boot. In case of necessity it should follow the same procedure as ATA module
#ifdef CFG_DM646x_EMAC
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_EMAC_MDIO*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_EMAC_MDIO*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_EMAC_MDIO*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_DM646x_USB
	// USB 2.0 module
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_DM646x_PCI
	// PCI module - if necessary
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_PCI*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_PCI*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_PCI*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_UART_16x50
	// UART module (meanwhile is enabled by u-boot)
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_UART0*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_UART0*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_UART0*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_GPIO
	// GPIO module - if necessary
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_GPIO*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_GPIO*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_GPIO*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_I2C
	// I2C module - if necessary
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_I2C*4, 0x103);		// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_I2C*4, ind(outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_I2C*4) | 0x03);		// LRST = 1, NEXT = 3 (enable state)
#endif
#ifdef CFG_DM646x_VPIF
	// VPIF - 2 LPSCs
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_VIDEOPORT_LPSC1, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_VIDEOPORT_LPSC1) | 0x3);	// NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_VIDEOPORT_LPSC2, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_VIDEOPORT_LPSC2) | 0x3);	// NEXT = 3 (enable state)
#endif

	// Step 3 - set EMURSTIE field in MDCTL register to 1 (not all modules need state 3, refer to "DM646x ARM Subsystem Reference Guide", ch. 6.3)
//	serial_printf("%s(): -- before -- IDE module ctl. in PSC: %08X, module sts.: %08X\n", __func__, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4), ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_ATA*4));
#ifdef CFG_IDE
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4, 0x303);			// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4, ind (PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4) | 0x200);			// LRST = 1, NEXT = 3 (enable state)
#endif
//	serial_printf("%s(): -- after -- IDE module ctl. in PSC: %08X, module sts.: %08X\n", __func__, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_ATA*4), ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_ATA*4));

#ifdef CFG_DM646x_USB
	// USB module needs step 3
//	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4, 0x303);			// LRST = 1, NEXT = 3 (enable state)
	outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4, ind(PSC_BASE + PSCMDCTL0 + PSC_MOD_USB20*4) | 0x200);			// LRST = 1, NEXT = 3 (enable state)
#endif

	// EMAC module needs step 3 - meanwhile let u-boot do it
	//outd(PSC_BASE + PSCMDCTL0 + PSC_MOD_EMAC_MDIO*4, 0x303);			// LRST = 1, NEXT = 3 (enable state)

	// Step 4 - set GO[0] bit in PTCMD to initiate transition
	outd(PSC_BASE + PSCPTCMD, 0x1);

	// Step 5 - wait for GOSTAT[0] to clear
	while (((ind(PSC_BASE + PSCPTSTAT)) & 0x1) != 0)
		;

	// Step 6 - wait for MDSTAT STATE field to change to "enabled"
#ifdef CFG_IDE
	while ((ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_ATA*4) & 0x3F) != 0x3)
		;
	serial_printf("%s(): ATA module enabled\n", __func__);
#endif
#ifdef CFG_DM646x_USB
	while ((ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_USB20*4) & 0x3F) != 0x3)
		;
	serial_printf("%s(): USB2.0 module enabled\n", __func__);
#endif
#ifdef CFG_DM646x_I2C
	while ((ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_I2C*4) & 0x3F) != 0x3)
		;
	serial_printf("%s(): I2C module enabled\n", __func__);
#endif
#ifdef CFG_DM646x_VPIF
	while ((ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_VIDEOPORT_LPSC1*4) & 0x3F) != 0x3)
		;
	serial_printf("%s(): VPIF module LPSC1 enabled\n", __func__);
	while ((ind(PSC_BASE + PSCMDSTAT0 + PSC_MOD_VIDEOPORT_LPSC2*4) & 0x3F) != 0x3)
		;
	serial_printf("%s(): VPIF module LPSC2 enabled\n", __func__);
#endif
#endif // any of modules

	// Perform various platform-specific device initialization. May be this will be moved later to respective device drivers initialization

	//
	// ATA - 5 steps of initialization for PIO mode 0
	//
#ifdef CFG_IDE
	// Step 1 - identify IDE clock
	serial_printf("%s(): SYSCLK4 divider -- before -- (PLLDIV4) is 0x%08X\n", __func__, ind(PLL1_BASE + PLLDIV4));
	// Set IDE clock to 2 * 33 MHz
	outd(PLL1_BASE + PLLDIV4, 0x00008008);
	serial_printf("%s(): SYSCLK4 divider -- after -- (PLLDIV4) is 0x%08X\n", __func__, ind(PLL1_BASE + PLLDIV4));

#if 0
	// Step 2 - timing and mode 0 access for 8-bit (control) registers
	serial_printf("%s(): -- before step 2 -- IDEMISCCTL=%08X IDEREGSTB=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEREGSTB));
	outd(IDE_MODULE_BASE+IDEREGSTB, 0x0101);			// 8 clocks for both master and slave - we're not stingy
	outd(IDE_MODULE_BASE+IDEREGRCVR, 0x0101);			// 8 clocks for both master and slave - we're not stingy
	outd(IDE_MODULE_BASE+IDEMISCCTL, 0x1);			// TIMORIDE = 1
	serial_printf("%s(): -- after step 2 -- IDEMISCCTL=%08X IDEREGSTB=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEREGSTB));

	// Step 3 - timing and mode 0 access for 16-bit (data) registers
	serial_printf("%s(): -- before step 3 -- IDEMISCCTL=%08X IDEDATSTB=%08X IDEDATRCVR=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEDATSTB), ind(IDE_MODULE_BASE+IDEDATRCVR));
	outd(IDE_MODULE_BASE+IDEDATSTB, 0x0101);			// 8 clocks for both master and slave - we're not stingy
	outd(IDE_MODULE_BASE+IDEDATRCVR, 0x0101);			// 8 clocks for both master and slave - we're not stingy
	outd(IDE_MODULE_BASE+IDEMISCCTL, 0x1);			// TIMORIDE = 1
	serial_printf("%s(): -- after step 3 -- IDEMISCCTL=%08X IDEDATSTB=%08X IDEDATRCVR=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEDATSTB), ind(IDE_MODULE_BASE+IDEDATRCVR));

	// Step 4 - override other timings
	serial_printf("%s(): -- before step 4 -- IDEMISCCTL=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL));
	outd(IDE_MODULE_BASE+IDEMISCCTL, 0x00110001);			// TIMORIDE = 1
	serial_printf("%s(): -- after step 4 -- IDEMISCCTL=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL));

	// Step 5 - enable IDE device access
	serial_printf("%s(): -- before step 5 -- IDETIMP=%08X\n", __func__, inw(IDE_MODULE_BASE+IDETIMP));
	outw(IDE_MODULE_BASE+IDETIMP, 0x8000);			// IDEEN = 1
	serial_printf("%s(): -- after step 5 -- IDETIMP=%08X\n", __func__, inw(IDE_MODULE_BASE+IDETIMP));
#endif

	// TODO: print out timing settings in palm_bk3710_init() [drivers/ide/davinci/palm_bk3710.c], and if they are not the same as in SeptemberOS,
	// implement the sequence
	// .....
	// Instead of the above, below initialization sequence is taken from Linux'es palm_bk3710_chipinit() [drivers/ide/davinci/palm_bk3710.c]
	outd(IDE_MODULE_BASE + IDEMISCCTL, 0x0300);
	udelay(100000);
	outd(IDE_MODULE_BASE + IDEMISCCTL, 0x0200);
	outw(IDE_MODULE_BASE + IDETIMP, 0xB388);
	outb(IDE_MODULE_BASE + IDETIMP + 4, 0);			// SIDETIM register, which doesn't appear in the doc
	outw(IDE_MODULE_BASE + IDEUDMACTL, 0);
	outd(IDE_MODULE_BASE + IDEMISCCTL, 0x0201);
	outd(IDE_MODULE_BASE + IDEIORDYTMP, 0xFFFF);
	outw(IDE_MODULE_BASE + IDEBMISP, 0);

	serial_printf("%s(): -- after step 2 -- IDEMISCCTL=%08X IDEREGSTB=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEREGSTB));
	serial_printf("%s(): -- after step 3 -- IDEMISCCTL=%08X IDEDATSTB=%08X IDEDATRCVR=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL), ind(IDE_MODULE_BASE+IDEDATSTB), ind(IDE_MODULE_BASE+IDEDATRCVR));
	serial_printf("%s(): -- after step 4 -- IDEMISCCTL=%08X\n", __func__, ind(IDE_MODULE_BASE+IDEMISCCTL));
	serial_printf("%s(): -- after step 5 -- IDETIMP=%08X\n", __func__, inw(IDE_MODULE_BASE+IDETIMP));

	// TODO: implement sequence from palm_bk3710_setpiomode() [drivers/ide/davinci/palm_bk3710.c] to set PIO mode 0
	// Print out values in Linux and in SeptOS and make sure that they are the same
	{
		unsigned	cycle_time, ide_t0, ide_t2, ide_t2i;
		unsigned	ide_clk;
		int	is_slave = 0;	// Later...

		ide_clk = 0xB;		// Read from clock divider register?
		cycle_time = 600;	// PIO timing cycle time for mode 0

		/* From Linux driver -- PIO Data Setup */
		ide_t0 = cycle_time / ide_clk;
		ide_t2 = 165 /* PIO timing active time for mode 0 */  / ide_clk;
		ide_t2i = ide_t0 - ide_t2 + 1;
		++ide_t2;

		if (!is_slave)
		{
			outd(IDE_MODULE_BASE + IDEDATSTB, ind(IDE_MODULE_BASE + IDEDATSTB) & 0xFF00);
			outd(IDE_MODULE_BASE + IDEDATSTB, ind(IDE_MODULE_BASE + IDEDATSTB) | ide_t2);
			outd(IDE_MODULE_BASE + IDEDATRCVR, ind(IDE_MODULE_BASE + IDEDATRCVR) & 0xFF00);
			outd(IDE_MODULE_BASE + IDEDATRCVR, ind(IDE_MODULE_BASE + IDEDATRCVR) | ide_t2i);
		}
		else
		{
			outd(IDE_MODULE_BASE + IDEDATSTB, ind(IDE_MODULE_BASE + IDEDATSTB) & 0xFF);
			outd(IDE_MODULE_BASE + IDEDATSTB, ind(IDE_MODULE_BASE + IDEDATSTB) | ide_t2 << 8);
			outd(IDE_MODULE_BASE + IDEDATRCVR, ind(IDE_MODULE_BASE + IDEDATRCVR) & 0xFF);
			outd(IDE_MODULE_BASE + IDEDATRCVR, ind(IDE_MODULE_BASE + IDEDATRCVR) | ide_t2i << 8);
		}


		/* From Linux driver -- TASKFILE Setup */
		ide_t2 = 290 /* Task timing active time for mode 0 */  / ide_clk;
		ide_t2i = ide_t0 - ide_t2 + 1;
		ide_t2++;

		if (!is_slave)
		{
			outd(IDE_MODULE_BASE + IDEREGSTB, ind(IDE_MODULE_BASE + IDEREGSTB) & 0xFF00);
			outd(IDE_MODULE_BASE + IDEREGSTB, ind(IDE_MODULE_BASE + IDEREGSTB) | ide_t2);
			outd(IDE_MODULE_BASE + IDEREGRCVR, ind(IDE_MODULE_BASE + IDEREGRCVR) & 0xFF00);
			outd(IDE_MODULE_BASE + IDEREGRCVR, ind(IDE_MODULE_BASE + IDEREGRCVR) | ide_t2i);
		}
		else
		{
			outd(IDE_MODULE_BASE + IDEREGSTB, ind(IDE_MODULE_BASE + IDEREGSTB) & 0xFF);
			outd(IDE_MODULE_BASE + IDEREGSTB, ind(IDE_MODULE_BASE + IDEREGSTB) | ide_t2 << 8);
			outd(IDE_MODULE_BASE + IDEREGRCVR, ind(IDE_MODULE_BASE + IDEREGRCVR) & 0xFF);
			outd(IDE_MODULE_BASE + IDEREGRCVR, ind(IDE_MODULE_BASE + IDEREGRCVR) | ide_t2i << 8);
		}
	}
#endif // CFG_IDE

}

// TODO: halt
void	plat_halt(void)
{
	for(;;)
		;
}

// DM6467's AINTC acknowledges interrupt by writing its positive mask (1 for interrupt number position) into IRQ0/IRQ1 regs
void	arch_eoi(int int_no)
{
	if (int_no < 32)
		outd(AINTC_BASE+AINTCIRQ0, 1<<int_no);
	else
		outd(AINTC_BASE+AINTCIRQ1, 1<<int_no-32);
}

int	printfxy( int x, int y, const char *fmt, ... )
{
}

int	cprintfxy( int color, int x, int y, const char *fmt, ... )
{
}

static void	transit_module_state(int module_id, unsigned next_state)
{
	// Step 1 - wait for GOSTAT[0] to clear
	while ((ind(PSC_BASE + PSCPTSTAT)) != 0)
		;

	// Step 2 - set NEXT field in module's MODCTL
	outd(PSC_BASE + PSCMDCTL0 + module_id*4, next_state);			// LRST = 0 (asserted), NEXT = 3 (SwRstDisable state)

	// Step 3 - set GO[0] bit in PTCMD to initiate transition
	outd(PSC_BASE + PSCPTCMD, 0x1);

	// Step 4 - wait for GOSTAT[0] to clear
	while ((ind(PSC_BASE + PSCPTSTAT)) != 0)
		;
}


// Reset module with given module_id
// Bad parameter is detected, and ignored
void evmdm6467_reset_module(int module_id)
{
	if (module_id < 0 || module_id >= PSC_NUM_MODULES)
		return;

	transit_module_state(module_id, 0);
}

// Take module with given module_id out of reset
// Bad parameter is detected, and ignored
void evmdm6467_wake_module(int module_id)
{
	if (module_id < 0 || module_id >= PSC_NUM_MODULES)
		return;

	transit_module_state(module_id, 0x103);
}


// Only 4 LSB bits matter
int	evmdm6467_set_leds(int num)
{
	struct i2c_msg  msg;
	uint8_t	data[8];
	int	rv;

	data[0] = (num << 4) & 0xFF;
	msg.addr = LEDS_ADDR;
	msg.flags = I2C_MSG_FLAG_STOP;
	msg.len = 1;
	msg.buf = data;
	rv = ioctl_drv(DEV_ID(DM646x_I2C_DEV_ID, 0), IOCTL_DM646x_I2C_WRITE, &msg);

	return	rv;
}

///////////////////// Several board-related utility functions ////////////////////////////////


#define CPLD_BASE_ADDRESS       (0x3A)
#define CPLD_RESET_POWER_REG    (0)
#define CPLD_VIDEO_REG          (0x3B)
#define CDCE949                 (0x6C)


int set_cpld_for_tvp5147(void)
{
        int err = 0;
        uint8_t val;
	struct i2c_msg	msg;

	msg.addr = CPLD_VIDEO_REG;
	msg.flags = I2C_MSG_FLAG_STOP;
	msg.len = 1;
	msg.buf = &val;
	err = ioctl_drv(DEV_ID(DM646x_I2C_DEV_ID, 0), IOCTL_DM646x_I2C_READ, &msg);
        if(err)
                return err;
        val &= 0xEF;
	err = ioctl_drv(DEV_ID(DM646x_I2C_DEV_ID, 0), IOCTL_DM646x_I2C_READ, &msg);
        return err;
}

void	plat_mask_unhandled_int(void)
{
}

