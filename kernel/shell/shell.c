#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/io.h>
#include <alpacos/kstring.h>
#include <alpacos/parser.h>
#include <alpacos/shell.h>
#include <alpacos/version.h>
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
static void cmd_meminfo(size_t argc, char **argv);
static void cmd_modules(size_t argc, char **argv);
static void cmd_halt(size_t argc, char **argv);

static const command_t commands[] = {
    {"help", "Lista los comandos disponibles", cmd_help},
    {"clear", "Limpia la consola", cmd_clear},
    {"echo", "Muestra los argumentos separados por espacios", cmd_echo},
    {"version", "Muestra la version y arquitectura", cmd_version},
    {"meminfo", "Muestra datos del arranque, no memoria libre", cmd_meminfo},
    {"modules", "Muestra el estado real de los modulos", cmd_modules},
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
    console_writeln("[BASE] Shell y E/S VGA, PS/2 US, COM1 por sondeo");
    console_writeln("[TODO] IDT, excepciones, IRQ y temporizador");
    console_writeln("[TODO] Procesos, PCB y cambio de contexto");
    console_writeln("[TODO] Planificacion de CPU");
    console_writeln("[TODO] Threads y sincronizacion");
    console_writeln("[TODO] Memoria y paginacion");
    console_writeln("[TODO] Sistema de archivos virtual");
    console_writeln("[TODO] Deteccion/gestion de deadlocks");
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
        int input = console_read();
        if (input < 0) {
            /* Busy polling is temporary: replace after IDT + IRQ are ready. */
            cpu_relax();
            continue;
        }
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
