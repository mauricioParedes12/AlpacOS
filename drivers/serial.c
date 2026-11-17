#include <alpacos/drivers.h>
#include <alpacos/io.h>
#include <alpacos/interrupts.h>
#include <stdbool.h>

#define COM1 0x3F8u
static bool available, irq_enabled;
static unsigned char input_buffer[256];
static unsigned input_head, input_tail;
static input_stats_t stats;
static void serial_interrupt(void)
{
    for (unsigned n = 0; n < sizeof(input_buffer) && (inb(COM1 + 5u) & 1u) != 0u; ++n) {
        unsigned char c = inb(COM1);
        ++stats.received;
        unsigned next = (input_head + 1u) % sizeof(input_buffer);
        if (next == input_tail) ++stats.dropped;
        else { input_buffer[input_head] = c; input_head = next; }
    }
}
void serial_enable_interrupts(void)
{
    if (!available) return;
    uint32_t flags = interrupts_save();
    irq_register_handler(4u, serial_interrupt);
    irq_enabled = true;
    outb(COM1 + 1u, 1u);
    interrupts_restore(flags);
}
void serial_get_stats(input_stats_t *out)
{
    uint32_t flags = interrupts_save();
    if (out != 0) *out = stats;
    interrupts_restore(flags);
}

void serial_init(void)
{
    outb(COM1 + 1u, 0x00);  /* IRQ4 se habilita despues de instalar IDT/PIC. */
    outb(COM1 + 3u, 0x80);  /* DLAB, divisor = 1 -> 115200 baud. */
    outb(COM1, 0x01);
    outb(COM1 + 1u, 0x00);
    outb(COM1 + 3u, 0x03);  /* 8 data bits, no parity, 1 stop bit. */
    outb(COM1 + 2u, 0xC7);
    outb(COM1 + 4u, 0x1E);  /* Loopback to check presence. */
    outb(COM1, 0xAE);
    available = inb(COM1) == 0xAE;
    outb(COM1 + 4u, 0x0F);
}

void serial_putc(char c)
{
    if (!available) {
        return;
    }
    /* Bound the wait so a missing/broken UART cannot freeze the VGA shell. */
    for (unsigned i = 0; i < 100000u; ++i) {
        if ((inb(COM1 + 5u) & 0x20u) != 0u) {
            outb(COM1, (uint8_t)c);
            return;
        }
        cpu_relax();
    }
}

int serial_read(void)
{
    if (!available) return -1;
    uint32_t flags = interrupts_save();
    int c = -1;
    if (irq_enabled) {
        if (input_tail != input_head) {
            c = input_buffer[input_tail];
            input_tail = (input_tail + 1u) % sizeof(input_buffer);
        }
    } else if ((inb(COM1 + 5u) & 1u) != 0u) c = inb(COM1);
    interrupts_restore(flags);
    return c;
}
