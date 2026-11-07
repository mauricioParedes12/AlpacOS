#include <alpacos/kstring.h>
#include <alpacos/vfs.h>

/* Cada entrada ocupa un espacio fijo. El indice 0 es la raiz "/". */
typedef struct {
    bool used;
    bool directory;
    size_t parent;
    char name[VFS_NAME_CAPACITY];
    size_t size;
    char text[VFS_FILE_CAPACITY];
} vfs_node_t;

static vfs_node_t nodes[VFS_MAX_NODES];

static bool valid_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
}

/* Rechaza rutas relativas, vacias, terminadas en '/', '.' y '..'. */
static bool valid_name(const char *name, size_t length)
{
    if (length == 0u || length >= VFS_NAME_CAPACITY) return false;
    if (length == 1u && name[0] == '.') return false;
    if (length == 2u && name[0] == '.' && name[1] == '.') return false;
    for (size_t i = 0; i < length; ++i) {
        if (!valid_char(name[i])) return false;
    }
    return true;
}

static size_t child_of(size_t parent, const char *name, size_t length)
{
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        if (nodes[i].used && nodes[i].parent == parent &&
            kstrlen(nodes[i].name) == length &&
            memcmp(nodes[i].name, name, length) == 0) {
            return i;
        }
    }
    return VFS_MAX_NODES;
}

/* En modo parent_only entrega el padre y el ultimo segmento sin copiarlo. */
static vfs_result_t resolve(const char *path, bool parent_only,
                            size_t *index, const char **leaf, size_t *leaf_size)
{
    if (path == NULL || path[0] != '/') return VFS_ERR_INVALID_PATH;
    size_t path_length = 0u;
    while (path[path_length] != '\0') {
        if (++path_length >= VFS_PATH_CAPACITY) return VFS_ERR_INVALID_PATH;
    }
    if (path[1] == '\0') {
        if (parent_only) return VFS_ERR_ROOT;
        *index = 0u;
        return VFS_OK;
    }
    /* Primero se comprueba toda la sintaxis: una ruta mal formada no debe
       dar NOT_FOUND solo porque alguno de sus padres aun no existe. */
    if (path[path_length - 1u] == '/') return VFS_ERR_INVALID_PATH;
    for (size_t start = 1u; start < path_length;) {
        size_t end = start;
        while (end < path_length && path[end] != '/') ++end;
        if (!valid_name(path + start, end - start)) return VFS_ERR_INVALID_PATH;
        start = end + 1u;
    }

    size_t current = 0u;
    const char *part = path + 1;
    for (;;) {
        const char *end = part;
        while (*end != '\0' && *end != '/') ++end;
        size_t length = (size_t)(end - part);
        if (!valid_name(part, length)) return VFS_ERR_INVALID_PATH;
        bool last = *end == '\0';
        if (parent_only && last) {
            *index = current;
            *leaf = part;
            *leaf_size = length;
            return VFS_OK;
        }
        size_t next = child_of(current, part, length);
        if (next == VFS_MAX_NODES) return VFS_ERR_NOT_FOUND;
        current = next;
        if (last) {
            *index = current;
            return VFS_OK;
        }
        if (!nodes[current].directory) return VFS_ERR_NOT_DIR;
        part = end + 1;
        if (*part == '\0') return VFS_ERR_INVALID_PATH;
    }
}

void vfs_init(void)
{
    memset(nodes, 0, sizeof(nodes));
    nodes[0].used = true;
    nodes[0].directory = true;
}

static vfs_result_t create(const char *path, bool directory)
{
    size_t parent, name_length;
    const char *name;
    vfs_result_t result = resolve(path, true, &parent, &name, &name_length);
    if (result != VFS_OK) return result;
    if (child_of(parent, name, name_length) != VFS_MAX_NODES) return VFS_ERR_EXISTS;
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        if (!nodes[i].used) {
            memset(&nodes[i], 0, sizeof(nodes[i]));
            nodes[i].used = true;
            nodes[i].directory = directory;
            nodes[i].parent = parent;
            memcpy(nodes[i].name, name, name_length);
            nodes[i].name[name_length] = '\0';
            return VFS_OK;
        }
    }
    return VFS_ERR_FULL;
}

vfs_result_t vfs_mkdir(const char *path)
{
    return create(path, true);
}

vfs_result_t vfs_touch(const char *path)
{
    return create(path, false);
}

vfs_result_t vfs_write(const char *path, const char *text)
{
    size_t index;
    if (text == NULL) return VFS_ERR_INVALID_PATH;
    vfs_result_t result = resolve(path, false, &index, NULL, NULL);
    if (result != VFS_OK) return result;
    if (nodes[index].directory) return VFS_ERR_IS_DIR;
    size_t length = 0u;
    while (text[length] != '\0') {
        if (++length >= VFS_FILE_CAPACITY) return VFS_ERR_TOO_LARGE;
    }
    memcpy(nodes[index].text, text, length + 1u);
    nodes[index].size = length;
    return VFS_OK;
}

vfs_result_t vfs_read(const char *path, const char **text, size_t *size)
{
    size_t index;
    if (text == NULL || size == NULL) return VFS_ERR_INVALID_PATH;
    vfs_result_t result = resolve(path, false, &index, NULL, NULL);
    if (result != VFS_OK) return result;
    if (nodes[index].directory) return VFS_ERR_IS_DIR;
    *text = nodes[index].text;
    *size = nodes[index].size;
    return VFS_OK;
}

vfs_result_t vfs_list(const char *path, vfs_list_callback_t callback, void *context)
{
    size_t parent;
    if (callback == NULL) return VFS_ERR_INVALID_PATH;
    vfs_result_t result = resolve(path, false, &parent, NULL, NULL);
    if (result != VFS_OK) return result;
    if (!nodes[parent].directory) return VFS_ERR_NOT_DIR;
    for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
        if (nodes[i].used && nodes[i].parent == parent) {
            callback(nodes[i].name, nodes[i].directory, nodes[i].size, context);
        }
    }
    return VFS_OK;
}

static vfs_result_t remove_node(const char *path, bool directory)
{
    size_t index;
    vfs_result_t result = resolve(path, false, &index, NULL, NULL);
    if (result != VFS_OK) return result;
    if (index == 0u) return VFS_ERR_ROOT;
    if (directory && !nodes[index].directory) return VFS_ERR_NOT_DIR;
    if (!directory && nodes[index].directory) return VFS_ERR_IS_DIR;
    if (directory) {
        for (size_t i = 1u; i < VFS_MAX_NODES; ++i) {
            if (nodes[i].used && nodes[i].parent == index) return VFS_ERR_NOT_EMPTY;
        }
    }
    memset(&nodes[index], 0, sizeof(nodes[index]));
    return VFS_OK;
}

vfs_result_t vfs_unlink(const char *path)
{
    return remove_node(path, false);
}

vfs_result_t vfs_rmdir(const char *path)
{
    return remove_node(path, true);
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
        case VFS_ERR_TOO_LARGE: return "Archivo demasiado grande (maximo 255 caracteres).";
        case VFS_ERR_ROOT: return "No se puede modificar la raiz /.";
    }
    return "Error VFS desconocido.";
}
