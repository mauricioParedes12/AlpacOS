#include <alpacos/console.h>
#include <alpacos/drivers.h>
#include <alpacos/interrupts.h>

void console_init(void)
{
    vga_init();
    serial_init();
}

void console_clear(void)
{
    uint32_t flags = interrupts_save();
    vga_clear();
    const char *clear = "\033[2J\033[H";
    while (*clear != '\0') {
        serial_putc(*clear++);
    }
    interrupts_restore(flags);
}

void console_set_color(uint8_t color)
{
    vga_set_color(color);
}

void console_putc(char c)
{
    /* Proteger el cursor VGA y la salida serie frente a un cambio de tarea. */
    uint32_t flags = interrupts_save();
    vga_putc(c);
    if (c == '\n') {
        serial_putc('\r');
    }
    if (c == '\b') {
        serial_putc('\b');
        serial_putc(' ');
        serial_putc('\b');
    } else {
        serial_putc(c);
    }
    interrupts_restore(flags);
}

void console_write(const char *text)
{
    uint32_t flags = interrupts_save();
    while (*text != '\0') {
        console_putc(*text++);
    }
    interrupts_restore(flags);
}

void console_writeln(const char *text)
{
    uint32_t flags = interrupts_save();
    console_write(text);
    console_putc('\n');
    interrupts_restore(flags);
}

void console_uint(uint32_t value)
{
    uint32_t flags = interrupts_save();
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count > 0u) {
        console_putc(digits[--count]);
    }
    interrupts_restore(flags);
}

void console_hex(uint32_t value)
{
    uint32_t flags = interrupts_save();
    static const char hex[] = "0123456789ABCDEF";
    console_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        console_putc(hex[(value >> (unsigned)shift) & 0xFu]);
    }
    interrupts_restore(flags);
}

int console_read(void)
{
    int c = serial_read();
    return c >= 0 ? c : keyboard_read();
}
