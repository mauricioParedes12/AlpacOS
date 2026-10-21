#include <alpacos/drivers.h>
#include <alpacos/io.h>

#define VGA_WIDTH 80u
#define VGA_HEIGHT 25u
static volatile uint16_t *const buffer = (volatile uint16_t *)0xB8000;
static unsigned row;
static unsigned col;
static uint8_t color = 0x07;

static uint16_t cell(char c)
{
    return (uint16_t)(uint8_t)c | (uint16_t)((uint16_t)color << 8);
}

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)pos);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));
}

static void scroll(void)
{
    if (row < VGA_HEIGHT) {
        return;
    }
    for (unsigned i = 0; i < (VGA_HEIGHT - 1u) * VGA_WIDTH; ++i) {
        buffer[i] = buffer[i + VGA_WIDTH];
    }
    for (unsigned i = (VGA_HEIGHT - 1u) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; ++i) {
        buffer[i] = cell(' ');
    }
    row = VGA_HEIGHT - 1u;
}

void vga_init(void)
{
    color = 0x07;
    vga_clear();
}

void vga_clear(void)
{
    for (unsigned i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        buffer[i] = cell(' ');
    }
    row = 0;
    col = 0;
    update_cursor();
}

void vga_set_color(uint8_t new_color)
{
    color = new_color;
}

void vga_putc(char c)
{
    if (c == '\n') {
        col = 0;
        ++row;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            --col;
        } else if (row > 0) {
            --row;
            col = VGA_WIDTH - 1u;
        }
        buffer[row * VGA_WIDTH + col] = cell(' ');
    } else {
        buffer[row * VGA_WIDTH + col] = cell(c);
        if (++col == VGA_WIDTH) {
            col = 0;
            ++row;
        }
    }
    scroll();
    update_cursor();
}
