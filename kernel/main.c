#include <alpacos/bootinfo.h>
#include <alpacos/console.h>
#include <alpacos/drivers.h>
#include <alpacos/interrupts.h>
#include <alpacos/memory.h>
#include <alpacos/io.h>
#include <alpacos/process.h>
#include <alpacos/shell.h>
#include <alpacos/timer.h>
#include <alpacos/version.h>
#include <alpacos/vfs.h>

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
    const boot_info_t *boot = bootinfo_get();
    interrupts_init();
    if (!memory_init(boot)) {
        console_set_color(0x0C);
        console_writeln("PANIC: no se pudo inicializar el gestor de memoria/paginacion.");
        cpu_halt();
    }
    serial_enable_interrupts();
    timer_init();
    bool keyboard_ready = keyboard_init();
    vfs_init();
    process_init();
    console_writeln("[OK] Multiboot, GDT, stack y kernel C inicializados.");
    console_writeln("[OK] IDT, excepciones y controlador PIC inicializados.");
    console_writeln("[OK] Temporizador PIT a unos 100 Hz (IRQ0).");
    console_writeln(keyboard_ready ? "[OK] Teclado PS/2 US por interrupciones (IRQ1)."
                                  : "[AVISO] Teclado PS/2 no disponible. Usa la terminal serie.");
    console_writeln("[OK] Consola VGA y puerto serie.");
    console_writeln("[OK] VFS con montajes; RAMFS montado en / (no persistente).");
    console_writeln("[OK] Procesos y Round Robin por hilos: quantum de 10 ticks (~100 ms).");
    console_writeln("[OK] Hilos, idle, sleep, mutex y semaforos con colas de espera.");
    console_writeln("[OK] Marcos de 4 KiB, mapa de RAM y paginacion x86 activa.");
    console_writeln("[OK] Regiones anonimas y paginas asignadas bajo demanda.");
    console_writeln("[OK] Reemplazo FIFO por region con respaldo en RAM (sin swap en disco).");
    console_writeln("[OK] Deteccion y recuperacion de deadlocks.");
    console_writeln("[OK] Directorio de paginas propio por proceso (ejecucion en ring 0).");
    console_writeln("Escribe help para ver los comandos.\n");
    interrupts_enable();
    shell_run();
}
