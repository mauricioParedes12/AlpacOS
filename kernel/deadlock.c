#include <alpacos/console.h>
#include <alpacos/deadlock.h>
#include <alpacos/interrupts.h>
#include <alpacos/kstring.h>
#include <alpacos/process.h>
#include <alpacos/sync.h>

/* Un recurso mutex tiene un unico propietario, por eso basta con seguir
 * la cadena: hilo -> mutex esperado -> propietario del mutex. */
typedef struct {
    uint32_t tid;
    const mutex_t *mutex;
    bool active;
} wait_entry_t;

static wait_entry_t waiting[THREAD_MAX];
static uint32_t detected_count;
static uint32_t recovered_count;
static uint32_t prevented_count;

static mutex_t resource_a = MUTEX_INITIALIZER;
static mutex_t resource_b = MUTEX_INITIALIZER;
static volatile bool a_holds_first;
static volatile bool b_holds_first;
static uint32_t demo_pid;
static uint32_t demo_tid_a;
static uint32_t demo_tid_b;
static uint32_t completed_workers;
static uint32_t abandoned_workers;

static wait_entry_t *find_wait(uint32_t tid)
{
    for (size_t i = 0u; i < THREAD_MAX; ++i) {
        if (waiting[i].active && waiting[i].tid == tid) return &waiting[i];
    }
    return NULL;
}

/* Solo seguir desde start: un ciclo ajeno no invalida esta solicitud. */
static bool cycle_from(uint32_t start, uint32_t *last)
{
    uint32_t current = start;
    for (size_t hop = 0; hop < THREAD_MAX; ++hop) {
        wait_entry_t *edge = find_wait(current);
        if (edge == NULL || edge->mutex->owner_tid == MUTEX_NO_OWNER) return false;
        if (edge->mutex->owner_tid == start) {
            if (last != NULL) *last = current;
            return true;
        }
        current = edge->mutex->owner_tid;
    }
    return false;
}
static bool detect_cycle(uint32_t *first, uint32_t *second)
{
    for (size_t i = 0; i < THREAD_MAX; ++i) {
        if (waiting[i].active && cycle_from(waiting[i].tid, second)) {
            if (first != NULL) *first = waiting[i].tid;
            return true;
        }
    }
    return false;
}
bool deadlock_has_cycle(void)
{
    uint32_t flags = interrupts_save();
    bool result = detect_cycle(NULL, NULL);
    interrupts_restore(flags);
    return result;
}
bool deadlock_wait_begin(uint32_t tid, const mutex_t *mutex)
{
    if (mutex == NULL) return true;
    uint32_t flags = interrupts_save();
    wait_entry_t *entry = find_wait(tid);
    if (entry == NULL) {
        for (size_t i = 0u; i < THREAD_MAX; ++i) {
            if (!waiting[i].active) { entry = &waiting[i]; break; }
        }
    }
    if (entry == NULL) {
        interrupts_restore(flags);
        return true; /* Sin espacio: no bloquear sin poder registrar la espera. */
    }
    entry->tid = tid;
    entry->mutex = mutex;
    entry->active = true;
    bool cycle = cycle_from(tid, NULL);
    if (cycle) {
        ++detected_count;
    }
    interrupts_restore(flags);
    return cycle;
}

void deadlock_wait_end(uint32_t tid)
{
    uint32_t flags = interrupts_save();
    wait_entry_t *entry = find_wait(tid);
    if (entry != NULL) entry->active = false;
    interrupts_restore(flags);
}

bool deadlock_wait_cycle(uint32_t tid)
{
    uint32_t flags = interrupts_save();
    bool keep_cycle = demo_pid != 0u && (tid == demo_tid_a || tid == demo_tid_b);
    if (!keep_cycle) ++prevented_count;
    interrupts_restore(flags);
    return keep_cycle;
}

static void worker_a(void)
{
    if (!mutex_lock(&resource_a)) return;
    a_holds_first = true;
    while (!b_holds_first) process_yield();
    if (mutex_lock(&resource_b)) {
        ++completed_workers;
        (void)mutex_unlock(&resource_b);
    } else {
        ++abandoned_workers;
    }
    (void)mutex_unlock(&resource_a);
}

static void worker_b(void)
{
    if (!mutex_lock(&resource_b)) return;
    b_holds_first = true;
    while (!a_holds_first) process_yield();
    if (mutex_lock(&resource_a)) {
        ++completed_workers;
        (void)mutex_unlock(&resource_a);
    } else {
        ++abandoned_workers;
    }
    (void)mutex_unlock(&resource_b);
}

static bool demo_is_active(void)
{
    for (size_t i = 1u; i < PROCESS_MAX; ++i) {
        process_info_t info;
        if (process_get_info(i, &info) && info.pid == demo_pid &&
            info.state != PROCESS_TERMINATED) return true;
    }
    return false;
}

static uint32_t find_initial_thread(uint32_t pid)
{
    for (size_t i = 1u; i < THREAD_MAX; ++i) {
        thread_info_t info;
        if (thread_get_info(i, &info) && info.pid == pid &&
            info.state != THREAD_TERMINATED) return info.tid;
    }
    return 0u;
}

uint32_t deadlock_demo_start(void)
{
    uint32_t flags = interrupts_save();
    if (demo_pid != 0u && demo_is_active()) {
        interrupts_restore(flags);
        console_writeln("Ya hay una demostracion activa. Usa deadlock check.");
        return 0u;
    }
    demo_pid = demo_tid_a = demo_tid_b = 0u;
    a_holds_first = b_holds_first = false;
    completed_workers = abandoned_workers = 0u;
    mutex_init(&resource_a);
    mutex_init(&resource_b);

    uint32_t pid = process_spawn("dl-demo", worker_a);
    uint32_t tid_a = pid != 0u ? find_initial_thread(pid) : 0u;
    uint32_t tid_b = tid_a != 0u ? thread_spawn(pid, "dl-b", worker_b) : 0u;
    if (tid_b == 0u) {
        if (pid != 0u) (void)process_kill(pid);
        interrupts_restore(flags);
        console_writeln("No se pudo crear la demo (sin plazas libres).");
        return 0u;
    }
    demo_pid = pid;
    demo_tid_a = tid_a;
    demo_tid_b = tid_b;
    interrupts_restore(flags);
    console_write("Demo iniciada. PID ");
    console_uint(pid);
    console_write(", TID ");
    console_uint(tid_a);
    console_write(" y ");
    console_uint(tid_b);
    console_writeln(". Ejecuta deadlock check para ver el ciclo.");
    return pid;
}

static void check_demo(void)
{
    uint32_t flags = interrupts_save();
    uint32_t first = 0u;
    uint32_t second = 0u;
    bool cycle = detect_cycle(&first, &second);
    console_writeln("Grafo de espera (TID -> TID propietario):");
    bool any = false;
    for (size_t i = 0u; i < THREAD_MAX; ++i) {
        if (!waiting[i].active) continue;
        console_write("  ");
        console_uint(waiting[i].tid);
        console_write(" -> ");
        console_uint(waiting[i].mutex->owner_tid);
        console_putc('\n');
        any = true;
    }
    if (!any) console_writeln("  Sin esperas activas.");
    if (cycle) {
        console_write("DEADLOCK DETECTADO. Ciclo que incluye TID ");
        console_uint(first);
        console_write(" y ");
        console_uint(second);
        console_writeln(".");
    } else {
        console_writeln("No hay ciclos de espera activos.");
    }
    console_write("Detecciones: ");
    console_uint(detected_count);
    console_write(" | Resoluciones: ");
    console_uint(recovered_count);
    console_write(" | Ciclos evitados: ");
    console_uint(prevented_count);
    console_putc('\n');
    console_write("Demo: completaron=");
    console_uint(completed_workers);
    console_write(" cancelaron segunda solicitud=");
    console_uint(abandoned_workers);
    console_putc('\n');
    interrupts_restore(flags);
}

bool deadlock_resolve(void)
{
    uint32_t flags = interrupts_save(), victim = 0u;
    bool found = detect_cycle(&victim, NULL);
    bool result = found && mutex_cancel_wait(victim);
    if (result) ++recovered_count;
    interrupts_restore(flags);
    return result;
}

void deadlock_command(size_t argc, char **argv)
{
    if (argc != 2u) {
        console_writeln("Uso: deadlock demo | check | resolve");
    } else if (kstreq(argv[1], "demo")) {
        (void)deadlock_demo_start();
    } else if (kstreq(argv[1], "check")) {
        check_demo();
    } else if (kstreq(argv[1], "resolve")) {
        console_writeln(deadlock_resolve() ? "Espera cancelada: el hilo liberara sus recursos al reanudarse." : "No hay un ciclo recuperable.");
    } else {
        console_writeln("Uso: deadlock demo | check | resolve");
    }
}
