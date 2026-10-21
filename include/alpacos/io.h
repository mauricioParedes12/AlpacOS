#ifndef ALPACOS_IO_H
#define ALPACOS_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static inline void cpu_relax(void)
{
    __asm__ volatile ("pause" : : : "memory");
}

_Noreturn static inline void cpu_halt(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt" : : : "memory");
    }
}

#endif
