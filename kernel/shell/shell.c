#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/deadlock.h>
#include <alpacos/drivers.h>
#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/memory.h>
#include <alpacos/parser.h>
#include <alpacos/process.h>
#include <alpacos/shell.h>
#include <alpacos/sync.h>
#include <alpacos/timer.h>
#include <alpacos/version.h>
#include <alpacos/vfs.h>
#include <stdbool.h>

typedef void (*command_handler_t)(size_t argc, char **argv);
typedef struct {
    const char *name;
    const char *description;
    command_handler_t handler;
} command_t;

static void cmd_procmap(size_t argc, char **argv);
static void cmd_ioinfo(size_t argc, char **argv);
static void cmd_faultdemo(size_t argc, char **argv);
static void cmd_help(size_t argc, char **argv);
static void cmd_clear(size_t argc, char **argv);
static void cmd_echo(size_t argc, char **argv);
static void cmd_version(size_t argc, char **argv);
static void cmd_uptime(size_t argc, char **argv);
static void cmd_meminfo(size_t argc, char **argv);
static void cmd_pagealloc(size_t argc, char **argv);
static void cmd_pagefree(size_t argc, char **argv);
static void cmd_modules(size_t argc, char **argv);
static void cmd_halt(size_t argc, char **argv);
static void cmd_ls(size_t argc, char **argv);
static void cmd_mkdir(size_t argc, char **argv);
static void cmd_touch(size_t argc, char **argv);
static void cmd_write(size_t argc, char **argv);
static void cmd_cat(size_t argc, char **argv);
static void cmd_rm(size_t argc, char **argv);
static void cmd_rmdir(size_t argc, char **argv);
static void cmd_ps(size_t argc, char **argv);
static void cmd_threads(size_t argc, char **argv);
static void cmd_spawn(size_t argc, char **argv);
static void cmd_thread(size_t argc, char **argv);
static void cmd_kill(size_t argc, char **argv);
static bool require_args(size_t argc, size_t expected, const char *usage);

static const command_t commands[] = {
    {"procmap", "Muestra CR3 y pagina privada: procmap PID", cmd_procmap},
    {"ioinfo", "Contadores de entrada PS/2 y COM1 por IRQ", cmd_ioinfo},
    {"faultdemo", "Crea un proceso que provoca un fallo de pagina", cmd_faultdemo},
    {"help", "Lista los comandos disponibles", cmd_help},
    {"clear", "Limpia la consola", cmd_clear},
    {"echo", "Muestra los argumentos separados por espacios", cmd_echo},
    {"version", "Muestra la version y arquitectura", cmd_version},
    {"uptime", "Muestra los segundos desde el inicio del temporizador", cmd_uptime},
    {"meminfo", "Muestra RAM, marcos libres y estado de paginacion", cmd_meminfo},
    {"pagealloc", "Reserva y mapea una pagina virtual de prueba", cmd_pagealloc},
    {"pagefree", "Desmapea y libera una pagina: pagefree 0xD0000000", cmd_pagefree},
    {"modules", "Muestra el estado real de los modulos", cmd_modules},
    {"deadlock", "Demo, deteccion y recuperacion: deadlock demo|check|resolve", deadlock_command},
    {"ps", "Lista procesos y cantidad de hilos", cmd_ps},
    {"threads", "Lista hilos, estados, turnos y contador compartido", cmd_threads},
    {"spawn", "Crea un proceso con su primer hilo: spawn nombre", cmd_spawn},
    {"thread", "Agrega un hilo a un proceso: thread PID nombre", cmd_thread},
    {"kill", "Finaliza todos los hilos y libera recursos: kill PID", cmd_kill},
    {"ls", "Lista archivos y directorios: ls [/ruta]", cmd_ls},
    {"mkdir", "Crea directorio: mkdir /ruta", cmd_mkdir},
    {"touch", "Crea archivo vacio: touch /ruta", cmd_touch},
    {"write", "Sobrescribe archivo: write /ruta texto", cmd_write},
    {"cat", "Lee el archivo: cat /ruta", cmd_cat},
    {"rm", "Elimina un archivo: rm /ruta", cmd_rm},
    {"rmdir", "Elimina directorio vacio: rmdir /ruta", cmd_rmdir},
    {"halt", "Detiene la CPU virtual; cierra QEMU para salir", cmd_halt}
};

static bool no_args(size_t argc)
{
    if (argc != 1u) {
        console_writeln("Error: este comando no recibe argumentos.");
        return false;
    }
    return true;
}

static void cmd_help(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("Comandos disponibles:");
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        console_write("  ");
        console_write(commands[i].name);
        console_write(" - ");
        console_writeln(commands[i].description);
    }
}

static void cmd_clear(size_t argc, char **argv)
{
    (void)argv;
    if (no_args(argc)) console_clear();
}

static void cmd_echo(size_t argc, char **argv)
{
    for (size_t i = 1; i < argc; ++i) {
        if (i > 1u) console_putc(' ');
        console_write(argv[i]);
    }
    console_putc('\n');
}

static void cmd_version(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln(ALPACOS_NAME " " ALPACOS_VERSION " | " ALPACOS_ARCH);
    console_writeln("Kernel freestanding en C11 y Assembly (GNU as).");
}

static void cmd_uptime(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_write("Tiempo encendido: ");
    console_uint(timer_uptime());
    console_writeln(" segundos.");
}

static void cmd_meminfo(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    const boot_info_t *info = bootinfo_get();
    memory_stats_t stats;
    memory_get_stats(&stats);
    if (info->memory_available) {
        console_write("Memoria reportada por Multiboot: ");
        console_uint(info->lower_kib + info->upper_kib);
        console_writeln(" KiB");
    } else {
        console_writeln("El cargador no proporciono memoria basica.");
    }
    console_writeln(stats.uses_memory_map ? "Mapa fisico: E820/Multiboot" : "Mapa fisico: memoria basica Multiboot");
    console_write("Marcos de 4 KiB: total=");
    console_uint(stats.total_frames);
    console_write(" usados=");
    console_uint(stats.used_frames);
    console_write(" libres=");
    console_uint(stats.free_frames);
    console_putc('\n');
    console_write("Reservados para memoria baja/kernel: ");
    console_uint(stats.reserved_frames);
    console_writeln(" marcos");
    console_write("Paginacion: ");
    console_writeln(stats.paging_enabled ? "ACTIVA (kernel compartido y directorio propio por proceso)" : "INACTIVA");
    console_write("Kernel: ");
    console_hex((uint32_t)info->kernel_start);
    console_write(" - ");
    console_hex((uint32_t)info->kernel_end);
    console_write(" (fin exclusivo, ");
    console_uint((uint32_t)(info->kernel_end - info->kernel_start));
    console_writeln(" bytes, incluida la pila)");
}

static bool parse_address(const char *text, uintptr_t *address)
{
    uint32_t base = 10u;
    uint32_t value = 0u;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
        base = 16u;
    }
    if (*text == '\0') return false;
    for (; *text != '\0'; ++text) {
        uint32_t digit;
        if (*text >= '0' && *text <= '9') digit = (uint32_t)(*text - '0');
        else if (base == 16u && *text >= 'a' && *text <= 'f') digit = (uint32_t)(*text - 'a') + 10u;
        else if (base == 16u && *text >= 'A' && *text <= 'F') digit = (uint32_t)(*text - 'A') + 10u;
        else return false;
        if (digit >= base || value > (UINT32_MAX - digit) / base) return false;
        value = value * base + digit;
    }
    *address = (uintptr_t)value;
    return true;
}

static bool page_test_slots[MEMORY_TEST_PAGE_COUNT];

static void cmd_pagealloc(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    size_t slot;
    for (slot = 0u; slot < MEMORY_TEST_PAGE_COUNT; ++slot) {
        if (!page_test_slots[slot]) break;
    }
    if (slot == MEMORY_TEST_PAGE_COUNT) {
        console_writeln("Error: se alcanzo el limite de paginas de prueba.");
        return;
    }

    uintptr_t virtual_address = MEMORY_TEST_VIRTUAL_BASE + slot * MEMORY_PAGE_SIZE;
    uintptr_t physical_address;
    if (!memory_page_alloc(virtual_address, &physical_address)) {
        console_writeln("Error: no se pudo reservar o mapear el marco fisico.");
        return;
    }
    volatile uint32_t *probe = (volatile uint32_t *)virtual_address;
    *probe = 0xA1FA0C05u;
    if (*probe != 0xA1FA0C05u) {
        (void)memory_page_free(virtual_address, NULL);
        console_writeln("Error: la prueba de lectura/escritura de la pagina fallo.");
        return;
    }
    page_test_slots[slot] = true;
    console_write("Pagina virtual: ");
    console_hex((uint32_t)virtual_address);
    console_write(" -> marco fisico: ");
    console_hex((uint32_t)physical_address);
    console_writeln(". Lectura/escritura correcta.");
}

static void cmd_pagefree(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "pagefree 0xD0000000")) return;
    uintptr_t virtual_address;
    if (!parse_address(argv[1], &virtual_address) ||
        virtual_address < MEMORY_TEST_VIRTUAL_BASE ||
        virtual_address >= MEMORY_TEST_VIRTUAL_BASE + MEMORY_TEST_PAGE_COUNT * MEMORY_PAGE_SIZE ||
        (virtual_address & (MEMORY_PAGE_SIZE - 1u)) != 0u) {
        console_writeln("Error: direccion invalida; usa una direccion mostrada por pagealloc.");
        return;
    }
    size_t slot = (virtual_address - MEMORY_TEST_VIRTUAL_BASE) / MEMORY_PAGE_SIZE;
    if (!page_test_slots[slot]) {
        console_writeln("Error: esa pagina no esta reservada.");
        return;
    }
    uintptr_t physical_address;
    if (!memory_page_free(virtual_address, &physical_address)) {
        console_writeln("Error: no se pudo desmapear o liberar el marco.");
        return;
    }
    page_test_slots[slot] = false;
    console_write("Pagina liberada. Marco fisico: ");
    console_hex((uint32_t)physical_address);
    console_putc('\n');
}

static void cmd_modules(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("[BASE] Boot Multiboot + kernel + GDT + stack");
    console_writeln("[BASE] Shell y VGA; entrada COM1 por IRQ4");
    console_writeln("[BASE] IDT, excepciones y PIC con IRQ 0..15");
    console_writeln("[BASE] PIT a unos 100 Hz y teclado PS/2 US por IRQ");
    console_writeln("[OK] Procesos, PCB y contexto completo por interrupciones");
    console_writeln("[OK] Round Robin preventivo: 10 ticks por turno (~100 ms)");
    console_writeln("[OK] Hilos del kernel y Round Robin por TID");
    console_writeln("[OK] Mutex, semaforos, sleep e idle; limpieza al finalizar");
    console_writeln("[OK] Allocator de marcos de 4 KiB y paginacion x86 activa");
    console_writeln("[OK] CR3 por proceso y pagina privada en 0x40000000");
    console_writeln("[OK] VFS en RAM: directorios y archivos (sin persistencia)");
    console_writeln("[OK] Deteccion de ciclos y recuperacion de deadlocks con mutex");
}

static bool require_args(size_t argc, size_t expected, const char *usage)
{
    if (argc != expected) {
        console_write("Uso: ");
        console_writeln(usage);
        return false;
    }
    return true;
}

typedef struct {
    uint32_t pid;
    volatile uint32_t counter;
    mutex_t mutex;
} demo_context_t;

static demo_context_t demo_contexts[PROCESS_MAX - 1u];

static demo_context_t *demo_context_for_pid(uint32_t pid)
{
    for (size_t i = 0u; i < PROCESS_MAX - 1u; ++i) {
        if (demo_contexts[i].pid == pid) return &demo_contexts[i];
    }
    return NULL;
}

static demo_context_t *demo_context_available(void)
{
    for (size_t i = 0u; i < PROCESS_MAX - 1u; ++i) {
        if (demo_contexts[i].pid == 0u) return &demo_contexts[i];
        bool active = false;
        for (size_t slot = 1u; slot < PROCESS_MAX; ++slot) {
            process_info_t info;
            if (process_get_info(slot, &info) &&
                info.pid == demo_contexts[i].pid &&
                info.state != PROCESS_TERMINATED) {
                active = true;
                break;
            }
        }
        if (!active) {
            demo_contexts[i].pid = 0u;
            return &demo_contexts[i];
        }
    }
    return NULL;
}

static void demo_thread(void)
{
    demo_context_t *context = demo_context_for_pid(process_current_pid());
    if (context == NULL) return;

    volatile uint32_t *counter = (volatile uint32_t *)MEMORY_PROCESS_BASE;
    for (;;) {
        if (!mutex_lock(&context->mutex)) return;
        ++*counter;
        context->counter = *counter;
        process_yield();
        if (!mutex_unlock(&context->mutex)) return;
    }
}

static void cmd_ps(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_write("Round Robin por hilos: quantum de ");
    console_uint(PROCESS_QUANTUM_TICKS);
    console_writeln(" ticks. El shell tambien participa.");
    console_writeln("PID  ESTADO       HILOS  TICKS  TURNOS  NOMBRE");
    for (size_t i = 0u; i < PROCESS_MAX; ++i) {
        process_info_t info;
        if (!process_get_info(i, &info)) continue;
        console_uint(info.pid);
        console_write("  ");
        switch (info.state) {
            case PROCESS_READY: console_write("READY        "); break;
            case PROCESS_RUNNING: console_write("RUNNING      "); break;
            case PROCESS_BLOCKED: console_write("BLOCKED      "); break;
            case PROCESS_TERMINATED: console_write("TERMINATED   "); break;
            default: console_write("UNUSED       "); break;
        }
        console_uint(info.thread_count);
        console_write("      ");
        console_uint(info.cpu_ticks);
        console_write("  ");
        console_uint(info.turns);
        console_write("  ");
        console_writeln(info.name);
    }
}

static void cmd_threads(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("TID  PID  ESTADO       TICKS  TURNOS  NOMBRE");
    for (size_t i = 0u; i < THREAD_MAX; ++i) {
        thread_info_t info;
        if (!thread_get_info(i, &info)) continue;
        console_uint(info.tid);
        console_write("  ");
        console_uint(info.pid);
        console_write("  ");
        switch (info.state) {
            case THREAD_READY: console_write("READY        "); break;
            case THREAD_RUNNING: console_write("RUNNING      "); break;
            case THREAD_SLEEPING: console_write("SLEEPING     "); break;
            case THREAD_BLOCKED: console_write("BLOCKED      "); break;
            case THREAD_TERMINATED: console_write("TERMINATED   "); break;
            default: console_write("UNUSED       "); break;
        }
        console_uint(info.cpu_ticks);
        console_write("  ");
        console_uint(info.turns);
        console_write("  ");
        console_writeln(info.name);
    }
    console_writeln("Contadores compartidos por proceso:");
    for (size_t i = 0u; i < PROCESS_MAX - 1u; ++i) {
        if (demo_contexts[i].pid == 0u) continue;
        console_write("  PID ");
        console_uint(demo_contexts[i].pid);
        console_write(": ");
        console_uint(demo_contexts[i].counter);
        console_putc('\n');
    }
}

static void cmd_spawn(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "spawn nombre")) return;
    demo_context_t *context = demo_context_available();
    if (context == NULL) {
        console_writeln("Error: no hay espacio para otro proceso de prueba.");
        return;
    }

    uint32_t flags = interrupts_save();
    uint32_t pid = process_spawn(argv[1], demo_thread);
    if (pid != 0u) {
        context->pid = pid;
        context->counter = 0u;
        mutex_init(&context->mutex);
    }
    interrupts_restore(flags);
    if (pid == 0u) {
        console_writeln("Error: nombre invalido o limite de procesos/hilos alcanzado.");
        return;
    }
    console_write("Proceso creado. PID: ");
    console_uint(pid);
    console_writeln(". Tiene un hilo inicial; usa thread para agregar mas.");
}

static bool parse_pid(const char *text, uint32_t *pid)
{
    uint32_t value = 0u;
    if (*text == '\0') return false;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9') return false;
        uint32_t digit = (uint32_t)(*text - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    *pid = value;
    return true;
}

static void cmd_thread(size_t argc, char **argv)
{
    if (!require_args(argc, 3u, "thread PID nombre")) return;
    uint32_t pid;
    if (!parse_pid(argv[1], &pid)) {
        console_writeln("Error: PID invalido.");
        return;
    }
    demo_context_t *context = demo_context_for_pid(pid);
    if (context == NULL) {
        console_writeln("Error: el proceso no existe o ya esta finalizando.");
        return;
    }
    uint32_t tid = thread_spawn(pid, argv[2], demo_thread);
    if (tid == 0u) {
        console_writeln("Error: proceso inexistente o limite de hilos alcanzado.");
        return;
    }
    console_write("Hilo creado. TID: ");
    console_uint(tid);
    console_write(" (PID ");
    console_uint(pid);
    console_writeln(").");
}

static void cmd_kill(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "kill PID")) return;
    uint32_t pid;
    if (!parse_pid(argv[1], &pid)) {
        console_writeln("Error: PID invalido.");
        return;
    }

    if (!process_kill(pid)) {
        console_writeln("Error: proceso inexistente o protegido (PID 0).");
        return;
    }
    console_writeln("Proceso terminado.");
}

static void fs_result(vfs_result_t result)
{
    if (result != VFS_OK) {
        console_write("Error VFS: ");
        console_writeln(vfs_result_message(result));
    }
}

static void print_entry(const char *name, bool is_directory,
                        size_t size, void *context)
{
    bool *any = context;
    *any = true;
    console_write(is_directory ? "  [DIR]  " : "  [FILE] ");
    console_write(name);
    if (is_directory) {
        console_putc('/');
    } else {
        console_write(" (");
        console_uint((uint32_t)size);
        console_write(" bytes)");
    }
    console_putc('\n');
}

static void cmd_ls(size_t argc, char **argv)
{
    if (argc > 2u) {
        console_writeln("Uso: ls [/ruta]");
        return;
    }
    bool any = false;
    vfs_result_t result = vfs_list(argc == 2u ? argv[1] : "/",
                                   print_entry, &any);
    if (result == VFS_OK && !any) console_writeln("  (directorio vacio)");
    fs_result(result);
}

static void cmd_mkdir(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "mkdir /ruta")) return;
    fs_result(vfs_mkdir(argv[1]));
}

static void cmd_touch(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "touch /ruta")) return;
    fs_result(vfs_touch(argv[1]));
}

static void cmd_write(size_t argc, char **argv)
{
    if (argc < 3u) {
        console_writeln("Uso: write /ruta texto (separado por espacios)");
        return;
    }
    char content[VFS_FILE_CAPACITY];
    size_t used = 0u;
    for (size_t i = 2u; i < argc; ++i) {
        if (i > 2u) content[used++] = ' ';
        size_t length = kstrlen(argv[i]);
        if (length >= sizeof(content) - used) {
            fs_result(VFS_ERR_TOO_LARGE);
            return;
        }
        memcpy(content + used, argv[i], length);
        used += length;
    }
    content[used] = '\0';
    fs_result(vfs_write(argv[1], content));
}

static void cmd_cat(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "cat /ruta")) return;
    char content[VFS_FILE_CAPACITY];
    size_t size;
    vfs_result_t result = vfs_read_copy(argv[1], content, sizeof(content), &size);
    if (result == VFS_OK) {
        (void)size;
        console_writeln(content);
    } else {
        fs_result(result);
    }
}

static void cmd_rm(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "rm /ruta")) return;
    fs_result(vfs_unlink(argv[1]));
}

static void cmd_rmdir(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "rmdir /ruta")) return;
    fs_result(vfs_rmdir(argv[1]));
}

static void cmd_halt(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("AlpacOS detenido. Cierra QEMU para terminar.");
    cpu_halt();
}

static void execute_line(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    size_t argc;
    parse_result_t result = shell_parse(line, argv, SHELL_MAX_ARGS, &argc);
    if (result == PARSE_EMPTY) return;
    if (result == PARSE_TOO_MANY_ARGS) {
        console_writeln("Error: maximo 16 palabras, incluido el comando.");
        return;
    }
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (kstreq(argv[0], commands[i].name)) {
            commands[i].handler(argc, argv);
            return;
        }
    }
    console_write("Comando desconocido: ");
    console_writeln(argv[0]);
}

static void prompt(void)
{
    console_set_color(0x0A);
    console_write("alpacos> ");
    console_set_color(0x07);
}

_Noreturn void shell_run(void)
{
    char line[SHELL_LINE_CAPACITY];
    size_t used = 0;
    bool overflow = false;
    bool previous_cr = false;
    unsigned escape_state = 0;
    prompt();
    for (;;) {
        interrupts_disable();
        int input = console_read();
        if (input < 0) {
            interrupts_enable();
            thread_sleep(1u);
            continue;
        }
        interrupts_enable();
        char c = (char)input;
        /* Ignore terminal arrow/function-key escape sequences (no history yet). */
        if (escape_state == 1u) {
            escape_state = (c == '[' || c == 'O') ? 2u : 0u;
            continue;
        }
        if (escape_state == 2u) {
            if (c >= '@' && c <= '~') escape_state = 0u;
            continue;
        }
        if (c == 27) {
            escape_state = 1u;
            continue;
        }
        if (c == '\n' && previous_cr) {
            previous_cr = false;
            continue;
        }
        previous_cr = c == '\r';
        if (c == '\n' || c == '\r') {
            console_putc('\n');
            if (overflow) {
                console_writeln("Error: linea demasiado larga (maximo 127 caracteres). Descartada.");
            } else {
                line[used] = '\0';
                execute_line(line);
            }
            used = 0;
            overflow = false;
            prompt();
        } else if (c == 3) { /* Ctrl+C through the serial terminal. */
            used = 0;
            overflow = false;
            console_writeln("^C");
            prompt();
        } else if (!overflow && (c == '\b' || c == 127)) {
            if (used > 0u) {
                --used;
                console_putc('\b');
            }
        } else if (!overflow && (c == '\t' || (c >= 32 && c <= 126))) {
            if (used + 1u < sizeof(line)) {
                line[used++] = c == '\t' ? ' ' : c;
                console_putc(c == '\t' ? ' ' : c);
            } else {
                /* Never execute a silently truncated command. */
                overflow = true;
            }
        }
    }
}

static void cmd_procmap(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "procmap PID")) return;
    uint32_t pid; process_info_t info;
    if (!parse_pid(argv[1], &pid) || !process_find_info(pid, &info) || info.state == PROCESS_TERMINATED) {
        console_writeln("Error: proceso no activo."); return;
    }
    console_write("PID "); console_uint(pid);
    console_write(" CR3="); console_hex(info.address_space);
    if (pid != 0u) {
        console_write(" | virtual="); console_hex(MEMORY_PROCESS_BASE);
        console_write(" -> fisica="); console_hex(info.private_frame);
        console_write(" | dato="); console_uint(*(volatile uint32_t *)info.private_frame);
    }
    console_writeln(" | ring 0");
}
static void cmd_ioinfo(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    input_stats_t serial, keyboard;
    serial_get_stats(&serial); keyboard_get_stats(&keyboard);
    console_write("COM1 IRQ4: recibidos="); console_uint(serial.received);
    console_write(" descartados="); console_uint(serial.dropped);
    console_write("\nPS/2 IRQ1: recibidos="); console_uint(keyboard.received);
    console_write(" descartados="); console_uint(keyboard.dropped);
    console_putc('\n');
}
static void fault_worker(void) { *(volatile uint32_t *)0x80000000u = 1u; }
static void cmd_faultdemo(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    uint32_t pid = process_spawn("fallo-pagina", fault_worker);
    console_write("Proceso de prueba de fallo: PID "); console_uint(pid); console_putc('\n');
}
