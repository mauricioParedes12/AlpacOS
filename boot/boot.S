/* Multiboot v1. GNU assembler (AT&T), no NASM dependency. */
.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003  /* Page-align modules and request memory info. */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .rodata
.align 8
gdt_start:
.quad 0x0000000000000000     /* Null descriptor. */
.quad 0x00CF9A000000FFFF     /* Ring 0 code: base 0, limit 4 GiB. */
.quad 0x00CF92000000FFFF     /* Ring 0 data: base 0, limit 4 GiB. */
gdt_end:
gdt_descriptor:
.word gdt_end - gdt_start - 1
.long gdt_start

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.code32
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli                         /* IDT/IRQs are a later milestone. */
    cld                         /* C memory routines assume forward direction. */
    movl %eax, %esi              /* Multiboot magic, preserved across setup. */
    movl %ebx, %edi              /* Multiboot information pointer. */
    lgdt gdt_descriptor
    ljmp $0x08, $reload_segments
reload_segments:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $stack_top, %esp
    xorl %ebp, %ebp

    /* i386 ABI: ESP is 16-byte aligned just before CALL. */
    subl $8, %esp
    pushl %edi
    pushl %esi
    call kernel_main

1:  cli
    hlt
    jmp 1b
.size _start, . - _start

.section .note.GNU-stack, "", @progbits
