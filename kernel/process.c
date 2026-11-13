#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/process.h>

#define PROCESS_STACK_WORDS 2048u /* 8 KiB por proceso. */

typedef struct {
    process_info_t info;
    interrupt_frame_t *context;
    process_entry_t entry;
} pcb_t;

static pcb_t processes[PROCESS_MAX];
static _Alignas(16) uint32_t stacks[PROCESS_MAX - 1u][PROCESS_STACK_WORDS];
static size_t current;
static uint32_t next_pid;
static unsigned slice_ticks;

/* Cola FIFO circular. El proceso RUNNING no esta en ella.
   Solo se modifica con las interrupciones desactivadas. */
static size_t ready[PROCESS_MAX];
static size_t ready_head;
static size_t ready_count;

static void ready_push(size_t slot)
{
    ready[(ready_head + ready_count) % PROCESS_MAX] = slot;
    ++ready_count;
}

static size_t ready_pop(void)
{
    size_t slot = ready[ready_head];
    ready_head = (ready_head + 1u) % PROCESS_MAX;
    --ready_count;
    return slot;
}

static void ready_remove(size_t slot)
{
    size_t count = ready_count;
    for (size_t i = 0u; i < count; ++i) {
        size_t candidate = ready_pop();
        if (candidate != slot) ready_push(candidate);
    }
}

static _Noreturn void process_start(void)
{
    processes[current].entry();
    process_exit();
}

void process_init(void)
{
    memset(processes, 0, sizeof(processes));
    current = ready_head = ready_count = 0u;
    slice_ticks = 0u;
    next_pid = 1u;
    memcpy(processes[0].info.name, "shell", sizeof("shell"));
    processes[0].info.state = PROCESS_RUNNING;
    processes[0].info.turns = 1u;
    /* El primer IRQ guarda el contexto del shell en su pila original. */
}

uint32_t process_spawn(const char *name, process_entry_t entry)
{
    if (name == NULL || entry == NULL) return 0u;
    size_t length = 0u;
    while (length < PROCESS_NAME_CAPACITY && name[length] != '\0') ++length;
    if (length == 0u || length == PROCESS_NAME_CAPACITY) return 0u;

    uint32_t flags = interrupts_save();
    if (current != 0u || next_pid == 0u) {
        interrupts_restore(flags);
        return 0u;
    }
    size_t slot = 1u;
    while (slot < PROCESS_MAX && processes[slot].info.state != PROCESS_UNUSED &&
           processes[slot].info.state != PROCESS_TERMINATED) ++slot;
    if (slot == PROCESS_MAX) {
        interrupts_restore(flags);
        return 0u;
    }

    pcb_t *process = &processes[slot];
    memset(process, 0, sizeof(*process));
    uint32_t pid = next_pid++;
    process->info.pid = pid;
    memcpy(process->info.name, name, length + 1u);
    process->info.state = PROCESS_READY;
    process->entry = entry;

    /* Simula el marco que deja isr.S para que IRET arranque la tarea.
       En ring 0, IRET consume EIP/CS/EFLAGS y conserva la pila elegida.
       Al entrar en C, ESP + 4 queda alineado a 16 bytes. */
    uint32_t *top = stacks[slot - 1u] + PROCESS_STACK_WORDS;
    *--top = 0u; /* Retorno ficticio: process_start no retorna. */
    interrupt_frame_t *frame = (interrupt_frame_t *)
        ((unsigned char *)top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));
    frame->gs = frame->fs = frame->es = frame->ds = 0x10u;
    frame->eip = (uint32_t)(uintptr_t)process_start;
    frame->cs = 0x08u;
    frame->eflags = 0x202u; /* IF activo, DF desactivado. */
    process->context = frame;
    ready_push(slot);
    interrupts_restore(flags);
    return pid;
}

/* Se llama desde la salida del IRQ0 (tras el EOI) o desde int 0x80.
   Nunca cambia de pila en medio del manejador: devuelve la que restaurara IRET. */
interrupt_frame_t *process_schedule(interrupt_frame_t *frame, bool timer_tick)
{
    pcb_t *old = &processes[current];
    old->context = frame;
    if (timer_tick) {
        ++old->info.cpu_ticks;
        if (++slice_ticks < PROCESS_QUANTUM_TICKS) return frame;
    }
    slice_ticks = 0u;
    if (old->info.state == PROCESS_RUNNING) {
        if (ready_count == 0u) return frame;
        old->info.state = PROCESS_READY;
        ready_push(current);
    }
    /* El shell no puede terminar: siempre hay alguien que pueda continuar. */
    current = ready_pop();
    pcb_t *next = &processes[current];
    next->info.state = PROCESS_RUNNING;
    ++next->info.turns;
    return next->context;
}

_Noreturn void process_exit(void)
{
    interrupts_disable();
    if (current == 0u) cpu_halt();
    processes[current].info.state = PROCESS_TERMINATED;
    process_yield();
    cpu_halt(); /* Los procesos terminados no vuelven a la cola. */
}

bool process_kill(uint32_t pid)
{
    uint32_t flags = interrupts_save();
    if (current == 0u) {
        for (size_t i = 1u; i < PROCESS_MAX; ++i) {
            if (processes[i].info.pid == pid &&
                processes[i].info.state == PROCESS_READY) {
                ready_remove(i); /* Quitar antes de permitir reutilizar la plaza. */
                processes[i].info.state = PROCESS_TERMINATED;
                interrupts_restore(flags);
                return true;
            }
        }
    }
    interrupts_restore(flags);
    return false;
}

bool process_get_info(size_t slot, process_info_t *info)
{
    if (slot >= PROCESS_MAX || info == NULL) return false;
    uint32_t flags = interrupts_save();
    bool used = processes[slot].info.state != PROCESS_UNUSED;
    if (used) *info = processes[slot].info;
    interrupts_restore(flags);
    return used;
}

uint32_t process_current_pid(void)
{
    uint32_t flags = interrupts_save();
    uint32_t pid = processes[current].info.pid;
    interrupts_restore(flags);
    return pid;
}

uint32_t process_cpu_ticks(void)
{
    uint32_t flags = interrupts_save();
    uint32_t ticks = processes[current].info.cpu_ticks;
    interrupts_restore(flags);
    return ticks;
}
