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
#define MEMORY_MAX_REGIONS 64u /* Compartidas entre los espacios de trabajo. */
#define MEMORY_REGION_MAX_PAGES 1024u
/* Un espacio es la direccion fisica de su directorio de paginas. */
typedef uintptr_t memory_space_t;
typedef struct {
    uint32_t total_frames, used_frames, free_frames, reserved_frames;
    bool paging_enabled, uses_memory_map;
    uint32_t demand_reserved_pages, demand_resident_pages, demand_faults;
} memory_stats_t;
typedef struct {
    uint32_t regions, reserved_pages, resident_pages, handled_faults;
} memory_demand_info_t;
typedef enum {
    MEMORY_FAULT_INVALID, MEMORY_FAULT_RESOLVED, MEMORY_FAULT_NO_MEMORY
} memory_fault_result_t;
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
/* Reserva paginas anonimas sin asignar marcos ni tablas. Solo para espacios
   de trabajo; base alineada, 1..MEMORY_REGION_MAX_PAGES, sin solapamientos.
   El primer acceso valido obtiene una pagina inicializada completamente a cero.
   writable=false permite leer, pero no escribir (tambien en ring 0 con CR0.WP). */
bool memory_space_reserve(memory_space_t space, uintptr_t base, uint32_t pages, bool writable);
/* Libera la region completa y todas sus paginas residentes. Las funciones
   alloc/free de una pagina no pueden modificar una region reservada. */
bool memory_space_unreserve(memory_space_t space, uintptr_t base);
/* Contadores de las regiones vigentes; al liberarlas dejan de contabilizarse. */
bool memory_space_demand_info(memory_space_t space, memory_demand_info_t *info);
/* Atiende #PF del espacio activo. No resuelve errores de proteccion ni
   accesos fuera de una reserva. No bloquea ni cambia de proceso. */
memory_fault_result_t memory_handle_page_fault(uintptr_t address, uint32_t error);
bool memory_page_alloc(uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_free(uintptr_t virtual_address, uintptr_t *physical_out);
bool memory_page_is_mapped(uintptr_t virtual_address);
void memory_get_stats(memory_stats_t *stats);
#endif
