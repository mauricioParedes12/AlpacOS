#include <alpacos/drivers.h>
#include <alpacos/io.h>
#include <stdbool.h>

#define COM1 0x3F8u
static bool available;

void serial_init(void)
{
    outb(COM1 + 1u, 0x00);  /* Disable UART IRQs: polling for this milestone. */
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
    if (!available || (inb(COM1 + 5u) & 1u) == 0u) {
        return -1;
    }
    return inb(COM1);
}
