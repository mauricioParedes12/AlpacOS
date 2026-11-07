#ifndef ALPACOS_INTERRUPTS_H
#define ALPACOS_INTERRUPTS_H

#include <stdint.h>

typedef void (*irq_handler_t)(void);

void interrupts_init(void);
void irq_register_handler(unsigned irq, irq_handler_t handler);

static inline void interrupts_disable(void)
{
    __asm__ volatile ("cli" : : : "memory");
}

static inline void interrupts_enable(void)
{
    __asm__ volatile ("sti" : : : "memory");
}

static inline uint32_t interrupts_save(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void interrupts_restore(uint32_t flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

/* Revisar la entrada con IRQ deshabilitadas antes de llamar a esta funcion. */
static inline void interrupts_wait(void)
{
    __asm__ volatile ("sti; hlt" : : : "memory");
}

#endif
