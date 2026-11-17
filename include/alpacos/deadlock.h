#ifndef ALPACOS_DEADLOCK_H
#define ALPACOS_DEADLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <alpacos/sync.h>

/* Registra una arista TID -> propietario del mutex.
 * Devuelve true si la nueva arista cierra un ciclo o no puede registrarse.
 */
bool deadlock_wait_begin(uint32_t tid, const mutex_t *mutex);
void deadlock_wait_end(uint32_t tid);

/* Solo la demostracion mantiene el ciclo hasta que el shell elija una victima.
 * En otros casos, mutex_lock devuelve false para impedir el deadlock.
 */
void deadlock_wait_cycle(uint32_t tid);

/* Muestra el estado del grafo y ejecuta la demostracion desde el shell. */
void deadlock_command(size_t argc, char **argv);

#endif
