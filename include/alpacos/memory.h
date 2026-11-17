#ifndef ALPACOS_MEMORY_H
#define ALPACOS_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
#include <alpacos/bootinfo.h>
#define MEMORY_PAGE_SIZE 4096u
#define MEMORY_DIRECT_MAP_LIMIT (64u * 1024u * 1024u)
#define MEMORY_PROCESS_BASE 0x40000000u
#define MEMORY_TEST_VIRTUAL_BASE 0xD0000000u
#define MEMORY_TEST_PAGE_COUNT 16u
/* Un espacio es la direccion fisica de su directorio de paginas. */
typedef uintptr_t memory_space_t;
typedef struct {
    uint32_t total_frames, used_frames, free_frames, reserved_frames;
    bool paging_enabled, uses_memory_map;
} memory_stats_t;
bool memory_init(const boot_info_t *boot);
uintptr_t memory_frame_alloc(void);
bool memory_frame_free(uintptr_t physical_address);
memory_space_t memory_space_current(void);
memory_space_t memory_space_kernel(void);
memory_space_t memory_space_create(void);
void memory_space_activate(memory_space_t space);
bool memory_space_destroy(memory_space_t space);
bool memory_space_alloc(memory_space_t space, uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_space_free(memory_space_t space, uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_space_translate(memory_space_t space, uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_alloc(uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_free(uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_is_mapped(uintptr_t virtual_address);
void memory_get_stats(memory_stats_t *stats);
#endif
