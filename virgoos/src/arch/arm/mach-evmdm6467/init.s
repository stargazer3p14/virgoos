.global septos_start
septos_start:
	ldr	sp, =stack_top
	bl entry
	b .


