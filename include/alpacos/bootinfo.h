#ifndef ALPACOS_BOOTINFO_H
#define ALPACOS_BOOTINFO_H
#include <stdbool.h>
#include <stdint.h>
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
/* Multiboot v1: campos hasta mmap_addr; los opcionales requieren su flag. */
typedef struct {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t symbols[4];
    uint32_t mmap_length, mmap_addr;
} multiboot_info_prefix_t;
typedef struct {
    uint32_t size;
    uint64_t address, length;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_entry_t;
typedef struct { uint32_t start, end, string, reserved; } multiboot_module_t;
typedef struct {
    bool memory_available, has_memory_map;
    uint32_t lower_kib, upper_kib;
    uintptr_t kernel_start, kernel_end, information_address;
    const multiboot_memory_entry_t *memory_map;
    uint32_t memory_map_bytes, module_count;
    const multiboot_module_t *modules;
} boot_info_t;
bool bootinfo_init(uint32_t magic, const multiboot_info_prefix_t *info);
const boot_info_t *bootinfo_get(void);
#endif
