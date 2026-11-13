#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/parser.h>
#include <alpacos/process.h>
#include <alpacos/shell.h>
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

static void cmd_help(size_t argc, char **argv);
static void cmd_clear(size_t argc, char **argv);
static void cmd_echo(size_t argc, char **argv);
static void cmd_version(size_t argc, char **argv);
static void cmd_uptime(size_t argc, char **argv);
static void cmd_meminfo(size_t argc, char **argv);
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
static void cmd_spawn(size_t argc, char **argv);
static void cmd_run(size_t argc, char **argv);
static void cmd_kill(size_t argc, char **argv);

static const command_t commands[] = {
    {"help", "Lista los comandos disponibles", cmd_help},
    {"clear", "Limpia la consola", cmd_clear},
    {"echo", "Muestra los argumentos separados por espacios", cmd_echo},
    {"version", "Muestra la version y arquitectura", cmd_version},
    {"uptime", "Muestra los segundos desde el inicio del temporizador", cmd_uptime},
    {"meminfo", "Muestra datos del arranque, no memoria libre", cmd_meminfo},
    {"modules", "Muestra el estado real de los modulos", cmd_modules},
    {"ps", "Lista PID, estado y nombre de los procesos", cmd_ps},
    {"spawn", "Crea un proceso de demostracion: spawn nombre", cmd_spawn},
    {"run", "Ejecuta/reanuda hasta ceder la CPU: run PID", cmd_run},
    {"kill", "Termina un proceso listo: kill PID", cmd_kill},
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
    if (info->memory_available) {
        console_write("Memoria convencional reportada: ");
        console_uint(info->lower_kib);
        console_writeln(" KiB");
        console_write("Memoria superior contigua reportada (desde 1 MiB): ");
        console_uint(info->upper_kib);
        console_writeln(" KiB");
    } else {
        console_writeln("El cargador no proporciono memoria basica.");
    }
    console_write("Kernel: ");
    console_hex((uint32_t)info->kernel_start);
    console_write(" - ");
    console_hex((uint32_t)info->kernel_end);
    console_write(" (fin exclusivo, ");
    console_uint((uint32_t)(info->kernel_end - info->kernel_start));
    console_writeln(" bytes reservados, incluida la pila)");
    console_writeln("Datos de arranque. Sin allocator ni paginacion todavia.");
}

static void cmd_modules(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("[BASE] Boot Multiboot + kernel + GDT + stack");
    console_writeln("[BASE] Shell, VGA y COM1 por sondeo");
    console_writeln("[BASE] IDT, excepciones y PIC con IRQ 0..15");
    console_writeln("[BASE] PIT a unos 100 Hz y teclado PS/2 US por IRQ");
    console_writeln("[OK] Procesos, PCB y contexto cooperativo con run PID");
    console_writeln("[TODO] Planificacion automatica de CPU (sin quantum)");
    console_writeln("[TODO] Threads y sincronizacion");
    console_writeln("[TODO] Memoria y paginacion");
    console_writeln("[OK] VFS en RAM: directorios y archivos (sin persistencia)");
    console_writeln("[TODO] Deteccion/gestion de deadlocks");
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

static void cmd_ps(size_t argc, char **argv)
{
    (void)argv;
    if (!no_args(argc)) return;
    console_writeln("PID  ESTADO       NOMBRE");
    for (size_t i = 0u; i < PROCESS_MAX; ++i) {
        process_info_t info;
        if (!process_get_info(i, &info)) continue;
        console_uint(info.pid);
        console_write("  ");
        switch (info.state) {
            case PROCESS_READY: console_write("READY        "); break;
            case PROCESS_RUNNING: console_write("RUNNING      "); break;
            case PROCESS_TERMINATED: console_write("TERMINATED   "); break;
            default: console_write("UNUSED       "); break;
        }
        console_writeln(info.name);
    }
}

static void demo_process(void)
{
    /* Paso local a cada ejecucion: se conserva en su contexto entre dos run. */
    for (uint32_t step = 1u; step <= 5u; ++step) {
        console_write("[PID ");
        console_uint(process_current_pid());
        console_write("] paso ");
        console_uint(step);
        console_writeln("/5");
        if (step < 5u) process_yield();
    }
    console_writeln("Proceso terminado.");
}

static void cmd_spawn(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "spawn nombre")) return;
    uint32_t pid = process_spawn(argv[1], demo_process);
    if (pid == 0u) {
        console_writeln("Error: nombre de 1 a 23 caracteres y maximo 7 procesos activos.");
        return;
    }
    console_write("Proceso creado. PID: ");
    console_uint(pid);
    console_writeln(" (READY). Usa run PID para ejecutarlo.");
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

static void cmd_run(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "run PID")) return;
    uint32_t pid;
    if (!parse_pid(argv[1], &pid) || !process_run(pid)) {
        console_writeln("Error: PID invalido, inexistente, terminado o reservado al shell.");
    }
}

static void cmd_kill(size_t argc, char **argv)
{
    if (!require_args(argc, 2u, "kill PID")) return;
    uint32_t pid;
    if (!parse_pid(argv[1], &pid) || !process_kill(pid)) {
        console_writeln("Error: solo se puede terminar un proceso READY distinto del shell.");
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
    const char *content;
    size_t size;
    vfs_result_t result = vfs_read(argv[1], &content, &size);
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
            /* STI + HLT evita perder una IRQ entre comprobar y dormir. */
            interrupts_wait();
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
