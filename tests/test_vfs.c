#include <alpacos/vfs.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned items;
    unsigned directories;
    unsigned files;
    size_t total_bytes;
} list_info_t;

static void count_entry(const char *name, bool is_directory, size_t size, void *ctx)
{
    list_info_t *info = ctx;
    assert(name[0] != '\0');
    ++info->items;
    if (is_directory) ++info->directories;
    else {
        ++info->files;
        info->total_bytes += size;
    }
}

static list_info_t scan(const char *path)
{
    list_info_t info = {0};
    assert(vfs_list(path, count_entry, &info) == VFS_OK);
    return info;
}

static void test_lifecycle(void)
{
    vfs_init();
    assert(scan("/").items == 0u);
    assert(vfs_mkdir("/docs") == VFS_OK);
    assert(vfs_touch("/docs/hola.txt") == VFS_OK);
    assert(vfs_write("/docs/hola.txt", "Hola desde AlpacOS") == VFS_OK);
    const char *data = NULL;
    size_t size = 0u;
    assert(vfs_read("/docs/hola.txt", &data, &size) == VFS_OK);
    assert(size == strlen("Hola desde AlpacOS"));
    assert(strcmp(data, "Hola desde AlpacOS") == 0);
    assert(scan("/").directories == 1u);
    list_info_t content = scan("/docs");
    assert(content.files == 1u && content.total_bytes == size);
    assert(vfs_rmdir("/docs") == VFS_ERR_NOT_EMPTY);
    assert(vfs_unlink("/docs") == VFS_ERR_IS_DIR);
    assert(vfs_rmdir("/docs/hola.txt") == VFS_ERR_NOT_DIR);
    assert(vfs_unlink("/docs/hola.txt") == VFS_OK);
    assert(vfs_rmdir("/docs") == VFS_OK);
    assert(scan("/").items == 0u);
    assert(vfs_read("/docs/hola.txt", &data, &size) == VFS_ERR_NOT_FOUND);
}

static void test_invalid_and_missing(void)
{
    vfs_init();
    assert(vfs_mkdir("/") == VFS_ERR_ROOT);
    assert(vfs_rmdir("/") == VFS_ERR_ROOT);
    assert(vfs_unlink("/") == VFS_ERR_ROOT);
    assert(vfs_touch("file") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("//a") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("/a/") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("/a/../b") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("/.") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("/bad name") == VFS_ERR_INVALID_PATH);
    assert(vfs_touch("/not-found/file") == VFS_ERR_NOT_FOUND);
    assert(vfs_touch("/file") == VFS_OK);
    assert(vfs_touch("/file") == VFS_ERR_EXISTS);
    assert(vfs_mkdir("/file/inside") == VFS_ERR_NOT_DIR);
    assert(vfs_list("/file", count_entry, NULL) == VFS_ERR_NOT_DIR);
    assert(vfs_write("/", "x") == VFS_ERR_IS_DIR);
    assert(vfs_unlink("/missing") == VFS_ERR_NOT_FOUND);
    assert(vfs_list("/", NULL, NULL) == VFS_ERR_INVALID_PATH);

    char long_name[VFS_NAME_CAPACITY + 2u];
    memset(long_name, 'n', sizeof(long_name));
    long_name[0] = '/';
    long_name[sizeof(long_name) - 1u] = '\0';
    assert(vfs_touch(long_name) == VFS_ERR_INVALID_PATH);
}

static void test_limits_and_reset(void)
{
    vfs_init();
    assert(vfs_touch("/nota") == VFS_OK);
    assert(vfs_write("/nota", "anterior") == VFS_OK);
    char text[VFS_FILE_CAPACITY + 1u];
    memset(text, 'a', VFS_FILE_CAPACITY - 1u);
    text[VFS_FILE_CAPACITY - 1u] = '\0';
    assert(vfs_write("/nota", text) == VFS_OK);
    text[VFS_FILE_CAPACITY - 1u] = 'b';
    text[VFS_FILE_CAPACITY] = '\0';
    assert(vfs_write("/nota", text) == VFS_ERR_TOO_LARGE);
    const char *data;
    size_t size;
    assert(vfs_read("/nota", &data, &size) == VFS_OK);
    assert(size == VFS_FILE_CAPACITY - 1u);
    assert(data[size - 1u] == 'a'); /* Rechazar no altera contenido. */

    for (unsigned i = 0u; i < VFS_MAX_NODES - 2u; ++i) {
        char path[16];
        snprintf(path, sizeof(path), "/f%u", i);
        assert(vfs_touch(path) == VFS_OK);
    }
    assert(scan("/").items == VFS_MAX_NODES - 1u);
    assert(vfs_touch("/sobrante") == VFS_ERR_FULL);
    assert(vfs_unlink("/f0") == VFS_OK);
    assert(vfs_touch("/nuevo") == VFS_OK);
    vfs_init();
    assert(scan("/").items == 0u);
}

int main(void)
{
    test_lifecycle();
    test_invalid_and_missing();
    test_limits_and_reset();
    puts("OK: VFS - ciclo de vida, jerarquia, validaciones, limites y reinicio.");
    return 0;
}
