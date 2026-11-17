#ifndef ALPACOS_RAMFS_H
#define ALPACOS_RAMFS_H

#include <alpacos/vfs.h>

/* Cada montaje reserva su propio arbol en RAM. Sus datos se pierden al
   desmontarlo o reiniciar. El VFS administra la vida de estas instancias. */
extern const vfs_driver_t ramfs_driver;

#endif
