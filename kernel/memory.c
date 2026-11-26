#include <alpacos/interrupts.h>
#include <alpacos/kstring.h>
#include <alpacos/memory.h>
#include <stddef.h>
#define ENTRIES 1024u
#define IDENTITY_TABLES 16u
#define FRAME_MAX (MEMORY_DIRECT_MAP_LIMIT / MEMORY_PAGE_SIZE)
#define PRESENT 1u
#define WRITABLE 2u
#define ADDRESS 0xFFFFF000u
/* La clase impide liberar por error una tabla como si fuera un marco suelto. */
enum { FRAME_FREE, FRAME_RESERVED, FRAME_RAW, FRAME_DIRECTORY, FRAME_TABLE, FRAME_PAGE };
static uint8_t frame_kind[FRAME_MAX];
static _Alignas(4096) uint32_t kernel_directory[ENTRIES];
static _Alignas(4096) uint32_t identity_tables[IDENTITY_TABLES][ENTRIES];
static memory_space_t active_space;
static uint32_t total_frames, used_frames, reserved_frames;
static bool initialized, uses_memory_map;

typedef struct {
    memory_space_t space; /* Cero identifica una entrada libre. */
    uintptr_t base;
    uint32_t pages, resident, faults;
    bool writable;
} demand_region_t;
static demand_region_t regions[MEMORY_MAX_REGIONS];
static uint32_t demand_faults;

static demand_region_t *region_at(memory_space_t space, uintptr_t address)
{
    for (size_t i = 0u; i < MEMORY_MAX_REGIONS; ++i) {
        demand_region_t *region = &regions[i];
        if (region->space == space && region->space != 0u && address >= region->base &&
            (address - region->base) / MEMORY_PAGE_SIZE < region->pages) return region;
    }
    return NULL;
}

static uintptr_t allocate(uint8_t kind)
{
    for (uint32_t f = 0; f < total_frames; ++f) {
        if (frame_kind[f] == FRAME_FREE) {
            frame_kind[f] = kind;
            ++used_frames;
            uintptr_t address = f * MEMORY_PAGE_SIZE;
            memset((void *)address, 0, MEMORY_PAGE_SIZE);
            return address;
        }
    }
    return 0u;
}
static bool release(uintptr_t address, uint8_t kind)
{
    if ((address & 4095u) != 0u || address >= total_frames * MEMORY_PAGE_SIZE) return false;
    uint32_t f = address / MEMORY_PAGE_SIZE;
    if (frame_kind[f] != kind) return false;
    frame_kind[f] = FRAME_FREE;
    --used_frames;
    return true;
}
static bool valid_space(memory_space_t space)
{
    return initialized && (space == (uintptr_t)kernel_directory ||
        ((space & 4095u) == 0u && space < total_frames * MEMORY_PAGE_SIZE &&
         frame_kind[space / MEMORY_PAGE_SIZE] == FRAME_DIRECTORY));
}
/* El mapa identidad se comparte y nunca se modifica desde las API publicas. */
static bool private_address(uintptr_t address)
{
    return address >= MEMORY_DIRECT_MAP_LIMIT && (address & 4095u) == 0u;
}
static void invalidate(memory_space_t space, uintptr_t address)
{
    if (space == active_space) __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}
static void mark_range(uint64_t start, uint64_t length, bool usable)
{
    uint64_t end = start + length;
    if (end < start) end = UINT64_MAX;
    if (start >= MEMORY_DIRECT_MAP_LIMIT) return;
    if (end > MEMORY_DIRECT_MAP_LIMIT) end = MEMORY_DIRECT_MAP_LIMIT;
    uint32_t first = usable ? (uint32_t)((start + 4095u) >> 12) : (uint32_t)(start >> 12);
    uint32_t last = usable ? (uint32_t)(end >> 12) : (uint32_t)((end + 4095u) >> 12);
    for (uint32_t f = first; f < last; ++f) frame_kind[f] = usable ? FRAME_FREE : FRAME_RESERVED;
    if (usable && last > total_frames) total_frames = last;
}
static bool apply_map(const boot_info_t *boot, bool usable_pass)
{
    uintptr_t base = (uintptr_t)boot->memory_map;
    if (boot->memory_map_bytes > UINT32_MAX - base) return false;
    uint32_t offset = 0;
    while (offset < boot->memory_map_bytes) {
        if (boot->memory_map_bytes - offset < 4u) return false;
        const multiboot_memory_entry_t *entry = (const void *)(base + offset);
        if (entry->size < 20u || entry->size > boot->memory_map_bytes - offset - 4u) return false;
        if ((entry->type == 1u) == usable_pass) mark_range(entry->address, entry->length, usable_pass);
        offset += entry->size + 4u;
    }
    return true;
}
bool memory_init(const boot_info_t *boot)
{
    if (initialized || boot == NULL) return false;
    memset(frame_kind, FRAME_RESERVED, sizeof(frame_kind));
    total_frames = 0;
    uses_memory_map = boot->has_memory_map;
    if (uses_memory_map) {
        if (!apply_map(boot, true) || !apply_map(boot, false)) return false;
    } else if (boot->memory_available) {
        mark_range(0u, (uint64_t)boot->lower_kib << 10, true);
        /* mem_upper comienza en 1 MiB, no en el final de mem_lower. */
        mark_range(0x100000u, (uint64_t)boot->upper_kib << 10, true);
    } else return false;
    mark_range(0, boot->kernel_end > 0x400000u ? boot->kernel_end : 0x400000u, false);
    mark_range(boot->information_address, sizeof(multiboot_info_prefix_t), false);
    if (boot->has_memory_map) mark_range((uintptr_t)boot->memory_map, boot->memory_map_bytes, false);
    mark_range((uintptr_t)boot->modules, (uint64_t)boot->module_count * sizeof(multiboot_module_t), false);
    for (uint32_t i = 0; i < boot->module_count; ++i) {
        if (boot->modules[i].end < boot->modules[i].start) return false;
        mark_range(boot->modules[i].start, boot->modules[i].end - boot->modules[i].start, false);
    }
    reserved_frames = 0;
    for (uint32_t f = 0; f < total_frames; ++f) if (frame_kind[f] == FRAME_RESERVED) ++reserved_frames;
    used_frames = reserved_frames;
    if (used_frames >= total_frames) return false;
    memset(kernel_directory, 0, sizeof(kernel_directory));
    for (uint32_t t = 0; t < IDENTITY_TABLES; ++t) {
        for (uint32_t p = 0; p < ENTRIES; ++p) {
            /* La pagina cero queda ausente para detectar punteros NULL. */
            uint32_t physical = (t * ENTRIES + p) * MEMORY_PAGE_SIZE;
            identity_tables[t][p] = physical == 0u ? 0u : physical | PRESENT | WRITABLE;
        }
        kernel_directory[t] = (uintptr_t)identity_tables[t] | PRESENT | WRITABLE;
    }
    active_space = (uintptr_t)kernel_directory;
    __asm__ volatile ("movl %0, %%cr3" : : "r"(active_space) : "memory");
    uint32_t cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000u;
    __asm__ volatile ("movl %0, %%cr0" : : "r"(cr0) : "memory");
    initialized = true;
    return true;
}
uintptr_t memory_frame_alloc(void)
{
    uint32_t flags = interrupts_save();
    uintptr_t result = initialized ? allocate(FRAME_RAW) : 0u;
    interrupts_restore(flags);
    return result;
}
bool memory_frame_free(uintptr_t address)
{
    uint32_t flags = interrupts_save();
    bool result = initialized && release(address, FRAME_RAW);
    interrupts_restore(flags);
    return result;
}
memory_space_t memory_space_kernel(void) { return (uintptr_t)kernel_directory; }
memory_space_t memory_space_current(void) { return active_space; }
memory_space_t memory_space_create(void)
{
    uint32_t flags = interrupts_save();
    uintptr_t address = initialized ? allocate(FRAME_DIRECTORY) : 0u;
    if (address != 0u) memcpy((void *)address, kernel_directory, IDENTITY_TABLES * sizeof(uint32_t));
    interrupts_restore(flags);
    return address;
}
void memory_space_activate(memory_space_t space)
{
    if (space != active_space && valid_space(space)) {
        active_space = space;
        __asm__ volatile ("movl %0, %%cr3" : : "r"(space) : "memory");
    }
}
/* Helpers internos: espacio/direccion validados y las IRQ desactivadas. */
static bool map_page(memory_space_t space, uintptr_t address, bool writable, uintptr_t *physical_out)
{
    uint32_t *directory = (void *)space;
    uint32_t d = address >> 22, p = (address >> 12) & 1023u;
    bool new_table = (directory[d] & PRESENT) == 0u;
    uintptr_t table_address = directory[d] & ADDRESS;
    if (new_table) table_address = allocate(FRAME_TABLE);
    if (table_address == 0u) return false;
    uint32_t *table = (void *)table_address;
    if ((table[p] & PRESENT) != 0u) return false;
    uintptr_t physical = allocate(FRAME_PAGE);
    if (physical == 0u) {
        if (new_table) (void)release(table_address, FRAME_TABLE);
        return false;
    }
    directory[d] = table_address | PRESENT | WRITABLE;
    table[p] = physical | PRESENT | (writable ? WRITABLE : 0u);
    invalidate(space, address);
    *physical_out = physical;
    return true;
}
static bool unmap_page(memory_space_t space, uintptr_t address, uintptr_t *physical_out)
{
    uint32_t *directory = (void *)space;
    uint32_t d = address >> 22, p = (address >> 12) & 1023u;
    if ((directory[d] & PRESENT) == 0u) return false;
    uint32_t *table = (void *)(uintptr_t)(directory[d] & ADDRESS);
    uintptr_t physical = table[p] & ADDRESS;
    if ((table[p] & PRESENT) == 0u || !release(physical, FRAME_PAGE)) {
        return false;
    }
    table[p] = 0u;
    invalidate(space, address);
    bool empty = true;
    for (uint32_t i = 0; i < ENTRIES; ++i) if ((table[i] & PRESENT) != 0u) { empty = false; break; }
    if (empty) {
        uintptr_t frame = directory[d] & ADDRESS;
        directory[d] = 0u;
        (void)release(frame, FRAME_TABLE);
    }
    if (physical_out != NULL) *physical_out = physical;
    return true;
}
bool memory_space_alloc(memory_space_t space, uintptr_t address, uintptr_t *physical_out)
{
    uint32_t flags = interrupts_save();
    bool result = valid_space(space) && private_address(address) && physical_out != NULL &&
        region_at(space, address) == NULL && map_page(space, address, true, physical_out);
    interrupts_restore(flags);
    return result;
}
bool memory_space_free(memory_space_t space, uintptr_t address, uintptr_t *physical_out)
{
    uint32_t flags = interrupts_save();
    bool result = valid_space(space) && private_address(address) &&
        region_at(space, address) == NULL && unmap_page(space, address, physical_out);
    interrupts_restore(flags);
    return result;
}

static bool page_present(memory_space_t space, uintptr_t address)
{
    const uint32_t *directory = (const void *)space;
    uint32_t entry = directory[address >> 22];
    if ((entry & PRESENT) == 0u) return false;
    const uint32_t *table = (const void *)(uintptr_t)(entry & ADDRESS);
    return (table[(address >> 12) & 1023u] & PRESENT) != 0u;
}

bool memory_space_reserve(memory_space_t space, uintptr_t base, uint32_t pages, bool writable)
{
    uint32_t flags = interrupts_save();
    uint64_t end = (uint64_t)base + (uint64_t)pages * MEMORY_PAGE_SIZE;
    if (!valid_space(space) || space == memory_space_kernel() || !private_address(base) ||
        pages == 0u || pages > MEMORY_REGION_MAX_PAGES || end > 0x100000000ull) {
        interrupts_restore(flags); return false;
    }
    demand_region_t *available = NULL;
    for (size_t i = 0u; i < MEMORY_MAX_REGIONS; ++i) {
        demand_region_t *region = &regions[i];
        if (region->space == 0u) { if (available == NULL) available = region; continue; }
        uint64_t other_end = (uint64_t)region->base + (uint64_t)region->pages * MEMORY_PAGE_SIZE;
        if (region->space == space && (uint64_t)base < other_end && (uint64_t)region->base < end) {
            interrupts_restore(flags); return false;
        }
    }
    if (available == NULL) { interrupts_restore(flags); return false; }
    for (uint32_t i = 0u; i < pages; ++i) {
        if (page_present(space, base + i * MEMORY_PAGE_SIZE)) {
            interrupts_restore(flags); return false;
        }
    }
    *available = (demand_region_t){.space = space, .base = base, .pages = pages, .writable = writable};
    interrupts_restore(flags);
    return true;
}

bool memory_space_unreserve(memory_space_t space, uintptr_t base)
{
    uint32_t flags = interrupts_save();
    demand_region_t *region = region_at(space, base);
    if (!valid_space(space) || region == NULL || region->base != base) {
        interrupts_restore(flags); return false;
    }
    for (uint32_t i = 0u; i < region->pages; ++i) {
        uintptr_t address = base + i * MEMORY_PAGE_SIZE;
        if (page_present(space, address)) (void)unmap_page(space, address, NULL);
    }
    memset(region, 0, sizeof(*region));
    interrupts_restore(flags);
    return true;
}

bool memory_space_demand_info(memory_space_t space, memory_demand_info_t *info)
{
    uint32_t flags = interrupts_save();
    if (!valid_space(space) || info == NULL) { interrupts_restore(flags); return false; }
    memset(info, 0, sizeof(*info));
    for (size_t i = 0u; i < MEMORY_MAX_REGIONS; ++i) {
        if (regions[i].space != space) continue;
        ++info->regions;
        info->reserved_pages += regions[i].pages;
        info->resident_pages += regions[i].resident;
        info->handled_faults += regions[i].faults;
    }
    interrupts_restore(flags);
    return true;
}

memory_fault_result_t memory_handle_page_fault(uintptr_t address, uint32_t error)
{
    uint32_t flags = interrupts_save();
    demand_region_t *region = region_at(active_space, address);
    /* Solo ausencia de pagina en ring 0: error 0 (lectura) o 2 (escritura).
       P=1, U/S=1, RSVD y otros bits no son demanda anonima valida. */
    if (!valid_space(active_space) || active_space == memory_space_kernel() ||
        (error & ~2u) != 0u || region == NULL || ((error & 2u) != 0u && !region->writable) ||
        page_present(active_space, address)) {
        interrupts_restore(flags); return MEMORY_FAULT_INVALID;
    }
    uintptr_t physical;
    if (!map_page(active_space, address & ADDRESS, region->writable, &physical)) {
        interrupts_restore(flags); return MEMORY_FAULT_NO_MEMORY;
    }
    ++region->resident;
    ++region->faults;
    ++demand_faults;
    interrupts_restore(flags);
    return MEMORY_FAULT_RESOLVED;
}
bool memory_space_translate(memory_space_t space, uintptr_t address, uintptr_t *physical_out)
{
    uint32_t flags = interrupts_save();
    bool result = false;
    if (valid_space(space) && physical_out != NULL) {
        uint32_t *directory = (void *)space;
        uint32_t entry = directory[address >> 22];
        if ((entry & PRESENT) != 0u) {
            uint32_t *table = (void *)(uintptr_t)(entry & ADDRESS);
            entry = table[(address >> 12) & 1023u];
            if ((entry & PRESENT) != 0u) { *physical_out = (entry & ADDRESS) | (address & 4095u); result = true; }
        }
    }
    interrupts_restore(flags);
    return result;
}
bool memory_space_destroy(memory_space_t space)
{
    uint32_t flags = interrupts_save();
    if (!valid_space(space) || space == active_space || space == memory_space_kernel()) {
        interrupts_restore(flags); return false;
    }
    uint32_t *directory = (void *)space;
    for (uint32_t d = IDENTITY_TABLES; d < ENTRIES; ++d) {
        if ((directory[d] & PRESENT) == 0u) continue;
        uint32_t *table = (void *)(uintptr_t)(directory[d] & ADDRESS);
        for (uint32_t p = 0; p < ENTRIES; ++p) {
            if ((table[p] & PRESENT) != 0u) (void)release(table[p] & ADDRESS, FRAME_PAGE);
        }
        (void)release(directory[d] & ADDRESS, FRAME_TABLE);
    }
    bool result = release(space, FRAME_DIRECTORY);
    if (result) {
        for (size_t i = 0u; i < MEMORY_MAX_REGIONS; ++i) {
            if (regions[i].space == space) memset(&regions[i], 0, sizeof(regions[i]));
        }
    }
    interrupts_restore(flags);
    return result;
}
bool memory_page_alloc(uintptr_t address, uintptr_t *physical_out) { return memory_space_alloc(active_space, address, physical_out); }
bool memory_page_free(uintptr_t address, uintptr_t *physical_out) { return memory_space_free(active_space, address, physical_out); }
bool memory_page_is_mapped(uintptr_t address) { uintptr_t physical; return memory_space_translate(active_space, address, &physical); }
void memory_get_stats(memory_stats_t *stats)
{
    if (stats == NULL) return;
    uint32_t flags = interrupts_save(), cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    *stats = (memory_stats_t){.total_frames = total_frames, .used_frames = used_frames,
        .free_frames = total_frames - used_frames, .reserved_frames = reserved_frames,
        .paging_enabled = initialized && (cr0 & 0x80000000u) != 0u,
        .uses_memory_map = uses_memory_map, .demand_faults = demand_faults};
    for (size_t i = 0u; i < MEMORY_MAX_REGIONS; ++i) {
        stats->demand_reserved_pages += regions[i].pages;
        stats->demand_resident_pages += regions[i].resident;
    }
    interrupts_restore(flags);
}
