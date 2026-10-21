#ifndef ALPACOS_BOOTINFO_H
#define ALPACOS_BOOTINFO_H

#include <stdbool.h>
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u

/* Only the first fields of the Multiboot v1 information structure are read. */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
} multiboot_info_prefix_t;

typedef struct {
    bool memory_available;
    uint32_t lower_kib;
    uint32_t upper_kib;
    uintptr_t kernel_start;
    uintptr_t kernel_end;
} boot_info_t;

bool bootinfo_init(uint32_t magic, const multiboot_info_prefix_t *info);
const boot_info_t *bootinfo_get(void);

#endif
