#ifndef ALPACOS_MEMORY_H
#define ALPACOS_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#define MEMORY_PAGE_SIZE 4096u
#define MEMORY_DIRECT_MAP_LIMIT (64u * 1024u * 1024u)
#define MEMORY_TEST_VIRTUAL_BASE 0xD0000000u
#define MEMORY_TEST_PAGE_COUNT 16u

typedef struct {
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
    uint32_t reserved_frames;
    bool paging_enabled;
} memory_stats_t;

/* Inicializa el allocator y activa paginacion con identidad para los primeros 64 MiB. */
bool memory_init(uint32_t lower_kib, uint32_t upper_kib,
                 uintptr_t kernel_end);

/* Gestion de marcos fisicos; cero indica que no hay marcos disponibles. */
uintptr_t memory_frame_alloc(void);
bool memory_frame_free(uintptr_t physical_address);

/* Reserva un marco y lo mapea en la direccion virtual indicada. */
bool memory_page_alloc(uintptr_t virtual_address, uintptr_t *physical_out);
/* Desmapea una pagina creada por memory_page_alloc y libera su marco. */
bool memory_page_free(uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_is_mapped(uintptr_t virtual_address);
void memory_get_stats(memory_stats_t *stats);

#endif
