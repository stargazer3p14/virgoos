OUTPUT_FORMAT("elf32-littlearm")
ENTRY(septos_start)
SECTIONS
{
. = 0x80100000;
.text :
{
	obj/init.obj
	*(.text)
}
.data : { *(.data) }
.bss : { *(.bss) }
. = . + 0x1000; /* 4K for application stack (redefine as necessary) */
stack_top = .;
. = . + 0x1000;	/* 4K for IRQ stack */
irq_stack_top = .;
}

