#include <alpacos/interrupts.h>
#include <alpacos/process.h>
#include <alpacos/deadlock.h>
#include <alpacos/sync.h>
#include <alpacos/kstring.h>
static mutex_t *mutexes[SYNC_MAX_OBJECTS];
static semaphore_t *semaphores[SYNC_MAX_OBJECTS];
static bool register_mutex(mutex_t *mutex)
{
    size_t empty = SYNC_MAX_OBJECTS;
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) {
        if (mutexes[i] == mutex) return true;
        if (mutexes[i] == NULL && empty == SYNC_MAX_OBJECTS) empty = i;
    }
    if (empty == SYNC_MAX_OBJECTS) return false;
    mutexes[empty] = mutex;
    return true;
}
static bool registered_semaphore(semaphore_t *semaphore)
{
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) if (semaphores[i] == semaphore) return true;
    return false;
}
static uint32_t queue_pop(uint32_t *queue, size_t *head, size_t *count)
{
    uint32_t tid = queue[*head];
    *head = (*head + 1u) % THREAD_MAX;
    --*count;
    return tid;
}
static void queue_push(uint32_t *queue, size_t head, size_t *count, uint32_t tid)
{
    queue[(head + *count) % THREAD_MAX] = tid;
    ++*count;
}
static bool queue_remove(uint32_t *queue, size_t *head, size_t *count, uint32_t tid)
{
    size_t original = *count;
    bool found = false;
    for (size_t i = 0; i < original; ++i) {
        uint32_t other = queue_pop(queue, head, count);
        if (other == tid) found = true;
        else queue_push(queue, *head, count, other);
    }
    return found;
}
bool mutex_init(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save();
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) {
        if (mutexes[i] == mutex && (mutex->owner_tid != MUTEX_NO_OWNER || mutex->wait_count != 0u)) {
            interrupts_restore(flags); return false;
        }
    }
    bool result = register_mutex(mutex);
    if (result) *mutex = (mutex_t)MUTEX_INITIALIZER;
    interrupts_restore(flags);
    return result;
}
bool mutex_destroy(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save();
    bool result = mutex->owner_tid == MUTEX_NO_OWNER && mutex->wait_count == 0u;
    if (result) for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) if (mutexes[i] == mutex) mutexes[i] = NULL;
    interrupts_restore(flags);
    return result;
}
bool mutex_lock(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save(), tid = thread_current_tid();
    if (!register_mutex(mutex)) { interrupts_restore(flags); return false; }
    if (mutex->owner_tid == MUTEX_NO_OWNER) {
        mutex->owner_tid = tid;
        interrupts_restore(flags);
        return true;
    }
    if (mutex->owner_tid == tid || mutex->wait_count == THREAD_MAX) { interrupts_restore(flags); return false; }
    if (deadlock_wait_begin(tid, mutex) && !deadlock_wait_cycle(tid)) {
        deadlock_wait_end(tid);
        interrupts_restore(flags);
        return false;
    }
    queue_push(mutex->waiters, mutex->wait_head, &mutex->wait_count, tid);
    if (!thread_block_current()) {
        (void)queue_remove(mutex->waiters, &mutex->wait_head, &mutex->wait_count, tid);
        deadlock_wait_end(tid);
        interrupts_restore(flags);
        return false;
    }
    bool acquired = mutex->owner_tid == tid;
    deadlock_wait_end(tid);
    interrupts_restore(flags);
    return acquired;
}
static void handoff(mutex_t *mutex)
{
    mutex->owner_tid = MUTEX_NO_OWNER;
    while (mutex->wait_count != 0u) {
        uint32_t tid = queue_pop(mutex->waiters, &mutex->wait_head, &mutex->wait_count);
        deadlock_wait_end(tid);
        if (thread_unblock(tid)) { mutex->owner_tid = tid; break; }
    }
}
bool mutex_unlock(mutex_t *mutex)
{
    if (mutex == NULL) return false;
    uint32_t flags = interrupts_save();
    if (mutex->owner_tid != thread_current_tid()) { interrupts_restore(flags); return false; }
    handoff(mutex);
    interrupts_restore(flags);
    return true;
}
bool mutex_cancel_wait(uint32_t tid)
{
    uint32_t flags = interrupts_save();
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) {
        mutex_t *mutex = mutexes[i];
        if (mutex != NULL && queue_remove(mutex->waiters, &mutex->wait_head, &mutex->wait_count, tid)) {
            deadlock_wait_end(tid);
            bool result = thread_unblock(tid);
            interrupts_restore(flags);
            return result;
        }
    }
    interrupts_restore(flags);
    return false;
}
bool semaphore_init(semaphore_t *semaphore, uint32_t initial, uint32_t limit)
{
    if (semaphore == NULL || limit == 0u || initial > limit) return false;
    uint32_t flags = interrupts_save();
    size_t empty = SYNC_MAX_OBJECTS;
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) {
        if (semaphores[i] == semaphore) {
            if (semaphore->wait_count != 0u) { interrupts_restore(flags); return false; }
            empty = i; break;
        }
        if (semaphores[i] == NULL && empty == SYNC_MAX_OBJECTS) empty = i;
    }
    if (empty == SYNC_MAX_OBJECTS) { interrupts_restore(flags); return false; }
    memset(semaphore, 0, sizeof(*semaphore));
    semaphore->value = initial;
    semaphore->limit = limit;
    semaphores[empty] = semaphore;
    interrupts_restore(flags);
    return true;
}
bool semaphore_destroy(semaphore_t *semaphore)
{
    if (semaphore == NULL) return false;
    uint32_t flags = interrupts_save();
    if (semaphore->wait_count != 0u) { interrupts_restore(flags); return false; }
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) if (semaphores[i] == semaphore) semaphores[i] = NULL;
    interrupts_restore(flags);
    return true;
}
bool semaphore_wait(semaphore_t *semaphore)
{
    if (semaphore == NULL) return false;
    uint32_t flags = interrupts_save();
    if (!registered_semaphore(semaphore)) { interrupts_restore(flags); return false; }
    if (semaphore->value != 0u) { --semaphore->value; interrupts_restore(flags); return true; }
    if (semaphore->wait_count == THREAD_MAX) { interrupts_restore(flags); return false; }
    uint32_t tid = thread_current_tid();
    queue_push(semaphore->waiters, semaphore->wait_head, &semaphore->wait_count, tid);
    bool result = thread_block_current();
    if (!result) (void)queue_remove(semaphore->waiters, &semaphore->wait_head, &semaphore->wait_count, tid);
    interrupts_restore(flags);
    return result;
}
bool semaphore_post(semaphore_t *semaphore)
{
    if (semaphore == NULL) return false;
    uint32_t flags = interrupts_save();
    if (!registered_semaphore(semaphore)) { interrupts_restore(flags); return false; }
    while (semaphore->wait_count != 0u) {
        uint32_t tid = queue_pop(semaphore->waiters, &semaphore->wait_head, &semaphore->wait_count);
        if (thread_unblock(tid)) { interrupts_restore(flags); return true; }
    }
    if (semaphore->value == semaphore->limit) { interrupts_restore(flags); return false; }
    ++semaphore->value;
    interrupts_restore(flags);
    return true;
}
void sync_thread_cleanup(uint32_t tid)
{
    uint32_t flags = interrupts_save();
    deadlock_wait_end(tid);
    for (size_t i = 0; i < SYNC_MAX_OBJECTS; ++i) {
        if (mutexes[i] != NULL) {
            (void)queue_remove(mutexes[i]->waiters, &mutexes[i]->wait_head, &mutexes[i]->wait_count, tid);
            if (mutexes[i]->owner_tid == tid) handoff(mutexes[i]);
        }
        if (semaphores[i] != NULL)
            (void)queue_remove(semaphores[i]->waiters, &semaphores[i]->wait_head, &semaphores[i]->wait_count, tid);
    }
    interrupts_restore(flags);
}
