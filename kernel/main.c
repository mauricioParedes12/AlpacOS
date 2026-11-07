#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/drivers.h>
#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <alpacos/shell.h>
#include <alpacos/timer.h>
#include <alpacos/version.h>

_Noreturn void kernel_main(uint32_t magic, const multiboot_info_prefix_t *info)
{
    /* Las IRQ se activan despues de instalar la IDT y los controladores. */
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
    interrupts_init();
    timer_init();
    bool keyboard_ready = keyboard_init();
    console_writeln("[OK] Multiboot, GDT, stack y kernel C inicializados.");
    console_writeln("[OK] IDT, excepciones y controlador PIC inicializados.");
    console_writeln("[OK] Temporizador PIT a unos 100 Hz (IRQ0).");
    console_writeln(keyboard_ready ? "[OK] Teclado PS/2 US por interrupciones (IRQ1)."
                                  : "[AVISO] Teclado PS/2 no disponible. Usa la terminal serie.");
    console_writeln("[OK] Consola VGA y puerto serie. Sin multitarea ni paginacion.");
    console_writeln("Escribe help para ver los comandos.\n");
    interrupts_enable();
    shell_run();
}
