#include <alpacos/drivers.h>
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

int keyboard_read(void)
{
    uint8_t status = inb(0x64);
    if ((status & 1u) == 0u) {
        return -1;
    }
    uint8_t code = inb(0x60);
    if ((status & 0xE0u) != 0u) {  /* Ignore mouse, parity and timeout bytes. */
        return -1;
    }
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
