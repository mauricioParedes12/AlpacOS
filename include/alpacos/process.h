#ifndef ALPACOS_PROCESS_H
#define ALPACOS_PROCESS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <alpacos/interrupts.h>
#include <alpacos/memory.h>
#define PROCESS_MAX 8u
#define THREAD_MAX 24u /* Shell, idle y hasta 22 hilos de trabajo. */
#define PROCESS_NAME_CAPACITY 24u
#define THREAD_NAME_CAPACITY 24u
#define PROCESS_QUANTUM_TICKS 10u
#define PROCESS_EXIT_KILLED (-1)
#define PROCESS_EXIT_PAGE_FAULT (-14)
typedef enum { PROCESS_UNUSED, PROCESS_READY, PROCESS_RUNNING, PROCESS_BLOCKED, PROCESS_TERMINATED } process_state_t;
typedef enum { THREAD_UNUSED, THREAD_READY, THREAD_RUNNING, THREAD_BLOCKED, THREAD_SLEEPING, THREAD_TERMINATED } thread_state_t;
typedef void (*process_entry_t)(void);
typedef process_entry_t thread_entry_t;
typedef struct {
    uint32_t pid;
    char name[PROCESS_NAME_CAPACITY];
    process_state_t state;
    uint32_t cpu_ticks, turns, thread_count;
    memory_space_t address_space;
    uintptr_t private_frame;
    int32_t exit_status;
} process_info_t;
typedef struct {
    uint32_t tid, pid;
    char name[THREAD_NAME_CAPACITY];
    thread_state_t state;
    uint32_t cpu_ticks, turns;
} thread_info_t;
void process_init(void);
uint32_t process_spawn(const char *name, process_entry_t entry);
uint32_t thread_spawn(uint32_t pid, const char *name, thread_entry_t entry);
interrupt_frame_t *process_schedule(interrupt_frame_t *frame, bool timer_tick);
bool process_kill(uint32_t pid);
bool process_get_info(size_t slot, process_info_t *info);
bool process_find_info(uint32_t pid, process_info_t *info);
bool thread_get_info(size_t slot, thread_info_t *info);
uint32_t process_current_pid(void);
uint32_t thread_current_tid(void);
uint32_t process_cpu_ticks(void);
uint32_t process_ticks(void);
bool thread_block_current(void);
bool thread_unblock(uint32_t tid);
void process_yield(void);
/* Maximo INT32_MAX ticks; cero equivale a yield. */
void thread_sleep(uint32_t ticks);
_Noreturn void thread_exit(void);
/* Finaliza TODOS los hilos, cancela esperas y devuelve sus paginas. */
_Noreturn void process_exit(void);
_Noreturn void process_exit_with_status(int32_t status);
#endif
