#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/io.h>
#include <alpacos/shell.h>
#include <alpacos/version.h>

_Noreturn void kernel_main(uint32_t magic, const multiboot_info_prefix_t *info)
{
    /* Keep IRQs masked until an IDT and interrupt handlers are implemented. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    console_init();
    console_set_color(0x0B);
    console_writeln("============================================================");
    console_writeln("  AlpacOS  |  Mini sistema operativo educativo");
    console_writeln("============================================================");
    console_set_color(0x07);
    console_writeln("Version " ALPACOS_VERSION " - " ALPACOS_ARCH);
    if (!bootinfo_init(magic, info)) {
        console_set_color(0x0C);
        console_writeln("PANIC: se requiere un cargador Multiboot v1.");
        cpu_halt();
    }
    console_writeln("[OK] Multiboot, GDT, stack y kernel C inicializados.");
    console_writeln("[OK] Consola VGA, puerto serie y entrada por sondeo.");
    console_writeln("Base inicial: sin multitarea ni paginacion todavia.");
    console_writeln("Escribe help para ver los comandos.\n");
    shell_run();
}
