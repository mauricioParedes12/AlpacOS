#include <alpacos/kstring.h>
#include <alpacos/ramfs.h>
#include <alpacos/vfs.h>
#include <stdint.h>
#ifndef ALPACOS_HOST_TEST
#include <alpacos/interrupts.h>
#else
static uint32_t interrupts_save(void) { return 0; }
static void interrupts_restore(uint32_t flags) { (void)flags; }
#endif

typedef struct {
    bool used;
    char path[VFS_PATH_CAPACITY];
    const vfs_driver_t *driver;
    void *instance;
    vfs_node_id_t root;
} vfs_mount_t;

/* Un vnode identifica un nodo junto con el montaje al que pertenece. */
typedef struct {
    vfs_mount_t *mount;
    vfs_node_id_t id;
} vfs_node_t;

static vfs_mount_t mounts[VFS_MAX_MOUNTS];

static bool valid_path(const char *path)
{
    if (path == NULL || path[0] != '/') return false;
    size_t length = 0u;
    while (path[length] != '\0') {
        if (++length >= VFS_PATH_CAPACITY) return false;
    }
    if (length == 1u) return true;
    if (path[length - 1u] == '/') return false;
    for (size_t start = 1u; start < length;) {
        size_t end = start;
        while (end < length && path[end] != '/') {
            char ch = path[end++];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                return false;
        }
        size_t size = end - start;
        if (size == 0u || size >= VFS_NAME_CAPACITY ||
            (size == 1u && path[start] == '.') ||
            (size == 2u && path[start] == '.' && path[start + 1u] == '.'))
            return false;
        start = end + 1u;
    }
    return true;
}

/* Compara componentes completos: /mnt no contiene /mnt2. */
static bool within(const char *path, const char *ancestor)
{
    size_t length = kstrlen(ancestor);
    if (length == 1u) return true; /* raiz */
    return kstrlen(path) >= length && memcmp(path, ancestor, length) == 0 &&
           (path[length] == '\0' || path[length] == '/');
}

static vfs_result_t node_stat(const vfs_node_t *node, vfs_stat_t *info)
{
    return node->mount->driver->operations->stat(node->mount->instance, node->id, info);
}

static vfs_result_t resolve(const char *path, vfs_node_t *node)
{
    if (!valid_path(path)) return VFS_ERR_INVALID_PATH;
    vfs_mount_t *best = NULL;
    size_t longest = 0u;
    for (size_t i = 0u; i < VFS_MAX_MOUNTS; ++i) {
        if (!mounts[i].used || !within(path, mounts[i].path)) continue;
        size_t length = kstrlen(mounts[i].path);
        if (length > longest) { best = &mounts[i]; longest = length; }
    }
    if (best == NULL) return VFS_ERR_NOT_FOUND;
    node->mount = best;
    node->id = best->root;
    const char *part = path + longest;
    if (*part == '/') ++part;
    while (*part != '\0') {
        const char *end = part;
        while (*end != '\0' && *end != '/') ++end;
        vfs_stat_t info;
        vfs_result_t result = node_stat(node, &info);
        if (result != VFS_OK) return result;
        if (!info.is_directory) return VFS_ERR_NOT_DIR;
        result = best->driver->operations->lookup(best->instance, node->id,
                                                  part, (size_t)(end - part), &node->id);
        if (result != VFS_OK) return result;
        part = *end == '/' ? end + 1 : end;
    }
    return VFS_OK;
}

static bool covers_mount(const char *path)
{
    for (size_t i = 0u; i < VFS_MAX_MOUNTS; ++i) {
        if (mounts[i].used && within(mounts[i].path, path)) return true;
    }
    return false;
}

static bool valid_driver(const vfs_driver_t *driver)
{
    if (driver == NULL || driver->name == NULL || driver->name[0] == '\0' ||
        driver->mount == NULL || driver->unmount == NULL || driver->operations == NULL)
        return false;
    const vfs_operations_t *ops = driver->operations;
    return ops->lookup != NULL && ops->stat != NULL && ops->create != NULL &&
           ops->read != NULL && ops->write != NULL && ops->list != NULL && ops->remove != NULL;
}

static vfs_result_t locked_mount(const char *path, const vfs_driver_t *driver)
{
    if (!valid_path(path)) return VFS_ERR_INVALID_PATH;
    if (!valid_driver(driver)) return VFS_ERR_INVALID_FS;
    /* Impide duplicados y ocultar montajes ya existentes bajo este destino. */
    if (covers_mount(path)) return VFS_ERR_BUSY;
    if (!kstreq(path, "/")) {
        vfs_node_t target;
        vfs_stat_t info;
        vfs_result_t result = resolve(path, &target);
        if (result != VFS_OK) return result;
        result = node_stat(&target, &info);
        if (result != VFS_OK) return result;
        if (!info.is_directory) return VFS_ERR_NOT_DIR;
    }
    size_t slot = 0u;
    while (slot < VFS_MAX_MOUNTS && mounts[slot].used) ++slot;
    if (slot == VFS_MAX_MOUNTS) return VFS_ERR_MOUNT_FULL;
    void *instance = NULL;
    vfs_node_id_t root = 0u;
    vfs_result_t result = driver->mount(&instance, &root);
    if (result != VFS_OK) return result;
    vfs_stat_t info;
    result = driver->operations->stat(instance, root, &info);
    if (result != VFS_OK || !info.is_directory) {
        driver->unmount(instance);
        return result != VFS_OK ? result : VFS_ERR_NOT_DIR;
    }
    mounts[slot].driver = driver;
    mounts[slot].instance = instance;
    mounts[slot].root = root;
    memcpy(mounts[slot].path, path, kstrlen(path) + 1u);
    mounts[slot].used = true;
    return VFS_OK;
}

static vfs_result_t locked_unmount(const char *path)
{
    if (!valid_path(path)) return VFS_ERR_INVALID_PATH;
    if (kstreq(path, "/")) return VFS_ERR_ROOT;
    vfs_mount_t *target = NULL;
    for (size_t i = 0u; i < VFS_MAX_MOUNTS; ++i) {
        if (!mounts[i].used) continue;
        if (kstreq(path, mounts[i].path)) target = &mounts[i];
        else if (within(mounts[i].path, path)) return VFS_ERR_BUSY;
    }
    if (target == NULL) return VFS_ERR_NO_MOUNT;
    target->driver->unmount(target->instance);
    memset(target, 0, sizeof(*target));
    return VFS_OK;
}

static vfs_result_t create(const char *path, bool directory)
{
    if (!valid_path(path)) return VFS_ERR_INVALID_PATH;
    if (kstreq(path, "/")) return VFS_ERR_ROOT;
    size_t leaf = kstrlen(path);
    while (path[leaf - 1u] != '/') --leaf;
    char parent_path[VFS_PATH_CAPACITY];
    size_t length = leaf == 1u ? 1u : leaf - 1u;
    memcpy(parent_path, path, length);
    parent_path[length] = '\0';
    vfs_node_t parent;
    vfs_result_t result = resolve(parent_path, &parent);
    if (result != VFS_OK) return result;
    vfs_stat_t info;
    result = node_stat(&parent, &info);
    if (result != VFS_OK) return result;
    if (!info.is_directory) return VFS_ERR_NOT_DIR;
    return parent.mount->driver->operations->create(parent.mount->instance, parent.id,
                                                     path + leaf, kstrlen(path + leaf), directory);
}

static vfs_result_t remove_node(const char *path, bool directory)
{
    if (!valid_path(path)) return VFS_ERR_INVALID_PATH;
    if (kstreq(path, "/")) return VFS_ERR_ROOT;
    if (covers_mount(path)) return VFS_ERR_BUSY;
    vfs_node_t node;
    vfs_result_t result = resolve(path, &node);
    if (result != VFS_OK) return result;
    return node.mount->driver->operations->remove(node.mount->instance, node.id, directory);
}

static vfs_result_t locked_mkdir(const char *path) { return create(path, true); }
static vfs_result_t locked_touch(const char *path) { return create(path, false); }
static vfs_result_t locked_unlink(const char *path) { return remove_node(path, false); }
static vfs_result_t locked_rmdir(const char *path) { return remove_node(path, true); }

static vfs_result_t locked_read(const char *path, const char **text, size_t *size)
{
    if (text == NULL || size == NULL) return VFS_ERR_INVALID_PATH;
    vfs_node_t node;
    vfs_result_t result = resolve(path, &node);
    if (result != VFS_OK) return result;
    return node.mount->driver->operations->read(node.mount->instance, node.id, text, size);
}

void vfs_init(void)
{
    uint32_t flags = interrupts_save();
    for (size_t i = VFS_MAX_MOUNTS; i > 0u; --i) {
        if (mounts[i - 1u].used)
            mounts[i - 1u].driver->unmount(mounts[i - 1u].instance);
    }
    memset(mounts, 0, sizeof(mounts));
    /* Unica politica de arranque: la raiz usa RAMFS. Todas las operaciones
       posteriores se despachan mediante el descriptor del montaje. */
    (void)locked_mount("/", &ramfs_driver);
    interrupts_restore(flags);
}

vfs_result_t vfs_mount(const char *path, const vfs_driver_t *driver)
{
    uint32_t flags = interrupts_save();
    vfs_result_t result = locked_mount(path, driver);
    interrupts_restore(flags);
    return result;
}

#define VFS_SINGLE(name) \
    vfs_result_t vfs_##name(const char *path) { \
        uint32_t flags = interrupts_save(); \
        vfs_result_t result = locked_##name(path); \
        interrupts_restore(flags); return result; \
    }
VFS_SINGLE(mkdir)
VFS_SINGLE(touch)
VFS_SINGLE(unlink)
VFS_SINGLE(rmdir)
VFS_SINGLE(unmount)

vfs_result_t vfs_stat(const char *path, vfs_stat_t *info)
{
    if (info == NULL) return VFS_ERR_INVALID_PATH;
    uint32_t flags = interrupts_save();
    vfs_node_t node;
    vfs_result_t result = resolve(path, &node);
    if (result == VFS_OK) result = node_stat(&node, info);
    interrupts_restore(flags);
    return result;
}

vfs_result_t vfs_write(const char *path, const char *text)
{
    if (text == NULL) return VFS_ERR_INVALID_PATH;
    uint32_t flags = interrupts_save();
    vfs_node_t node;
    vfs_result_t result = resolve(path, &node);
    if (result == VFS_OK)
        result = node.mount->driver->operations->write(node.mount->instance, node.id, text);
    interrupts_restore(flags);
    return result;
}

vfs_result_t vfs_read(const char *path, const char **text, size_t *size)
{
    uint32_t flags = interrupts_save();
    vfs_result_t result = locked_read(path, text, size);
    interrupts_restore(flags);
    return result;
}

vfs_result_t vfs_read_copy(const char *path, char *buffer, size_t capacity, size_t *size)
{
    if (buffer == NULL || size == NULL) return VFS_ERR_INVALID_PATH;
    uint32_t flags = interrupts_save();
    const char *text;
    size_t length;
    vfs_result_t result = locked_read(path, &text, &length);
    if (result == VFS_OK) {
        if (length >= capacity) result = VFS_ERR_TOO_LARGE;
        else { memmove(buffer, text, length + 1u); *size = length; }
    }
    interrupts_restore(flags);
    return result;
}

vfs_result_t vfs_list(const char *path, vfs_list_callback_t callback, void *context)
{
    if (callback == NULL) return VFS_ERR_INVALID_PATH;
    uint32_t flags = interrupts_save();
    vfs_node_t node;
    vfs_result_t result = resolve(path, &node);
    if (result == VFS_OK)
        result = node.mount->driver->operations->list(node.mount->instance, node.id, callback, context);
    interrupts_restore(flags);
    return result;
}

vfs_result_t vfs_list_mounts(vfs_mount_callback_t callback, void *context)
{
    if (callback == NULL) return VFS_ERR_INVALID_PATH;
    uint32_t flags = interrupts_save();
    for (size_t i = 0u; i < VFS_MAX_MOUNTS; ++i) {
        if (mounts[i].used) callback(mounts[i].path, mounts[i].driver->name, context);
    }
    interrupts_restore(flags);
    return VFS_OK;
}

const char *vfs_result_message(vfs_result_t result)
{
    switch (result) {
        case VFS_OK: return "Operacion completada.";
        case VFS_ERR_INVALID_PATH: return "Ruta invalida (usa /dir/archivo; sin espacios, . o ..).";
        case VFS_ERR_NOT_FOUND: return "No existe el archivo o directorio.";
        case VFS_ERR_EXISTS: return "Ya existe un elemento con ese nombre.";
        case VFS_ERR_NOT_DIR: return "La ruta no corresponde a un directorio.";
        case VFS_ERR_IS_DIR: return "La ruta corresponde a un directorio.";
        case VFS_ERR_FULL: return "Sin entradas libres en el sistema de archivos.";
        case VFS_ERR_NOT_EMPTY: return "El directorio no esta vacio.";
        case VFS_ERR_TOO_LARGE: return "Contenido demasiado grande o buffer insuficiente.";
        case VFS_ERR_ROOT: return "No se puede modificar la raiz /.";
        case VFS_ERR_BUSY: return "Ruta ocupada: desmonta los sistemas montados en ella o debajo.";
        case VFS_ERR_MOUNT_FULL: return "Limite de montajes alcanzado (incluye la raiz).";
        case VFS_ERR_NO_MOUNT: return "No hay un montaje en esa ruta.";
        case VFS_ERR_INVALID_FS: return "Controlador de sistema de archivos invalido.";
    }
    return "Error VFS desconocido.";
}
