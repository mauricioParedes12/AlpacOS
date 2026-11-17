#include <alpacos/kstring.h>
#include <alpacos/ramfs.h>

typedef struct {
    bool used;
    bool directory;
    vfs_node_id_t parent;
    char name[VFS_NAME_CAPACITY];
    size_t size;
    char text[VFS_FILE_CAPACITY];
} ramfs_node_t;

typedef struct {
    bool used;
    ramfs_node_t nodes[VFS_MAX_NODES];
} ramfs_instance_t;

static ramfs_instance_t instances[VFS_MAX_MOUNTS];

static ramfs_node_t *get_node(void *instance, vfs_node_id_t id)
{
    ramfs_instance_t *fs = instance;
    if (fs == NULL || !fs->used || id >= VFS_MAX_NODES || !fs->nodes[id].used)
        return NULL;
    return &fs->nodes[id];
}

static vfs_result_t ramfs_mount(void **instance, vfs_node_id_t *root)
{
    for (size_t i = 0u; i < VFS_MAX_MOUNTS; ++i) {
        if (instances[i].used) continue;
        ramfs_instance_t *fs = &instances[i];
        memset(fs, 0, sizeof(*fs));
        fs->used = true;
        fs->nodes[0].used = true;
        fs->nodes[0].directory = true;
        *instance = fs;
        *root = 0u;
        return VFS_OK;
    }
    return VFS_ERR_FULL;
}

static void ramfs_unmount(void *instance)
{
    memset(instance, 0, sizeof(ramfs_instance_t));
}

static vfs_result_t ramfs_lookup(void *instance, vfs_node_id_t parent,
                                 const char *name, size_t length, vfs_node_id_t *id)
{
    ramfs_instance_t *fs = instance;
    ramfs_node_t *node = get_node(instance, parent);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    if (!node->directory) return VFS_ERR_NOT_DIR;
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        node = &fs->nodes[i];
        if (node->used && node->parent == parent && kstrlen(node->name) == length &&
            memcmp(node->name, name, length) == 0) {
            *id = i;
            return VFS_OK;
        }
    }
    return VFS_ERR_NOT_FOUND;
}

static vfs_result_t ramfs_stat(void *instance, vfs_node_id_t id, vfs_stat_t *info)
{
    ramfs_node_t *node = get_node(instance, id);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    info->is_directory = node->directory;
    info->size = node->size;
    return VFS_OK;
}

static vfs_result_t ramfs_create(void *instance, vfs_node_id_t parent,
                                 const char *name, size_t length, bool directory)
{
    ramfs_instance_t *fs = instance;
    if (length == 0u || length >= VFS_NAME_CAPACITY) return VFS_ERR_INVALID_PATH;
    vfs_node_id_t existing;
    vfs_result_t result = ramfs_lookup(instance, parent, name, length, &existing);
    if (result == VFS_OK) return VFS_ERR_EXISTS;
    if (result != VFS_ERR_NOT_FOUND) return result;
    /* El VFS entrega un padre valido, pero el backend tambien lo comprueba. */
    if (get_node(instance, parent) == NULL) return VFS_ERR_NOT_FOUND;
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        ramfs_node_t *node = &fs->nodes[i];
        if (node->used) continue;
        memset(node, 0, sizeof(*node));
        node->used = true;
        node->directory = directory;
        node->parent = parent;
        memcpy(node->name, name, length);
        return VFS_OK;
    }
    return VFS_ERR_FULL;
}

static vfs_result_t ramfs_write(void *instance, vfs_node_id_t id, const char *text)
{
    ramfs_node_t *node = get_node(instance, id);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    if (node->directory) return VFS_ERR_IS_DIR;
    size_t length = 0u;
    while (text[length] != '\0') {
        if (++length >= VFS_FILE_CAPACITY) return VFS_ERR_TOO_LARGE;
    }
    /* memmove admite escribir usando un puntero prestado del mismo archivo. */
    memmove(node->text, text, length + 1u);
    node->size = length;
    return VFS_OK;
}

static vfs_result_t ramfs_read(void *instance, vfs_node_id_t id,
                               const char **text, size_t *size)
{
    ramfs_node_t *node = get_node(instance, id);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    if (node->directory) return VFS_ERR_IS_DIR;
    *text = node->text;
    *size = node->size;
    return VFS_OK;
}

static vfs_result_t ramfs_list(void *instance, vfs_node_id_t parent,
                               vfs_list_callback_t callback, void *context)
{
    ramfs_instance_t *fs = instance;
    ramfs_node_t *node = get_node(instance, parent);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    if (!node->directory) return VFS_ERR_NOT_DIR;
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        node = &fs->nodes[i];
        if (node->used && node->parent == parent)
            callback(node->name, node->directory, node->size, context);
    }
    return VFS_OK;
}

static vfs_result_t ramfs_remove(void *instance, vfs_node_id_t id, bool directory)
{
    ramfs_instance_t *fs = instance;
    ramfs_node_t *node = get_node(instance, id);
    if (node == NULL) return VFS_ERR_NOT_FOUND;
    if (id == 0u) return VFS_ERR_ROOT;
    if (directory && !node->directory) return VFS_ERR_NOT_DIR;
    if (!directory && node->directory) return VFS_ERR_IS_DIR;
    if (directory) {
        for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
            if (fs->nodes[i].used && fs->nodes[i].parent == id)
                return VFS_ERR_NOT_EMPTY;
        }
    }
    memset(node, 0, sizeof(*node));
    return VFS_OK;
}

static const vfs_operations_t ramfs_operations = {
    .lookup = ramfs_lookup, .stat = ramfs_stat, .create = ramfs_create,
    .read = ramfs_read, .write = ramfs_write, .list = ramfs_list,
    .remove = ramfs_remove
};

const vfs_driver_t ramfs_driver = {
    .name = "ramfs", .operations = &ramfs_operations,
    .mount = ramfs_mount, .unmount = ramfs_unmount
};
