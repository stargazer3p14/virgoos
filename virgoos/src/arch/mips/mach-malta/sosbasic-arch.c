/*
 *	SeptOS init and basic routines specific to ARM platform
 */

#include "config.h"
#include "sosdef.h"


void	init_platform(void)
{
	int	i;

	// 
	// Initialize IRQ handlers
	//
	// We use similar model to ARM on MIPS: copy exception handlers to fixed vector addresses. Exception handlers entry points are position-independent.
	// All code (both exception  handlers and interrupted code) runs in kernel mode
	//
	// In order to handle IRQ we will use general exception handler (the most compatible mode) for now
	//
	// (!) MIPS assembler "reorganizes" code so that an instruction that precedes branch goes into branch delay slot (becomes the first after branch)
	// Branch delay slots are brain-dead
	//
	// (!) On MIPS we may scratch $26 and $27 without care of saving/restoring them: they are "reserved for OS kernel" and normal application or SeptOS
	// code will not use them
	//
	// (!) we need that .L_end_reloc_exceptions - .L_start_reloc_exceptions will not exceed 128 bytes <-> 32 MIPS instructions (real, pseudo-instructions
	// may take more than just one MIPS instruction each)
	// 
	// (!) We also don't need to bother saving $31 because exception return address is at cp0's EPC, and we don't allow nested interrupts
	//
	__asm__ __volatile__ ("\tnop\n"
					"\tj .L_relocate_exceptions\n"
				".L_start_reloc_exceptions:\n"
				".L_gen_exc:\n"

					// Check exception cause
					"\tmfc0	$26, $13, 0\n"
					"\tandi $27, $26, 0x7F\n"
					// Exception cause bits 2..6 == 0 -> interupt
					"\tnop\n"		// Goes to delay slot
					"\tbeq $27, $0, .L_handle_irq\n"
					//
					// Here we've got another exception -- later we'll decide what to do with it
					// 
				".L_exc_halt:\n"
					"\tnop\n"
					"\tb .L_exc_halt\n"
					
				".L_handle_irq:\n"
					// Branch to non-relocatable code
					"\tla $26, .L_report_irq\n"
					"\tnop\n"		// Goes to delay slot
					"\tjr $26\n"

				".L_end_reloc_exceptions:\n"
				////////////////////// End of relocated exception handlers

				// Non-relocated part of exception handler
				".L_report_irq:\n"
					// Everything interesting on Malta is reported via HW0
					"\tmfc0 $26, $13, 0\n"
					"\tandi $26, $26, 0x0400\n"		// Bit 10 -> HW0
					"\tnop\n"		// Goes to delay slot
					"\tbne $26, $0, .L_report_hw0\n"
					"\teret\n"		// No delay slot
				".L_report_hw0:\n"
					"\taddiu $29, -44\n"
					"\tsw $4, 0($29)\n"	// Save on stack - $4, $8 - $15, $24, $25
					"\tsw $8, 4($29)\n"
					"\tsw $9, 8($29)\n"
					"\tsw $10, 12($29)\n"
					"\tsw $11, 16($29)\n"
					"\tsw $12, 20($29)\n"
					"\tsw $13, 24($29)\n"
					"\tsw $14, 28($29)\n"
					"\tsw $15, 32($29)\n"
					"\tsw $24, 36($29)\n"
					"\tsw $25, 40($29)\n"
					// Read PCI0 IACK reg
					"\tli $8, 0xBBE00C34\n"
					"\tlw $9, 0($8)\n"
					// Now PCI0 INT is ACKed; read ISR in south bridge
					"\tli $8, 0xB8000020\n"	// Master PIC base port address
					// This or the next value for OCW3 doesn't matter
					"\tori $9, $0, 0xB\n"	// No action on bit 5, OCW3, no poll command, no SMM, RR=1, RIS=1
					"\tsb $9, 0($8)\n"	// Issue OCW3
					"\tlb $10, 0($8)\n"	// Read ISR for master 8259
					"\tli $8, 0xB80000A0\n"	// Slave PIC base port address
					"\tsb $9, 0($8)\n"	// Issue OCW3
					"\tlb $11, 0($8)\n"	// Read ISR for slave 8259
					// Isolate highest priority IRQ (LSB) for master PIC
					"\taddiu $12, $10, -1\n"
					"\txor $12, $12, $10\n"
					"\tand $12, $12, $10\n"
					"\tli $13, 4\n"
					"\tnop\n"		// Goes to delay slot
					"\tbne	$12, $13, .L_$12_has_irq\n"
					// If highest priority IRQ for master PIC is IRQ2 (cascade), then get highest priority slave IRQ and shift-left it by 8
					"\taddiu $12, $11, -1\n"
					"\txor $12, $12, $11\n"
					"\tand $12, $12, $11\n"
					"\tsll $12, $12, 8\n"
					".L_$12_has_irq:\n"
					// Convert IRQ mask to IRQ number
					"\tor $4, $0, $0\n"	// $4 = 0
					".L_irq_mask_to_num:\n"
					"\tsrl $12, $12, 1\n"
					"\nnop\n"		// Goes to delay slot
					"\tbeq $12, $0, .L_$4_has_irq_num\n"
					"\taddiu $4, $4, 1\n"	// Goes to delay slot - rare occasion when there's something useful
					"\tb .L_irq_mask_to_num\n"
					".L_$4_has_irq_num:\n"
					"\tnop\n"		// Goes to delay slot
					"\tjal call_int_callbacks\n"
					"\tlw $4, 0($29)\n"
					"\tlw $8, 4($29)\n"
					"\tlw $9, 8($29)\n"
					"\tlw $10, 12($29)\n"
					"\tlw $11, 16($29)\n"
					"\tlw $12, 20($29)\n"
					"\tlw $13, 24($29)\n"
					"\tlw $14, 28($29)\n"
					"\tlw $15, 32($29)\n"
					"\tlw $24, 36($29)\n"
					"\tlw $25, 40($29)\n"
					"\taddiu $29, 44\n"
					"\teret\n"
					
				////////////////////// End of non-relocated part of exception handler

				".L_relocate_exceptions:\n"
					"\tla $8, .L_start_reloc_exceptions\n"
					"\tli $9, .L_end_reloc_exceptions - .L_start_reloc_exceptions\n"
					"\tli $10, 0x80000180\n"
				".L_copy_exc_loop:\n"
					"\tlw $11, 0($8)\n"
					"\tsw $11, 0($10)\n"
					"\taddiu $8, $8, 4\n"
					"\taddiu $9, $9, -4\n"
					"\taddiu $10,  $10, 4\n"		// This will go to branch delay slot
					"\tbne $9, $0, .L_copy_exc_loop\n");


	// TODO: init the GT64120 and configure PIIX in its PCI space

	// Done: init IRQ system with CPU and/or South Bridge


// Master 8259
	outb(PIC_MASTER_PORT, PIC_CMD_ICW1);
	outb(PIC_MASTER_MASK_PORT, PIC0_BASE_INT);
	outb(PIC_MASTER_MASK_PORT, PIC_CMD_ICW3);
	outb(PIC_MASTER_MASK_PORT, PIC_CMD_ICW4);

// Slave 8259
	outb(PIC_SLAVE_PORT, PIC_CMD_ICW1);
	outb(PIC_SLAVE_MASK_PORT, PIC1_BASE_INT);
	outb(PIC_SLAVE_MASK_PORT, PIC_CMD_SLAVE_ICW3);
	outb(PIC_SLAVE_MASK_PORT, PIC_CMD_ICW4);

// Mask all interrupts
// TODO: interrupt masks don't affect register space supposedly belonging to the PIIX; hence it's proven that the addresses DON'T belong to PIIX.
// We need to figure out whether our assumption on GT64120 addresses is correct and set up PIIX in its PCI space properly
	//outb(PIC_MASTER_MASK_PORT, 0xFF);
	//outb(PIC_SLAVE_MASK_PORT, 0xFF);

// Initialize CPU configuration and PCI command
//	*(volatile uint32_t*)(GT64120_PCI0_CMD) |= 0x1;
//	*(volatile uint32_t*)(GT64120_CPU_CONFIG) |= 0x1000;
}

// TODO: halt
void	plat_halt(void)
{
	for(;;)
		;
}

// On MIPS Malta 8259 (compatible implementation in PIIX) is used
void	arch_eoi(int int_no)
{
	// EOI to southbridge
	if (int_no >= 8)
		outb(PIC_SLAVE_PORT, PIC_CMD_EOI);
	outb(PIC_MASTER_PORT, PIC_CMD_EOI);

	// EOI to GT64120
	// TODO: figure out why timer IRQ from south bridge doesn't get stuffed by EOI. Isn't it supposed to be transparent?
	//*(volatile dword*)(GT64120_INT_CAUSE) = *(volatile dword*)(GT64120_INT_CAUSE) & ~0x3C000000;		// Write 0s to bits 26..29
	*(volatile uint32_t*)(GT64120_INT_CAUSE) = 0;		// Write 0s to all bits
}

void	plat_mask_unhandled_int(void)
{
}

