#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/process.h>

#define PROCESS_STACK_WORDS 2048u /* 8 KiB por proceso, reservados en el kernel. */

/* PCB: identidad, estado, punto de entrada y contexto guardado en su pila. */
typedef struct {
    process_info_t info;
    uint32_t *stack_pointer;
    process_entry_t entry;
} pcb_t;

static pcb_t processes[PROCESS_MAX];
static _Alignas(16) uint32_t stacks[PROCESS_MAX - 1u][PROCESS_STACK_WORDS];
static size_t current;
static uint32_t next_pid;

extern void context_switch(uint32_t **old_stack, uint32_t *new_stack);

static _Noreturn void process_start(void)
{
    processes[current].entry();
    process_exit(); /* Retornar de la tarea termina el proceso. */
}

void process_init(void)
{
    memset(processes, 0, sizeof(processes));
    current = 0u;
    next_pid = 1u;
    memcpy(processes[0].info.name, "shell", sizeof("shell"));
    processes[0].info.state = PROCESS_RUNNING;
    /* La pila del shell se captura en el primer context_switch. */
}

uint32_t process_spawn(const char *name, process_entry_t entry)
{
    if (current != 0u || name == NULL || entry == NULL || next_pid == 0u) return 0u;
    size_t length = 0u;
    while (length < PROCESS_NAME_CAPACITY && name[length] != '\0') ++length;
    if (length == 0u || length == PROCESS_NAME_CAPACITY) return 0u;

    size_t slot = 1u;
    while (slot < PROCESS_MAX && processes[slot].info.state != PROCESS_UNUSED &&
           processes[slot].info.state != PROCESS_TERMINATED) ++slot;
    if (slot == PROCESS_MAX) return 0u;

    uint32_t flags = interrupts_save();
    pcb_t *process = &processes[slot];
    memset(process, 0, sizeof(*process));
    process->info.pid = next_pid++;
    memcpy(process->info.name, name, length + 1u);
    process->info.state = PROCESS_READY;
    process->entry = entry;

    uint32_t *sp = stacks[slot - 1u] + PROCESS_STACK_WORDS;
    /* Al entrar en una funcion C, ESP + 4 debe estar alineado a 16 bytes.
       context.S restaura: EDI, ESI, EBX, EBP, EFLAGS y direccion de retorno. */
    *--sp = 0u; /* Retorno ficticio: process_start no retorna. */
    *--sp = (uint32_t)(uintptr_t)process_start;
    *--sp = 0x202u; /* EFLAGS: interrupciones activas, DF desactivado. */
    *--sp = 0u; /* EBP */
    *--sp = 0u; /* EBX */
    *--sp = 0u; /* ESI */
    *--sp = 0u; /* EDI */
    process->stack_pointer = sp;
    interrupts_restore(flags);
    return process->info.pid;
}

static size_t find_ready(uint32_t pid)
{
    for (size_t i = 1u; i < PROCESS_MAX; ++i) {
        if (processes[i].info.pid == pid &&
            processes[i].info.state == PROCESS_READY) return i;
    }
    return PROCESS_MAX;
}

bool process_run(uint32_t pid)
{
    if (current != 0u) return false;
    size_t slot = find_ready(pid);
    if (slot == PROCESS_MAX) return false;

    uint32_t flags = interrupts_save();
    processes[0].info.state = PROCESS_READY;
    processes[slot].info.state = PROCESS_RUNNING;
    current = slot;
    context_switch(&processes[0].stack_pointer, processes[slot].stack_pointer);
    /* Se retoma aqui cuando la tarea cede la CPU o termina. */
    interrupts_restore(flags);
    return true;
}

void process_yield(void)
{
    if (current == 0u) return;
    uint32_t flags = interrupts_save();
    size_t slot = current;
    processes[slot].info.state = PROCESS_READY;
    processes[0].info.state = PROCESS_RUNNING;
    current = 0u;
    context_switch(&processes[slot].stack_pointer, processes[0].stack_pointer);
    /* Otro run reanuda esta misma pila; se conserva el estado de las IRQ. */
    interrupts_restore(flags);
}

_Noreturn void process_exit(void)
{
    if (current == 0u) cpu_halt();
    interrupts_disable();
    size_t slot = current;
    processes[slot].info.state = PROCESS_TERMINATED;
    processes[0].info.state = PROCESS_RUNNING;
    current = 0u;
    context_switch(&processes[slot].stack_pointer, processes[0].stack_pointer);
    cpu_halt(); /* Un proceso terminado no vuelve a ejecutarse. */
}

bool process_kill(uint32_t pid)
{
    if (current != 0u) return false;
    size_t slot = find_ready(pid);
    if (slot == PROCESS_MAX) return false;
    processes[slot].info.state = PROCESS_TERMINATED;
    return true;
}

bool process_get_info(size_t slot, process_info_t *info)
{
    if (slot >= PROCESS_MAX || info == NULL ||
        processes[slot].info.state == PROCESS_UNUSED) return false;
    *info = processes[slot].info;
    return true;
}

uint32_t process_current_pid(void)
{
    return processes[current].info.pid;
}
