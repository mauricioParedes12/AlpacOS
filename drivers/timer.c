#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/timer.h>

#define PIT_CLOCK_HZ 1193182u

static unsigned ticks;
static volatile uint32_t seconds;

static void timer_interrupt(void)
{
    if (++ticks == TIMER_HZ) {
        ticks = 0;
        ++seconds;
    }
}

void timer_init(void)
{
    uint16_t divisor = (PIT_CLOCK_HZ + TIMER_HZ / 2u) / TIMER_HZ;
    ticks = 0;
    seconds = 0;
    /* Canal 0, divisor bajo/alto y modo 2: una IRQ periodica. */
    outb(0x43, 0x34);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
    irq_register_handler(0, timer_interrupt);
}

uint32_t timer_uptime(void)
{
    return seconds;
}
