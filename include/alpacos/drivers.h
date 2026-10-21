#ifndef ALPACOS_DRIVERS_H
#define ALPACOS_DRIVERS_H

#include <stdint.h>

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t color);
void serial_init(void);
void serial_putc(char c);
int serial_read(void);     /* -1 when no character is available. */
int keyboard_read(void);   /* -1 when no printable key is available. */

#endif
