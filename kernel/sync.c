#include <alpacos/interrupts.h>
#include <alpacos/process.h>
#include <alpacos/sync.h>

void mutex_init(mutex_t *mutex)
{
    if (mutex == NULL) return;
    uint32_t flags = interrupts_save();
    mutex->owner_tid = MUTEX_NO_OWNER;
    mutex->wait_head = 0u;
    mutex->wait_count = 0u;
    for (size_t i = 0u; i < THREAD_MAX; ++i) mutex->waiters[i] = 0u;
    interrupts_restore(flags);
}

bool mutex_lock(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save();
    uint32_t tid = thread_current_tid();

    if (mutex->owner_tid == MUTEX_NO_OWNER) {
        mutex->owner_tid = tid;
        interrupts_restore(flags);
        return true;
    }
    if (mutex->owner_tid == tid || mutex->wait_count == THREAD_MAX) {
        interrupts_restore(flags);
        return false;
    }

    size_t tail = (mutex->wait_head + mutex->wait_count) % THREAD_MAX;
    mutex->waiters[tail] = tid;
    ++mutex->wait_count;

    if (!thread_block_current()) {
        /* No hay otro hilo listo: retirar la espera y evitar quedar bloqueado. */
        --mutex->wait_count;
        interrupts_restore(flags);
        return false;
    }

    /* mutex_unlock asigna la propiedad antes de despertar este hilo. */
    bool acquired = mutex->owner_tid == tid;
    interrupts_restore(flags);
    return acquired;
}

bool mutex_unlock(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save();
    uint32_t tid = thread_current_tid();
    if (mutex->owner_tid != tid) {
        interrupts_restore(flags);
        return false;
    }

    mutex->owner_tid = MUTEX_NO_OWNER;
    while (mutex->wait_count > 0u) {
        uint32_t next_tid = mutex->waiters[mutex->wait_head];
        mutex->waiters[mutex->wait_head] = 0u;
        mutex->wait_head = (mutex->wait_head + 1u) % THREAD_MAX;
        --mutex->wait_count;
        if (thread_unblock(next_tid)) {
            /* Transferencia directa: no hay ventana para que otro hilo lo tome. */
            mutex->owner_tid = next_tid;
            break;
        }
    }
    interrupts_restore(flags);
    return true;
}
