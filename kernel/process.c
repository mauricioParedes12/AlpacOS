#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/process.h>
#include <alpacos/sync.h>

#define THREAD_STACK_WORDS 2048u /* 8 KiB por hilo. */

typedef struct {
    process_info_t info;
} pcb_t;

typedef struct {
    thread_info_t info;
    interrupt_frame_t *context;
    thread_entry_t entry;
    size_t process_slot;
    uint32_t wake_tick;
} tcb_t;

static pcb_t processes[PROCESS_MAX];
static tcb_t threads[THREAD_MAX];
static _Alignas(16) uint32_t stacks[THREAD_MAX - 1u][THREAD_STACK_WORDS];
static size_t current_thread;
static uint32_t next_pid;
static uint32_t next_tid;
static unsigned slice_ticks;
static uint32_t scheduler_ticks;
#define IDLE_SLOT 1u

/* La cola contiene indices de TCB; el hilo RUNNING no esta en ella. */
static size_t ready[THREAD_MAX];
static size_t ready_head;
static size_t ready_count;

static _Noreturn void thread_start(void);
static void idle_entry(void) { for (;;) interrupts_wait(); }

static bool valid_name(const char *name, size_t capacity, size_t *length)
{
    if (name == NULL) return false;
    size_t used = 0u;
    while (used < capacity && name[used] != '\0') ++used;
    if (used == 0u || used == capacity) return false;
    if (length != NULL) *length = used;
    return true;
}

static void ready_push(size_t slot)
{
    ready[(ready_head + ready_count) % THREAD_MAX] = slot;
    ++ready_count;
}

static size_t ready_pop(void)
{
    size_t slot = ready[ready_head];
    ready_head = (ready_head + 1u) % THREAD_MAX;
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

static void refresh_process_state(size_t process_slot)
{
    pcb_t *process = &processes[process_slot];
    uint32_t active = 0u;
    bool running = false;
    bool ready_thread = false;
    bool blocked = false;

    for (size_t i = 0u; i < THREAD_MAX; ++i) {
        const tcb_t *thread = &threads[i];
        if (thread->process_slot != process_slot ||
            thread->info.state == THREAD_UNUSED ||
            thread->info.state == THREAD_TERMINATED) continue;
        ++active;
        if (thread->info.state == THREAD_RUNNING) running = true;
        if (thread->info.state == THREAD_READY) ready_thread = true;
        if (thread->info.state == THREAD_BLOCKED || thread->info.state == THREAD_SLEEPING) blocked = true;
    }

    process->info.thread_count = active;
    if (active == 0u) {
        process->info.state = PROCESS_TERMINATED;
    } else if (running) {
        process->info.state = PROCESS_RUNNING;
    } else if (ready_thread) {
        process->info.state = PROCESS_READY;
    } else if (blocked) {
        process->info.state = PROCESS_BLOCKED;
    }
}

static size_t find_process(uint32_t pid)
{
    for (size_t i = 0u; i < PROCESS_MAX; ++i) {
        if (processes[i].info.pid == pid &&
            processes[i].info.state != PROCESS_UNUSED &&
            processes[i].info.state != PROCESS_TERMINATED) return i;
    }
    return PROCESS_MAX;
}

static size_t find_free_thread(void)
{
    for (size_t i = 2u; i < THREAD_MAX; ++i) {
        if (threads[i].info.state == THREAD_UNUSED ||
            threads[i].info.state == THREAD_TERMINATED) return i;
    }
    return THREAD_MAX;
}

static size_t find_free_process(void)
{
    for (size_t i = 1u; i < PROCESS_MAX; ++i) {
        if (processes[i].info.state == PROCESS_UNUSED ||
            processes[i].info.state == PROCESS_TERMINATED) return i;
    }
    return PROCESS_MAX;
}

static size_t create_thread_at(size_t slot, size_t process_slot, const char *name,
                            size_t name_length, thread_entry_t entry)
{
    if (slot == THREAD_MAX || next_tid == 0u || next_tid == UINT32_MAX) return THREAD_MAX;

    tcb_t *thread = &threads[slot];
    memset(thread, 0, sizeof(*thread));
    thread->info.tid = next_tid++;
    thread->info.pid = processes[process_slot].info.pid;
    memcpy(thread->info.name, name, name_length + 1u);
    thread->info.state = THREAD_READY;
    thread->entry = entry;
    thread->process_slot = process_slot;

    /* Prepara el marco que restauraran isr.S e IRET al arrancar el hilo. */
    uint32_t *top = stacks[slot - 1u] + THREAD_STACK_WORDS;
    *--top = 0u; /* Retorno ficticio: thread_start no retorna. */
    interrupt_frame_t *frame = (interrupt_frame_t *)
        ((unsigned char *)top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));
    frame->gs = frame->fs = frame->es = frame->ds = 0x10u;
    frame->eip = (uint32_t)(uintptr_t)thread_start;
    frame->cs = 0x08u;
    frame->eflags = 0x202u; /* IF activo, DF desactivado. */
    thread->context = frame;
    if (slot != IDLE_SLOT) ready_push(slot);
    return slot;
}

static _Noreturn void thread_start(void)
{
    threads[current_thread].entry();
    thread_exit();
}

void process_init(void)
{
    memset(processes, 0, sizeof(processes));
    memset(threads, 0, sizeof(threads));
    current_thread = ready_head = ready_count = 0u;
    slice_ticks = 0u;
    next_pid = 1u;
    next_tid = 2u;
    scheduler_ticks = 0u;

    processes[0].info.pid = 0u;
    memcpy(processes[0].info.name, "shell", sizeof("shell"));
    processes[0].info.state = PROCESS_RUNNING;
    processes[0].info.thread_count = 2u;
    processes[0].info.address_space = memory_space_kernel();

    threads[0].info.tid = 0u;
    threads[0].info.pid = 0u;
    memcpy(threads[0].info.name, "shell", sizeof("shell"));
    threads[0].info.state = THREAD_RUNNING;
    threads[0].process_slot = 0u;
    (void)create_thread_at(IDLE_SLOT, 0u, "idle", 4u, idle_entry);
    threads[IDLE_SLOT].info.tid = 1u;
    next_tid = 2u;
    /* El primer IRQ guarda el contexto del shell en su pila original. */
}

uint32_t process_spawn(const char *name, process_entry_t entry)
{
    size_t length;
    if (!valid_name(name, PROCESS_NAME_CAPACITY, &length) || entry == NULL) return 0u;

    uint32_t flags = interrupts_save();
    if (threads[current_thread].process_slot != 0u || next_pid == 0u) {
        interrupts_restore(flags);
        return 0u;
    }
    size_t process_slot = find_free_process();
    if (process_slot == PROCESS_MAX || find_free_thread() == THREAD_MAX) {
        interrupts_restore(flags);
        return 0u;
    }

    pcb_t *process = &processes[process_slot];
    memset(process, 0, sizeof(*process));
    process->info.pid = next_pid++;
    memcpy(process->info.name, name, length + 1u);
    process->info.state = PROCESS_READY;
    process->info.address_space = memory_space_create();
    if (process->info.address_space == 0u ||
        !memory_space_alloc(process->info.address_space, MEMORY_PROCESS_BASE, &process->info.private_frame)) {
        if (process->info.address_space != 0u) (void)memory_space_destroy(process->info.address_space);
        memset(process, 0, sizeof(*process));
        interrupts_restore(flags);
        return 0u;
    }
    if (create_thread_at(find_free_thread(), process_slot, name, length, entry) == THREAD_MAX) {
        (void)memory_space_destroy(process->info.address_space);
        memset(process, 0, sizeof(*process));
        interrupts_restore(flags);
        return 0u;
    }
    refresh_process_state(process_slot);
    uint32_t pid = process->info.pid;
    interrupts_restore(flags);
    return pid;
}

uint32_t thread_spawn(uint32_t pid, const char *name, thread_entry_t entry)
{
    size_t length;
    if (!valid_name(name, THREAD_NAME_CAPACITY, &length) || entry == NULL) return 0u;

    uint32_t flags = interrupts_save();
    if (pid == 0u || (threads[current_thread].process_slot != 0u &&
        threads[current_thread].info.pid != pid)) {
        interrupts_restore(flags);
        return 0u;
    }
    size_t process_slot = find_process(pid);
    if (process_slot == PROCESS_MAX || find_free_thread() == THREAD_MAX) {
        interrupts_restore(flags);
        return 0u;
    }
    size_t thread_slot = create_thread_at(find_free_thread(), process_slot, name, length, entry);
    if (thread_slot == THREAD_MAX) {
        interrupts_restore(flags);
        return 0u;
    }
    refresh_process_state(process_slot);
    uint32_t tid = threads[thread_slot].info.tid;
    interrupts_restore(flags);
    return tid;
}

/* Primero se cambia CR3; despues se pueden devolver las paginas del anterior.
 * Las pilas y el codigo del kernel tienen el mismo mapeo en todos los espacios. */
static void reap_spaces(void)
{
    for (size_t i = 1u; i < PROCESS_MAX; ++i) {
        process_info_t *p = &processes[i].info;
        if (p->state == PROCESS_TERMINATED && p->address_space != 0u &&
            memory_space_destroy(p->address_space)) {
            p->address_space = 0u;
            p->private_frame = 0u;
        }
    }
}
interrupt_frame_t *process_schedule(interrupt_frame_t *frame, bool timer_tick)
{
    tcb_t *old = &threads[current_thread];
    old->context = frame;
    if (timer_tick) {
        ++scheduler_ticks;
        for (size_t i = 0; i < THREAD_MAX; ++i) {
            if (threads[i].info.state == THREAD_SLEEPING &&
                (int32_t)(scheduler_ticks - threads[i].wake_tick) >= 0) {
                threads[i].info.state = THREAD_READY;
                ready_push(i);
                refresh_process_state(threads[i].process_slot);
            }
        }
        ++old->info.cpu_ticks;
        ++processes[old->process_slot].info.cpu_ticks;
        if (++slice_ticks < PROCESS_QUANTUM_TICKS && current_thread != IDLE_SLOT) return frame;
    }
    slice_ticks = 0u;
    if (old->info.state == THREAD_RUNNING) {
        old->info.state = THREAD_READY;
        if (current_thread != IDLE_SLOT) ready_push(current_thread);
        refresh_process_state(old->process_slot);
    }
    current_thread = ready_count != 0u ? ready_pop() : IDLE_SLOT;
    tcb_t *next = &threads[current_thread];
    next->info.state = THREAD_RUNNING;
    ++next->info.turns;
    ++processes[next->process_slot].info.turns;
    refresh_process_state(next->process_slot);
    memory_space_activate(processes[next->process_slot].info.address_space);
    reap_spaces();
    return next->context;
}
static void terminate_process(size_t slot, int32_t status)
{
    /* Marcar primero impide transferir un mutex a otro hilo que tambien muere. */
    for (size_t i = 2u; i < THREAD_MAX; ++i) {
        if (threads[i].process_slot == slot && threads[i].info.state != THREAD_UNUSED &&
            threads[i].info.state != THREAD_TERMINATED) {
            ready_remove(i);
            threads[i].info.state = THREAD_TERMINATED;
        }
    }
    for (size_t i = 2u; i < THREAD_MAX; ++i) {
        if (threads[i].process_slot == slot && threads[i].info.state == THREAD_TERMINATED)
            sync_thread_cleanup(threads[i].info.tid);
    }
    processes[slot].info.exit_status = status;
    refresh_process_state(slot);
}
_Noreturn void thread_exit(void)
{
    interrupts_disable();
    if (current_thread < 2u) cpu_halt();
    threads[current_thread].info.state = THREAD_TERMINATED;
    sync_thread_cleanup(threads[current_thread].info.tid);
    refresh_process_state(threads[current_thread].process_slot);
    process_yield();
    cpu_halt();
}
_Noreturn void process_exit_with_status(int32_t status)
{
    interrupts_disable();
    size_t slot = threads[current_thread].process_slot;
    if (slot == 0u) cpu_halt();
    terminate_process(slot, status);
    process_yield();
    cpu_halt();
}
_Noreturn void process_exit(void) { process_exit_with_status(0); }
bool process_kill(uint32_t pid)
{
    uint32_t flags = interrupts_save();
    size_t slot = find_process(pid);
    if (pid == 0u || slot == PROCESS_MAX || threads[current_thread].process_slot != 0u) {
        interrupts_restore(flags);
        return false;
    }
    terminate_process(slot, PROCESS_EXIT_KILLED);
    reap_spaces();
    interrupts_restore(flags);
    return true;
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

bool thread_get_info(size_t slot, thread_info_t *info)
{
    if (slot >= THREAD_MAX || info == NULL) return false;
    uint32_t flags = interrupts_save();
    bool used = threads[slot].info.state != THREAD_UNUSED;
    if (used) *info = threads[slot].info;
    interrupts_restore(flags);
    return used;
}

uint32_t process_current_pid(void)
{
    uint32_t flags = interrupts_save();
    uint32_t pid = threads[current_thread].info.pid;
    interrupts_restore(flags);
    return pid;
}

uint32_t thread_current_tid(void)
{
    uint32_t flags = interrupts_save();
    uint32_t tid = threads[current_thread].info.tid;
    interrupts_restore(flags);
    return tid;
}

uint32_t process_cpu_ticks(void)
{
    uint32_t flags = interrupts_save();
    uint32_t ticks = processes[threads[current_thread].process_slot].info.cpu_ticks;
    interrupts_restore(flags);
    return ticks;
}

bool thread_block_current(void)
{
    uint32_t flags = interrupts_save();
    if (current_thread == IDLE_SLOT || threads[current_thread].info.state != THREAD_RUNNING) {
        interrupts_restore(flags);
        return false;
    }
    size_t process_slot = threads[current_thread].process_slot;
    threads[current_thread].info.state = THREAD_BLOCKED;
    refresh_process_state(process_slot);
    process_yield(); /* Al reanudarse, este mismo hilo continua aqui. */
    interrupts_restore(flags);
    return true;
}

bool thread_unblock(uint32_t tid)
{
    uint32_t flags = interrupts_save();
    for (size_t i = 0u; i < THREAD_MAX; ++i) {
        if (i == IDLE_SLOT) continue;
        if (threads[i].info.tid == tid &&
            threads[i].info.state == THREAD_BLOCKED) {
            threads[i].info.state = THREAD_READY;
            ready_push(i);
            refresh_process_state(threads[i].process_slot);
            interrupts_restore(flags);
            return true;
        }
    }
    interrupts_restore(flags);
    return false;
}

uint32_t process_ticks(void) { return scheduler_ticks; }
bool process_find_info(uint32_t pid, process_info_t *info)
{
    if (info == NULL) return false;
    uint32_t flags = interrupts_save();
    for (size_t i = 0; i < PROCESS_MAX; ++i) {
        if (processes[i].info.state != PROCESS_UNUSED && processes[i].info.pid == pid) {
            *info = processes[i].info;
            interrupts_restore(flags);
            return true;
        }
    }
    interrupts_restore(flags);
    return false;
}
void thread_sleep(uint32_t ticks)
{
    if (ticks == 0u) { process_yield(); return; }
    if (ticks > INT32_MAX || current_thread == IDLE_SLOT) return;
    uint32_t flags = interrupts_save();
    threads[current_thread].wake_tick = scheduler_ticks + ticks;
    threads[current_thread].info.state = THREAD_SLEEPING;
    refresh_process_state(threads[current_thread].process_slot);
    process_yield();
    interrupts_restore(flags);
}
