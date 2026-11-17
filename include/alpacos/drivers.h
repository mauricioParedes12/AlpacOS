#ifndef ALPACOS_DRIVERS_H
#define ALPACOS_DRIVERS_H

#include <stdint.h>
#include <stdbool.h>

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t color);
typedef struct { uint32_t received, dropped; } input_stats_t;
void serial_init(void);
void serial_enable_interrupts(void);
void serial_get_stats(input_stats_t *stats);
void keyboard_get_stats(input_stats_t *stats);
void serial_putc(char c);
int serial_read(void);     /* -1 when no character is available. */
bool keyboard_init(void);
int keyboard_read(void);   /* -1 when no printable key is available. */

#endif
