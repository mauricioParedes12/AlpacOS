#include <alpacos/drivers.h>
#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <stdbool.h>

/* QEMU PC BIOS defaults to translated scan-code set 1, US layout. */
static const char normal[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4', [0x06]='5',
    [0x07]='6', [0x08]='7', [0x09]='8', [0x0A]='9', [0x0B]='0',
    [0x0C]='-', [0x0D]='=', [0x0E]='\b', [0x0F]='\t',
    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r', [0x14]='t',
    [0x15]='y', [0x16]='u', [0x17]='i', [0x18]='o', [0x19]='p',
    [0x1A]='[', [0x1B]=']', [0x1C]='\n',
    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f', [0x22]='g',
    [0x23]='h', [0x24]='j', [0x25]='k', [0x26]='l', [0x27]=';',
    [0x28]='\'', [0x29]='`', [0x2B]='\\',
    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',', [0x34]='.', [0x35]='/', [0x39]=' '
};
static const char shifted[128] = {
    [0x02]='!', [0x03]='@', [0x04]='#', [0x05]='$', [0x06]='%',
    [0x07]='^', [0x08]='&', [0x09]='*', [0x0A]='(', [0x0B]=')',
    [0x0C]='_', [0x0D]='+', [0x1A]='{', [0x1B]='}', [0x27]=':',
    [0x28]='"', [0x29]='~', [0x2B]='|', [0x33]='<', [0x34]='>', [0x35]='?'
};
static bool left_shift;
static bool right_shift;
static bool caps;
static unsigned skip;
static char buffer[128];
static unsigned head;
static unsigned tail;

static int decode_scancode(uint8_t code)
{
    if (skip > 0u) {
        --skip;
        return -1;
    }
    if (code == 0xE0u || code == 0xE1u) {
        skip = code == 0xE0u ? 1u : 5u;
        return -1;
    }
    bool released = (code & 0x80u) != 0u;
    code &= 0x7Fu;
    if (code == 0x2Au) {
        left_shift = !released;
        return -1;
    }
    if (code == 0x36u) {
        right_shift = !released;
        return -1;
    }
    if (released) {
        return -1;
    }
    if (code == 0x3Au) {
        caps = !caps;
        return -1;
    }
    char c = normal[code];
    bool shift = left_shift || right_shift;
    if (c >= 'a' && c <= 'z') {
        if (shift != caps) {
            c = (char)(c - 'a' + 'A');
        }
    } else if (shift && shifted[code] != '\0') {
        c = shifted[code];
    }
    return c == '\0' ? -1 : (unsigned char)c;
}

static void keyboard_interrupt(void)
{
    uint8_t status = inb(0x64);
    if ((status & 1u) == 0u) return;
    uint8_t code = inb(0x60);
    if ((status & 0xE0u) != 0u) return; /* Mouse, paridad o timeout. */
    int c = decode_scancode(code);
    if (c < 0) return;
    unsigned next = (head + 1u) % sizeof(buffer);
    if (next == tail) return; /* Buffer lleno: descartar sin bloquear la IRQ. */
    buffer[head] = (char)c;
    head = next;
}

static bool controller_ready(void)
{
    for (unsigned i = 0; i < 100000u; ++i) {
        if ((inb(0x64) & 2u) == 0u) return true;
        cpu_relax();
    }
    return false;
}

bool keyboard_init(void)
{
    head = tail = skip = 0;
    left_shift = right_shift = caps = false;
    for (unsigned i = 0; i < 256u && (inb(0x64) & 1u) != 0u; ++i) {
        (void)inb(0x60);
    }
    if (!controller_ready()) return false;
    outb(0x64, 0x20); /* Leer el byte de configuracion del controlador PS/2. */
    unsigned i;
    for (i = 0; i < 100000u; ++i) {
        if ((inb(0x64) & 1u) != 0u) break;
        cpu_relax();
    }
    if (i == 100000u) return false;
    uint8_t config = inb(0x60);
    /* IRQ1 y traduccion set 1 activas, reloj habilitado, IRQ12 desactivada. */
    config = (uint8_t)((config | 0x41u) & (uint8_t)~0x12u);
    if (!controller_ready()) return false;
    outb(0x64, 0x60);
    if (!controller_ready()) return false;
    outb(0x60, config);
    irq_register_handler(1, keyboard_interrupt);
    return true;
}

int keyboard_read(void)
{
    uint32_t flags = interrupts_save();
    int c = -1;
    if (tail != head) {
        c = (unsigned char)buffer[tail];
        tail = (tail + 1u) % sizeof(buffer);
    }
    interrupts_restore(flags);
    return c;
}
