#include <alpacos/console.h>
#include <alpacos/interrupts.h>
#include <alpacos/io.h>
#include <stddef.h>

#define PIC_MASTER_COMMAND 0x20u
#define PIC_MASTER_DATA    0x21u
#define PIC_SLAVE_COMMAND  0xA0u
#define PIC_SLAVE_DATA     0xA1u
#define IRQ_BASE           32u

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t reserved;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_pointer_t;

/* Debe coincidir con el orden de guardado en interrupts.S. */
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, saved_esp, ebx, edx, ecx, eax;
    uint32_t vector, error;
    uint32_t eip, cs, eflags;
} interrupt_frame_t;

_Static_assert(sizeof(idt_entry_t) == 8, "Tamano de entrada IDT incorrecto");
_Static_assert(offsetof(interrupt_frame_t, vector) == 48, "Pila de IRQ incorrecta");

extern const uint32_t isr_stub_table[256];
static idt_entry_t idt[256];
static irq_handler_t irq_handlers[16];

static void pic_write(uint16_t port, uint8_t value)
{
    outb(port, value);
    outb(0x80, 0); /* Espera de E/S entre comandos del PIC. */
}

void interrupts_init(void)
{
    interrupts_disable();
    for (unsigned i = 0; i < 256u; ++i) {
        uint32_t address = isr_stub_table[i];
        idt[i].offset_low = (uint16_t)address;
        idt[i].selector = 0x08;
        idt[i].reserved = 0;
        idt[i].flags = 0x8E; /* Presente, ring 0, interrupt gate de 32 bits. */
        idt[i].offset_high = (uint16_t)(address >> 16);
    }
    idt_pointer_t pointer = {sizeof(idt) - 1u, (uint32_t)(uintptr_t)idt};
    __asm__ volatile ("lidt %0" : : "m"(pointer) : "memory");

    /* IRQ 0..15 pasan a los vectores 32..47, fuera de las excepciones. */
    pic_write(PIC_MASTER_COMMAND, 0x11);
    pic_write(PIC_SLAVE_COMMAND, 0x11);
    pic_write(PIC_MASTER_DATA, IRQ_BASE);
    pic_write(PIC_SLAVE_DATA, IRQ_BASE + 8u);
    pic_write(PIC_MASTER_DATA, 0x04);
    pic_write(PIC_SLAVE_DATA, 0x02);
    pic_write(PIC_MASTER_DATA, 0x01);
    pic_write(PIC_SLAVE_DATA, 0x01);
    pic_write(PIC_MASTER_DATA, 0xFF);
    pic_write(PIC_SLAVE_DATA, 0xFF);
}

void irq_register_handler(unsigned irq, irq_handler_t handler)
{
    if (irq >= 16u || handler == NULL) return;
    uint32_t flags = interrupts_save();
    irq_handlers[irq] = handler;
    uint16_t port = irq < 8u ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    uint8_t bit = (uint8_t)(1u << (irq % 8u));
    outb(port, (uint8_t)(inb(port) & (uint8_t)~bit));
    if (irq >= 8u) {
        outb(PIC_MASTER_DATA, (uint8_t)(inb(PIC_MASTER_DATA) & 0xFBu));
    }
    interrupts_restore(flags);
}

static const char *const exception_names[32] = {
    "Division por cero", "Depuracion", "Interrupcion no enmascarable",
    "Punto de interrupcion", "Desbordamiento", "Limites excedidos",
    "Instruccion invalida", "Dispositivo no disponible", "Doble fallo",
    "Segmento del coprocesador", "TSS invalida", "Segmento no presente",
    "Fallo de pila", "Proteccion general", "Fallo de pagina", "Reservada",
    "Error de FPU", "Alineacion", "Error de maquina", "Error SIMD",
    "Virtualizacion", "Proteccion de control", "Reservada", "Reservada",
    "Reservada", "Reservada", "Reservada", "Reservada",
    "Hipervisor", "Comunicacion virtual", "Seguridad", "Reservada"
};

static _Noreturn void exception_panic(const interrupt_frame_t *frame)
{
    console_set_color(0x0C);
    console_write("\nPANIC: ");
    console_write(frame->vector < 32u ? exception_names[frame->vector]
                                    : "Interrupcion inesperada");
    console_write(" (vector ");
    console_uint(frame->vector);
    console_writeln(")");
    console_write("EIP: ");
    console_hex(frame->eip);
    console_write("  Error: ");
    console_hex(frame->error);
    console_putc('\n');
    if (frame->vector == 14u) {
        uint32_t address;
        __asm__ volatile ("movl %%cr2, %0" : "=r"(address));
        console_write("Direccion de memoria: ");
        console_hex(address);
        console_putc('\n');
    }
    console_writeln("AlpacOS detenido. Reinicia QEMU para continuar.");
    cpu_halt();
}

void interrupts_dispatch(const interrupt_frame_t *frame)
{
    if (frame->vector < IRQ_BASE || frame->vector >= IRQ_BASE + 16u) {
        exception_panic(frame);
    }
    unsigned irq = frame->vector - IRQ_BASE;
    /* Las IRQ espurias 7 y 15 no deben confirmarse como IRQ reales. */
    if (irq == 7u) {
        outb(PIC_MASTER_COMMAND, 0x0B);
        if ((inb(PIC_MASTER_COMMAND) & 0x80u) == 0u) return;
    }
    if (irq == 15u) {
        outb(PIC_SLAVE_COMMAND, 0x0B);
        if ((inb(PIC_SLAVE_COMMAND) & 0x80u) == 0u) {
            outb(PIC_MASTER_COMMAND, 0x20);
            return;
        }
    }
    if (irq_handlers[irq] != NULL) irq_handlers[irq]();
    if (irq >= 8u) outb(PIC_SLAVE_COMMAND, 0x20);
    outb(PIC_MASTER_COMMAND, 0x20);
}
