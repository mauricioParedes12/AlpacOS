#include <alpacos/bootinfo.h>
#include <alpacos/kstring.h>
#include <stddef.h>
extern char _kernel_start[], _kernel_end[];
static boot_info_t boot_info;
_Static_assert(offsetof(multiboot_info_prefix_t, mmap_addr) == 48u, "Multiboot v1");
bool bootinfo_init(uint32_t magic, const multiboot_info_prefix_t *info)
{
    memset(&boot_info, 0, sizeof(boot_info));
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || info == NULL) return false;
    boot_info.kernel_start = (uintptr_t)_kernel_start;
    boot_info.kernel_end = (uintptr_t)_kernel_end;
    boot_info.information_address = (uintptr_t)info;
    if ((info->flags & 1u) != 0u) {
        boot_info.memory_available = true;
        boot_info.lower_kib = info->mem_lower;
        boot_info.upper_kib = info->mem_upper;
    }
    if ((info->flags & (1u << 6)) != 0u && info->mmap_length != 0u) {
        boot_info.has_memory_map = true;
        boot_info.memory_map = (const multiboot_memory_entry_t *)(uintptr_t)info->mmap_addr;
        boot_info.memory_map_bytes = info->mmap_length;
    }
    if ((info->flags & (1u << 3)) != 0u) {
        boot_info.module_count = info->mods_count;
        boot_info.modules = (const multiboot_module_t *)(uintptr_t)info->mods_addr;
    }
    return boot_info.memory_available || boot_info.has_memory_map;
}
const boot_info_t *bootinfo_get(void) { return &boot_info; }
