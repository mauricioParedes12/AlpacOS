#ifndef ALPACOS_PROCESS_H
#define ALPACOS_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <alpacos/interrupts.h>

#define PROCESS_MAX 8u /* Shell (PID 0) y hasta siete procesos adicionales. */
#define THREAD_MAX 24u /* Incluye el hilo del shell. */
#define PROCESS_NAME_CAPACITY 24u
#define THREAD_NAME_CAPACITY 24u
#define PROCESS_QUANTUM_TICKS 10u /* PIT a 100 Hz: aproximadamente 100 ms. */

typedef enum {
    PROCESS_UNUSED,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef enum {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef void (*process_entry_t)(void);
typedef process_entry_t thread_entry_t;

typedef struct {
    uint32_t pid;
    char name[PROCESS_NAME_CAPACITY];
    process_state_t state;
    uint32_t cpu_ticks;
    uint32_t turns;
    uint32_t thread_count;
} process_info_t;

typedef struct {
    uint32_t tid;
    uint32_t pid;
    char name[THREAD_NAME_CAPACITY];
    thread_state_t state;
    uint32_t cpu_ticks;
    uint32_t turns;
} thread_info_t;

void process_init(void);
/* Crea un proceso y su primer hilo. Cero indica parametros/cupo invalidos. */
uint32_t process_spawn(const char *name, process_entry_t entry);
/* Crea otro hilo dentro de un proceso existente. Cero indica error. */
uint32_t thread_spawn(uint32_t pid, const char *name, thread_entry_t entry);
/* Se llama desde IRQ0 o int 0x80, con interrupciones desactivadas. */
interrupt_frame_t *process_schedule(interrupt_frame_t *frame, bool timer_tick);
bool process_kill(uint32_t pid);
bool process_get_info(size_t slot, process_info_t *info);
bool thread_get_info(size_t slot, thread_info_t *info);
uint32_t process_current_pid(void);
uint32_t thread_current_tid(void);
uint32_t process_cpu_ticks(void);
/* Bloquea el hilo actual si existe otro hilo listo; thread_unblock lo reactiva. */
bool thread_block_current(void);
bool thread_unblock(uint32_t tid);
/* Cede el resto del turno voluntariamente. */
void process_yield(void);
_Noreturn void thread_exit(void);
_Noreturn void process_exit(void);

#endif
