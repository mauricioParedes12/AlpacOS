#ifndef ALPACOS_SYNC_H
#define ALPACOS_SYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <alpacos/process.h>

#define MUTEX_NO_OWNER UINT32_MAX
#define MUTEX_INITIALIZER { MUTEX_NO_OWNER, {0u}, 0u, 0u }

typedef struct {
    uint32_t owner_tid;
    uint32_t waiters[THREAD_MAX];
    size_t wait_head;
    size_t wait_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
/* Bloquea el hilo si otro posee el mutex. No es recursivo. */
bool mutex_lock(mutex_t *mutex);
/* Libera el mutex y despierta al siguiente hilo en espera. */
bool mutex_unlock(mutex_t *mutex);

#endif
