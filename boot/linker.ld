OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* Read + execute. */
    data PT_LOAD FLAGS(6); /* Read + write. */
}

SECTIONS
{
    . = 1M;
    _kernel_start = .;
    .multiboot ALIGN(4) : { KEEP(*(.multiboot)) } :text
    .text ALIGN(4096) : { *(.text .text.*) } :text
    .rodata ALIGN(4096) : { *(.rodata .rodata.*) } :text
    .data ALIGN(4096) : { *(.data .data.*) } :data
    .bss ALIGN(4096) (NOLOAD) :
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    . = ALIGN(4096);
    _kernel_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
