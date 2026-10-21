#ifndef ALPACOS_CONSOLE_H
#define ALPACOS_CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_set_color(uint8_t color);
void console_putc(char c);
void console_write(const char *text);
void console_writeln(const char *text);
void console_uint(uint32_t value);
void console_hex(uint32_t value);
int console_read(void);

#endif
