#ifndef ALPACOS_VFS_H
#define ALPACOS_VFS_H

#include <stdbool.h>
#include <stddef.h>

/* API educativa de archivos de texto y rutas absolutas. */
#define VFS_MAX_NODES 32u      /* Limite por instancia del RAMFS incluido. */
#define VFS_MAX_MOUNTS 4u     /* Incluye el montaje raiz. */
#define VFS_NAME_CAPACITY 24u  /* Nombre de hasta 23 caracteres + NUL. */
#define VFS_FILE_CAPACITY 256u /* Texto de hasta 255 caracteres + NUL. */
#define VFS_PATH_CAPACITY 128u

typedef enum {
    VFS_OK,
    VFS_ERR_INVALID_PATH,
    VFS_ERR_NOT_FOUND,
    VFS_ERR_EXISTS,
    VFS_ERR_NOT_DIR,
    VFS_ERR_IS_DIR,
    VFS_ERR_FULL,
    VFS_ERR_NOT_EMPTY,
    VFS_ERR_TOO_LARGE,
    VFS_ERR_ROOT,
    VFS_ERR_BUSY,
    VFS_ERR_MOUNT_FULL,
    VFS_ERR_NO_MOUNT,
    VFS_ERR_INVALID_FS
} vfs_result_t;

typedef void (*vfs_list_callback_t)(const char *name, bool is_directory,
                                    size_t size, void *context);

/* El identificador solo tiene sentido dentro de una instancia del backend. */
typedef size_t vfs_node_id_t;
typedef struct {
    bool is_directory;
    size_t size;
} vfs_stat_t;

/* Las operaciones reciben nodos, no rutas globales. El VFS resuelve las rutas
   y serializa las llamadas en este kernel de una CPU. No deben bloquear,
   ceder la CPU ni volver a entrar al VFS. read entrega texto terminado en NUL. */
typedef struct {
    vfs_result_t (*lookup)(void *instance, vfs_node_id_t parent,
                           const char *name, size_t length, vfs_node_id_t *node);
    vfs_result_t (*stat)(void *instance, vfs_node_id_t node, vfs_stat_t *info);
    vfs_result_t (*create)(void *instance, vfs_node_id_t parent,
                           const char *name, size_t length, bool directory);
    vfs_result_t (*read)(void *instance, vfs_node_id_t node,
                         const char **text, size_t *size);
    vfs_result_t (*write)(void *instance, vfs_node_id_t node, const char *text);
    vfs_result_t (*list)(void *instance, vfs_node_id_t node,
                         vfs_list_callback_t callback, void *context);
    vfs_result_t (*remove)(void *instance, vfs_node_id_t node, bool directory);
} vfs_operations_t;

/* Descriptor estatico: debe vivir mientras existan sus montajes. mount crea
   una instancia independiente; si falla, no debe dejar recursos reservados.
   unmount libera la instancia. La persistencia depende de cada backend. */
typedef struct {
    const char *name;
    const vfs_operations_t *operations;
    vfs_result_t (*mount)(void **instance, vfs_node_id_t *root);
    void (*unmount)(void *instance);
} vfs_driver_t;

typedef void (*vfs_mount_callback_t)(const char *path, const char *filesystem,
                                    void *context);

/* Arranque/reinicio: monta una instancia vacia de RAMFS en /. */
void vfs_init(void);
/* El destino debe existir y ser directorio; su contenido queda oculto hasta
   desmontar. No se puede ocultar, borrar ni desmontar un montaje con hijos. */
vfs_result_t vfs_mount(const char *path, const vfs_driver_t *driver);
vfs_result_t vfs_unmount(const char *path);
/* El callback no debe bloquear ni modificar el VFS. */
vfs_result_t vfs_list_mounts(vfs_mount_callback_t callback, void *context);
vfs_result_t vfs_stat(const char *path, vfs_stat_t *info);
vfs_result_t vfs_mkdir(const char *path);
vfs_result_t vfs_touch(const char *path);
vfs_result_t vfs_write(const char *path, const char *text);
/* Lectura prestada: vale hasta modificar, desmontar o reiniciar el VFS.
   Para compartir datos entre hilos, usar vfs_read_copy. */
vfs_result_t vfs_read(const char *path, const char **text, size_t *size);
vfs_result_t vfs_read_copy(const char *path, char *buffer, size_t capacity, size_t *size);
/* El callback de listado no debe bloquear ni modificar el VFS. */
vfs_result_t vfs_list(const char *path, vfs_list_callback_t callback, void *context);
vfs_result_t vfs_unlink(const char *path);
vfs_result_t vfs_rmdir(const char *path);
const char *vfs_result_message(vfs_result_t result);

#endif
