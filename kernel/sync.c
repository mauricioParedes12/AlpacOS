#include <alpacos/interrupts.h>
#include <alpacos/process.h>
#include <alpacos/deadlock.h>
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

    /* Si la espera cierra un ciclo, no se mete al hilo en la cola del mutex.
     * En la demo queda bloqueado para mostrar el ciclo; fuera de ella
     * se cancela esta solicitud y el llamador puede liberar sus recursos. */
    if (deadlock_wait_begin(tid, mutex)) {
        deadlock_wait_cycle(tid);
        deadlock_wait_end(tid);
        interrupts_restore(flags);
        return false;
    }

    size_t tail = (mutex->wait_head + mutex->wait_count) % THREAD_MAX;
    mutex->waiters[tail] = tid;
    ++mutex->wait_count;

    if (!thread_block_current()) {
        /* No hay otro hilo listo: retirar la espera y evitar quedar bloqueado. */
        --mutex->wait_count;
        deadlock_wait_end(tid);
        interrupts_restore(flags);
        return false;
    }

    /* mutex_unlock asigna la propiedad antes de despertar este hilo. */
    bool acquired = mutex->owner_tid == tid;
    deadlock_wait_end(tid);
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
            deadlock_wait_end(next_tid);
            /* Transferencia directa: no hay ventana para que otro hilo lo tome. */
            mutex->owner_tid = next_tid;
            break;
        }
    }
    interrupts_restore(flags);
    return true;
}
