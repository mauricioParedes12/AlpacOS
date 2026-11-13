.section .text
.code32
.global context_switch
.type context_switch, @function

/* Cambio cooperativo mediante llamada C (ABI i386).
   EAX, ECX y EDX son temporales segun la ABI; el llamador guarda sus valores
   vivos. Todos los procesos usan los mismos segmentos y memoria del kernel.
   No usar esta rutina directamente como cambio preventivo desde una IRQ. */
context_switch:
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 24(%esp), %eax /* &old_stack */
    movl 28(%esp), %edx /* new_stack */
    movl %esp, (%eax)
    movl %edx, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    popfl
    ret
.size context_switch, . - context_switch

.section .note.GNU-stack, "", @progbits
