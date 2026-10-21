#include <alpacos/bootinfo.h>

extern char _kernel_start[];
extern char _kernel_end[];
static boot_info_t boot_info;

bool bootinfo_init(uint32_t magic, const multiboot_info_prefix_t *info)
{
    boot_info.kernel_start = (uintptr_t)_kernel_start;
    boot_info.kernel_end = (uintptr_t)_kernel_end;
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || info == 0) {
        return false;
    }
    if ((info->flags & 1u) != 0u) {
        boot_info.memory_available = true;
        boot_info.lower_kib = info->mem_lower;
        boot_info.upper_kib = info->mem_upper;
    }
    return true;
}

const boot_info_t *bootinfo_get(void)
{
    return &boot_info;
}
