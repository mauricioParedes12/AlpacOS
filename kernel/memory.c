#include <alpacos/interrupts.h>
#include <alpacos/kstring.h>
#include <alpacos/memory.h>
#include <stddef.h>

#define PAGE_ENTRIES 1024u
#define IDENTITY_TABLE_COUNT (MEMORY_DIRECT_MAP_LIMIT / (PAGE_ENTRIES * MEMORY_PAGE_SIZE))
#define FRAME_COUNT_MAX (MEMORY_DIRECT_MAP_LIMIT / MEMORY_PAGE_SIZE)
#define PAGE_PRESENT 0x001u
#define PAGE_WRITABLE 0x002u
#define PAGE_ADDRESS_MASK 0xFFFFF000u
#define PAGE_FLAGS_MASK 0x00000FFFu
#define MINIMUM_RESERVED_BYTES (4u * 1024u * 1024u)

_Static_assert(IDENTITY_TABLE_COUNT == 16u, "El mapa identidad debe cubrir 64 MiB");

static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(MEMORY_PAGE_SIZE)));
static uint32_t identity_tables[IDENTITY_TABLE_COUNT][PAGE_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static uintptr_t dynamic_table_frames[PAGE_ENTRIES];
static uint8_t frame_used[FRAME_COUNT_MAX];
static uint32_t total_frames;
static uint32_t used_frames;
static uint32_t reserved_frames;
static bool initialized;

static uintptr_t frame_alloc_locked(void)
{
    for (uint32_t frame = reserved_frames; frame < total_frames; ++frame) {
        if (frame_used[frame] == 0u) {
            frame_used[frame] = 1u;
            ++used_frames;
            return (uintptr_t)frame * MEMORY_PAGE_SIZE;
        }
    }
    return 0u;
}

static bool frame_free_locked(uintptr_t physical_address)
{
    if ((physical_address & (MEMORY_PAGE_SIZE - 1u)) != 0u) return false;
    uint32_t frame = (uint32_t)(physical_address / MEMORY_PAGE_SIZE);
    if (frame < reserved_frames || frame >= total_frames || frame_used[frame] == 0u) {
        return false;
    }
    frame_used[frame] = 0u;
    --used_frames;
    return true;
}

static uint32_t *page_table_locked(uintptr_t virtual_address, bool create)
{
    uint32_t directory_index = (uint32_t)(virtual_address >> 22);
    uint32_t entry = page_directory[directory_index];
    if ((entry & PAGE_PRESENT) == 0u) {
        if (!create) return NULL;
        uintptr_t table_frame = frame_alloc_locked();
        if (table_frame == 0u) return NULL;
        uint32_t *table = (uint32_t *)table_frame; /* Physical RAM is identity-mapped. */
        memset(table, 0, MEMORY_PAGE_SIZE);
        dynamic_table_frames[directory_index] = table_frame;
        page_directory[directory_index] = (uint32_t)table_frame | PAGE_PRESENT | PAGE_WRITABLE;
        entry = page_directory[directory_index];
    }
    return (uint32_t *)(uintptr_t)(entry & PAGE_ADDRESS_MASK);
}

static bool map_page_locked(uintptr_t virtual_address, uintptr_t physical_address)
{
    if (((virtual_address | physical_address) & (MEMORY_PAGE_SIZE - 1u)) != 0u ||
        physical_address >= MEMORY_DIRECT_MAP_LIMIT) return false;
    uint32_t *table = page_table_locked(virtual_address, true);
    if (table == NULL) return false;
    uint32_t table_index = (uint32_t)((virtual_address >> 12) & 0x3FFu);
    if ((table[table_index] & PAGE_PRESENT) != 0u) return false;
    table[table_index] = (uint32_t)physical_address | PAGE_PRESENT | PAGE_WRITABLE;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return true;
}

static bool unmap_page_locked(uintptr_t virtual_address, uintptr_t *physical_out)
{
    if ((virtual_address & (MEMORY_PAGE_SIZE - 1u)) != 0u ||
        virtual_address < MEMORY_DIRECT_MAP_LIMIT) return false;
    uint32_t *table = page_table_locked(virtual_address, false);
    if (table == NULL) return false;
    uint32_t table_index = (uint32_t)((virtual_address >> 12) & 0x3FFu);
    uint32_t entry = table[table_index];
    if ((entry & PAGE_PRESENT) == 0u) return false;
    table[table_index] = 0u;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");

    uint32_t directory_index = (uint32_t)(virtual_address >> 22);
    if (dynamic_table_frames[directory_index] != 0u) {
        bool empty = true;
        for (uint32_t i = 0u; i < PAGE_ENTRIES; ++i) {
            if ((table[i] & PAGE_PRESENT) != 0u) {
                empty = false;
                break;
            }
        }
        if (empty) {
            uintptr_t table_frame = dynamic_table_frames[directory_index];
            page_directory[directory_index] = 0u;
            dynamic_table_frames[directory_index] = 0u;
            (void)frame_free_locked(table_frame);
        }
    }
    if (physical_out != NULL) *physical_out = (uintptr_t)(entry & PAGE_ADDRESS_MASK);
    return true;
}

bool memory_init(uint32_t lower_kib, uint32_t upper_kib, uintptr_t kernel_end)
{
    if (initialized) return false;
    uint64_t reported_kib = (uint64_t)lower_kib + (uint64_t)upper_kib;
    uint64_t reported_bytes = reported_kib << 10;
    uint32_t frames = (uint32_t)(reported_bytes / MEMORY_PAGE_SIZE);
    if (frames > FRAME_COUNT_MAX) frames = FRAME_COUNT_MAX;
    if (frames <= (MINIMUM_RESERVED_BYTES / MEMORY_PAGE_SIZE)) return false;

    uint32_t kernel_limit = (uint32_t)kernel_end;
    if (kernel_limit < MINIMUM_RESERVED_BYTES) kernel_limit = MINIMUM_RESERVED_BYTES;
    kernel_limit = (kernel_limit + MEMORY_PAGE_SIZE - 1u) & ~(MEMORY_PAGE_SIZE - 1u);
    reserved_frames = kernel_limit / MEMORY_PAGE_SIZE;
    if (reserved_frames >= frames) return false;

    total_frames = frames;
    used_frames = reserved_frames;
    memset(frame_used, 0, sizeof(frame_used));
    for (uint32_t i = 0u; i < reserved_frames; ++i) frame_used[i] = 1u;
    memset(page_directory, 0, sizeof(page_directory));
    memset(identity_tables, 0, sizeof(identity_tables));
    memset(dynamic_table_frames, 0, sizeof(dynamic_table_frames));

    for (uint32_t table = 0u; table < IDENTITY_TABLE_COUNT; ++table) {
        for (uint32_t page = 0u; page < PAGE_ENTRIES; ++page) {
            uint32_t address = (table * PAGE_ENTRIES + page) * MEMORY_PAGE_SIZE;
            identity_tables[table][page] = address | PAGE_PRESENT | PAGE_WRITABLE;
        }
        page_directory[table] = (uint32_t)(uintptr_t)&identity_tables[table][0] |
                                PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint32_t directory_address = (uint32_t)(uintptr_t)page_directory;
    uint32_t cr0;
    __asm__ volatile ("movl %0, %%cr3" : : "r"(directory_address) : "memory");
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000u; /* PG: paging; WP: honor read-only PTEs in ring 0. */
    __asm__ volatile ("movl %0, %%cr0" : : "r"(cr0) : "memory");
    initialized = true;
    return true;
}

uintptr_t memory_frame_alloc(void)
{
    uint32_t flags = interrupts_save();
    uintptr_t result = initialized ? frame_alloc_locked() : 0u;
    interrupts_restore(flags);
    return result;
}

bool memory_frame_free(uintptr_t physical_address)
{
    uint32_t flags = interrupts_save();
    bool result = initialized && frame_free_locked(physical_address);
    interrupts_restore(flags);
    return result;
}

bool memory_page_alloc(uintptr_t virtual_address, uintptr_t *physical_out)
{
    if (!initialized || physical_out == NULL) return false;
    uint32_t flags = interrupts_save();
    uintptr_t frame = frame_alloc_locked();
    bool result = frame != 0u && map_page_locked(virtual_address, frame);
    if (!result && frame != 0u) (void)frame_free_locked(frame);
    if (result) *physical_out = frame;
    interrupts_restore(flags);
    return result;
}

bool memory_page_free(uintptr_t virtual_address, uintptr_t *physical_out)
{
    if (!initialized) return false;
    uint32_t flags = interrupts_save();
    uintptr_t frame = 0u;
    bool result = unmap_page_locked(virtual_address, &frame);
    if (result) result = frame_free_locked(frame);
    if (result && physical_out != NULL) *physical_out = frame;
    interrupts_restore(flags);
    return result;
}

bool memory_page_is_mapped(uintptr_t virtual_address)
{
    if (!initialized) return false;
    uint32_t flags = interrupts_save();
    uint32_t *table = page_table_locked(virtual_address, false);
    bool mapped = false;
    if (table != NULL) {
        uint32_t index = (uint32_t)((virtual_address >> 12) & 0x3FFu);
        mapped = (table[index] & PAGE_PRESENT) != 0u;
    }
    interrupts_restore(flags);
    return mapped;
}

void memory_get_stats(memory_stats_t *stats)
{
    if (stats == NULL) return;
    uint32_t flags = interrupts_save();
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = total_frames - used_frames;
    stats->reserved_frames = reserved_frames;
    uint32_t cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    stats->paging_enabled = initialized && (cr0 & 0x80000000u) != 0u;
    interrupts_restore(flags);
}
