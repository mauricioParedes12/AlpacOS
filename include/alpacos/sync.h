#ifndef ALPACOS_SYNC_H
#define ALPACOS_SYNC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <alpacos/process.h>
#define MUTEX_NO_OWNER UINT32_MAX
#define MUTEX_INITIALIZER { MUTEX_NO_OWNER, {0u}, 0u, 0u }
#define SYNC_MAX_OBJECTS 48u
/* Los objetos deben vivir en memoria compartida del kernel hasta destroy(). */
typedef struct {
    uint32_t owner_tid;
    uint32_t waiters[THREAD_MAX];
    size_t wait_head, wait_count;
} mutex_t;
typedef struct {
    uint32_t value, limit;
    uint32_t waiters[THREAD_MAX];
    size_t wait_head, wait_count;
} semaphore_t;
/* init/destroy rechazan un objeto ocupado; init registra como maximo 48. */
bool mutex_init(mutex_t *mutex);
bool mutex_destroy(mutex_t *mutex);
bool mutex_lock(mutex_t *mutex);
bool mutex_unlock(mutex_t *mutex);
bool mutex_cancel_wait(uint32_t tid);
bool semaphore_init(semaphore_t *semaphore, uint32_t initial, uint32_t limit);
bool semaphore_destroy(semaphore_t *semaphore);
bool semaphore_wait(semaphore_t *semaphore);
bool semaphore_post(semaphore_t *semaphore);
/* El scheduler llama con el hilo ya TERMINATED e IRQ desactivadas. */
void sync_thread_cleanup(uint32_t tid);
#endif
