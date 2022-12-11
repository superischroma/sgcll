main:
	pushq %rbp
	movq %rsp, %rbp
	subq 48, %rsp
	movd $3, %eax
	movd %eax, -4(%rbp)
	addq 48, %rsp
	popq %rbp
	ret
