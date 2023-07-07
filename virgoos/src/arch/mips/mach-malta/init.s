.global septos_start
septos_start:
	la	$sp, stack_top
	nop
	jal	entry
	nop				/* Delay slot */
	j .-4

