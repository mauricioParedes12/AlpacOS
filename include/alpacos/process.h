#ifndef ALPACOS_PROCESS_H
#define ALPACOS_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCESS_MAX 8u /* Shell (PID 0) y hasta siete procesos adicionales. */
#define PROCESS_NAME_CAPACITY 24u

typedef enum {
    PROCESS_UNUSED,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

typedef void (*process_entry_t)(void);

typedef struct {
    uint32_t pid;
    char name[PROCESS_NAME_CAPACITY];
    process_state_t state;
} process_info_t;

void process_init(void);
/* Devuelve un PID distinto de cero; cero indica nombre/entrada invalidos o cupo lleno. */
uint32_t process_spawn(const char *name, process_entry_t entry);
/* Solo desde el shell: ejecuta hasta yield o hasta que la funcion termine. */
bool process_run(uint32_t pid);
bool process_kill(uint32_t pid);
bool process_get_info(size_t slot, process_info_t *info);
uint32_t process_current_pid(void);
void process_yield(void);
_Noreturn void process_exit(void);

#endif
