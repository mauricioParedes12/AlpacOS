#ifndef ALPACOS_VFS_H
#define ALPACOS_VFS_H

#include <stdbool.h>
#include <stddef.h>

/* Sistema de archivos en RAM, sin montajes ni persistencia tras reiniciar. */
#define VFS_MAX_NODES 32u
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
    VFS_ERR_ROOT
} vfs_result_t;

typedef void (*vfs_list_callback_t)(const char *name, bool is_directory,
                                    size_t size, void *context);

void vfs_init(void);
vfs_result_t vfs_mkdir(const char *path);
vfs_result_t vfs_touch(const char *path);
vfs_result_t vfs_write(const char *path, const char *text);
vfs_result_t vfs_read(const char *path, const char **text, size_t *size);
vfs_result_t vfs_list(const char *path, vfs_list_callback_t callback, void *context);
vfs_result_t vfs_unlink(const char *path);
vfs_result_t vfs_rmdir(const char *path);
const char *vfs_result_message(vfs_result_t result);

#endif
