#include <alpacos/console.h>
#include <alpacos/drivers.h>

void console_init(void)
{
    vga_init();
    serial_init();
}

void console_clear(void)
{
    vga_clear();
    const char *clear = "\033[2J\033[H";
    while (*clear != '\0') {
        serial_putc(*clear++);
    }
}

void console_set_color(uint8_t color)
{
    vga_set_color(color);
}

void console_putc(char c)
{
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
}

void console_write(const char *text)
{
    while (*text != '\0') {
        console_putc(*text++);
    }
}

void console_writeln(const char *text)
{
    console_write(text);
    console_putc('\n');
}

void console_uint(uint32_t value)
{
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count > 0u) {
        console_putc(digits[--count]);
    }
}

void console_hex(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    console_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        console_putc(hex[(value >> (unsigned)shift) & 0xFu]);
    }
}

int console_read(void)
{
    int c = serial_read();
    return c >= 0 ? c : keyboard_read();
}
