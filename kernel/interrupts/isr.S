.section .text
.code32
.altmacro
.extern interrupts_dispatch

/* Algunas excepciones ya incluyen un codigo de error en la pila. */
.macro ISR number
.type isr_\number, @function
isr_\number:
    .if (\number != 8) && (\number != 10) && (\number != 11) && (\number != 12) && (\number != 13) && (\number != 14) && (\number != 17) && (\number != 21) && (\number != 29) && (\number != 30)
        pushl $0
    .endif
    pushl $\number
    jmp isr_common
.size isr_\number, . - isr_\number
.endm

.set vector, 0
.rept 256
    ISR %vector
    .set vector, vector + 1
.endr

.type isr_common, @function
isr_common:
    cld
    pushal
    xorl %eax, %eax
    movw %ds, %ax
    pushl %eax
    movw %es, %ax
    pushl %eax
    movw %fs, %ax
    pushl %eax
    movw %gs, %ax
    pushl %eax

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* C requiere ESP alineado a 16 bytes antes de CALL. */
    movl %esp, %ebx
    andl $-16, %esp
    subl $12, %esp
    pushl %ebx
    call interrupts_dispatch
    movl %ebx, %esp

    popl %eax
    movw %ax, %gs
    popl %eax
    movw %ax, %fs
    popl %eax
    movw %ax, %es
    popl %eax
    movw %ax, %ds
    popal
    addl $8, %esp
    iret
.size isr_common, . - isr_common

.section .rodata
.align 4
.global isr_stub_table
.type isr_stub_table, @object
isr_stub_table:
.macro ISR_POINTER number
    .long isr_\number
.endm
.set vector, 0
.rept 256
    ISR_POINTER %vector
    .set vector, vector + 1
.endr
.size isr_stub_table, . - isr_stub_table

.section .note.GNU-stack, "", @progbits
