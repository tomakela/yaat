#include "runtime/asset_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAAT_ASSET_ARCHIVE_MAGIC "YAATDAT\0"
#define YAAT_ASSET_ARCHIVE_MAGIC_SIZE 8
#define YAAT_ASSET_MAX_PATCH_NUMBER 9999

static void yaat_asset_copy_string(char *dst, int dst_size, const char *src)
{
    int i;

    if (dst == 0 || dst_size <= 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void yaat_asset_join_path(char *dst, int dst_size, const char *left,
                                 const char *right)
{
    int len;

    yaat_asset_copy_string(dst, dst_size, left);
    len = (int)strlen(dst);
    if (len > 0 && len < dst_size - 1 && dst[len - 1] != '/' &&
        dst[len - 1] != '\\') {
        dst[len] = '/';
        dst[len + 1] = '\0';
    }
    if ((int)strlen(dst) < dst_size - 1) {
        strncat(dst, right, dst_size - 1 - (int)strlen(dst));
    }
}

static unsigned int yaat_asset_read_le16(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned long yaat_asset_read_le32(const unsigned char *bytes)
{
    return (unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) | ((unsigned long)bytes[3] << 24);
}

static int yaat_asset_file_size(FILE *file, unsigned long *size)
{
    long end;

    if (fseek(file, 0, SEEK_END) != 0) {
        return 0;
    }
    end = ftell(file);
    if (end < 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        return 0;
    }
    *size = (unsigned long)end;
    return 1;
}

static int yaat_asset_read_file_slice(const char *path, unsigned long offset,
                                      unsigned long size, YaatAssetData *data)
{
    FILE *file;

    data->bytes = 0;
    data->size = 0;
    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    data->bytes = (unsigned char *)malloc(size == 0 ? 1 : size);
    if (data->bytes == 0) {
        fclose(file);
        return 0;
    }
    if (fseek(file, (long)offset, SEEK_SET) != 0 ||
        fread(data->bytes, 1, size, file) != size) {
        fclose(file);
        yaat_asset_data_free(data);
        return 0;
    }
    fclose(file);
    data->size = size;
    return 1;
}

static int yaat_asset_archive_load(const char *path, int patch_number,
                                   YaatAssetStoreArchive *archive)
{
    FILE *file;
    unsigned long file_size;
    unsigned char header[12];
    unsigned long i;

    memset(archive, 0, sizeof(*archive));
    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    if (!yaat_asset_file_size(file, &file_size) || file_size < sizeof(header) ||
        fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, YAAT_ASSET_ARCHIVE_MAGIC,
               YAAT_ASSET_ARCHIVE_MAGIC_SIZE) != 0) {
        fclose(file);
        return 0;
    }

    archive->entry_count = yaat_asset_read_le32(header + 8);
    archive->entries = (YaatAssetStoreArchiveEntry *)calloc(
        archive->entry_count == 0 ? 1 : archive->entry_count,
        sizeof(YaatAssetStoreArchiveEntry));
    if (archive->entries == 0) {
        fclose(file);
        return 0;
    }

    for (i = 0; i < archive->entry_count; ++i) {
        unsigned char entry_header[10];
        unsigned int name_length;
        YaatAssetStoreArchiveEntry *entry = &archive->entries[i];

        if (fread(entry_header, 1, sizeof(entry_header), file) !=
            sizeof(entry_header)) {
            fclose(file);
            free(archive->entries);
            archive->entries = 0;
            return 0;
        }
        name_length = yaat_asset_read_le16(entry_header);
        entry->offset = yaat_asset_read_le32(entry_header + 2);
        entry->size = yaat_asset_read_le32(entry_header + 6);
        if (name_length == 0 || name_length >= YAAT_ASSET_STORE_MAX_PATH ||
            entry->offset > file_size || entry->size > file_size - entry->offset ||
            fread(entry->name, 1, name_length, file) != name_length) {
            fclose(file);
            free(archive->entries);
            archive->entries = 0;
            return 0;
        }
        entry->name[name_length] = '\0';
    }

    fclose(file);
    yaat_asset_copy_string(archive->path, sizeof(archive->path), path);
    archive->patch_number = patch_number;
    return 1;
}

static void yaat_asset_store_add_archive(YaatAssetStore *store,
                                         const char *archive_name,
                                         int patch_number)
{
    char path[YAAT_ASSET_STORE_MAX_PATH];
    YaatAssetStoreArchive archive;

    if (store->archive_count >= YAAT_ASSET_STORE_MAX_ARCHIVES) {
        return;
    }
    yaat_asset_join_path(path, sizeof(path), store->root_path, archive_name);
    if (!yaat_asset_archive_load(path, patch_number, &archive)) {
        return;
    }
    if (store->archive_count >= store->archive_capacity) {
        int new_capacity = store->archive_capacity == 0 ? 4 :
                           store->archive_capacity * 2;
        YaatAssetStoreArchive *new_archives;
        if (new_capacity > YAAT_ASSET_STORE_MAX_ARCHIVES) {
            new_capacity = YAAT_ASSET_STORE_MAX_ARCHIVES;
        }
        new_archives = (YaatAssetStoreArchive *)realloc(
            store->archives, (unsigned int)new_capacity *
                             sizeof(YaatAssetStoreArchive));
        if (new_archives == 0) {
            free(archive.entries);
            return;
        }
        store->archives = new_archives;
        store->archive_capacity = new_capacity;
    }
    store->archives[store->archive_count++] = archive;
}

int yaat_asset_store_open(YaatAssetStore *store, const char *root_path)
{
    int patch;

    if (store == 0) {
        return 0;
    }
    memset(store, 0, sizeof(*store));
    yaat_asset_copy_string(store->root_path, sizeof(store->root_path),
                           (root_path == 0 || root_path[0] == '\0') ? "." :
                                                                     root_path);

    for (patch = YAAT_ASSET_MAX_PATCH_NUMBER; patch >= 0; --patch) {
        char archive_name[16];
        sprintf(archive_name, "patch%04d.dat", patch);
        yaat_asset_store_add_archive(store, archive_name, patch);
    }
    yaat_asset_store_add_archive(store, "game.dat", -1);
    return 1;
}

void yaat_asset_store_close(YaatAssetStore *store)
{
    int i;

    if (store == 0) {
        return;
    }
    for (i = 0; i < store->archive_count; ++i) {
        free(store->archives[i].entries);
        store->archives[i].entries = 0;
    }
    free(store->archives);
    store->archives = 0;
    memset(store, 0, sizeof(*store));
}

int yaat_asset_store_read(YaatAssetStore *store, const char *logical_path,
                          YaatAssetData *data)
{
    char loose_path[YAAT_ASSET_STORE_MAX_PATH];
    char game_path[YAAT_ASSET_STORE_MAX_PATH];
    int archive_index;

    if (data == 0) {
        return 0;
    }
    data->bytes = 0;
    data->size = 0;
    if (store == 0 || logical_path == 0 || logical_path[0] == '\0') {
        return 0;
    }

    yaat_asset_join_path(game_path, sizeof(game_path), "game", logical_path);
    yaat_asset_join_path(loose_path, sizeof(loose_path), store->root_path,
                         game_path);
    {
        FILE *loose_file = fopen(loose_path, "rb");
        unsigned long loose_size;
        if (loose_file != 0) {
            if (!yaat_asset_file_size(loose_file, &loose_size)) {
                fclose(loose_file);
                return 0;
            }
            fclose(loose_file);
            return yaat_asset_read_file_slice(loose_path, 0, loose_size, data);
        }
    }

    for (archive_index = 0; archive_index < store->archive_count;
         ++archive_index) {
        YaatAssetStoreArchive *archive = &store->archives[archive_index];
        unsigned long entry_index;

        for (entry_index = 0; entry_index < archive->entry_count; ++entry_index) {
            YaatAssetStoreArchiveEntry *entry = &archive->entries[entry_index];
            if (strcmp(entry->name, logical_path) == 0) {
                return yaat_asset_read_file_slice(archive->path, entry->offset,
                                                  entry->size, data);
            }
        }
    }
    return 0;
}

void yaat_asset_data_free(YaatAssetData *data)
{
    if (data == 0) {
        return;
    }
    free(data->bytes);
    data->bytes = 0;
    data->size = 0;
}
