#ifndef YAAT_RUNTIME_ASSET_STORE_H
#define YAAT_RUNTIME_ASSET_STORE_H

#define YAAT_ASSET_STORE_MAX_ROOT 260
#define YAAT_ASSET_STORE_MAX_PATH 128
#define YAAT_ASSET_STORE_MAX_DATA 1024

typedef struct YaatAssetReadResult {
    int ok;
    char error[160];
    unsigned char data[YAAT_ASSET_STORE_MAX_DATA];
    int size;
    char source[YAAT_ASSET_STORE_MAX_ROOT];
} YaatAssetReadResult;

typedef struct YaatAssetStore {
    char root[YAAT_ASSET_STORE_MAX_ROOT];
} YaatAssetStore;

void yaat_asset_store_init(YaatAssetStore *store, const char *root);
int yaat_asset_normalize_path(const char *input, char *output, int output_size);
void yaat_asset_store_read(YaatAssetStore *store, const char *asset_path,
                           YaatAssetReadResult *result);
#define YAAT_ASSET_STORE_MAX_PATH 128
#define YAAT_ASSET_STORE_MAX_ARCHIVES 10001

/*
 * YAAT packed archive format, version 0:
 *   8 bytes  magic: "YAATDAT\0"
 *   4 bytes  little-endian entry count
 *   Repeated entry count times:
 *     2 bytes little-endian logical path length, not including NUL
 *     4 bytes little-endian data offset from the beginning of the file
 *     4 bytes little-endian data size in bytes
 *     N bytes path bytes using normalized forward slashes
 *
 * Archive validation checks that every table entry is well-formed and points
 * inside the archive file. If an archive contains duplicate logical paths, the
 * lookup behavior is deterministic: the first validated table entry wins.
 */

typedef struct YaatAssetStoreArchiveEntry {
    char name[YAAT_ASSET_STORE_MAX_PATH];
    unsigned long offset;
    unsigned long size;
} YaatAssetStoreArchiveEntry;

typedef struct YaatAssetStoreArchive {
    char path[YAAT_ASSET_STORE_MAX_PATH];
    int patch_number;
    unsigned long entry_count;
    YaatAssetStoreArchiveEntry *entries;
} YaatAssetStoreArchive;

typedef struct YaatAssetStore {
    char root_path[YAAT_ASSET_STORE_MAX_PATH];
    int archive_count;
    int archive_capacity;
    YaatAssetStoreArchive *archives;
} YaatAssetStore;

typedef struct YaatAssetData {
    unsigned char *bytes;
    unsigned long size;
} YaatAssetData;

int yaat_asset_store_open(YaatAssetStore *store, const char *root_path);
void yaat_asset_store_close(YaatAssetStore *store);

/*
 * Reads game/<logical_path> from the loose folder first, then the highest
 * discovered patch%04d.dat down to the lowest, and finally game.dat.
 * The returned byte buffer must be released with yaat_asset_data_free().
 */
int yaat_asset_store_read(YaatAssetStore *store, const char *logical_path,
                          YaatAssetData *data);
void yaat_asset_data_free(YaatAssetData *data);

#endif /* YAAT_RUNTIME_ASSET_STORE_H */
#define YAAT_ASSET_STORE_MAX_PATH 260
#define YAAT_ASSET_STORE_OK 0
#define YAAT_ASSET_STORE_ERROR 1
#define YAAT_ASSET_STORE_NOT_FOUND 2
#define YAAT_ASSET_STORE_BAD_PATH 3
#define YAAT_ASSET_STORE_NO_MEMORY 4

typedef struct YaatAssetStore {
    char base_dir[YAAT_ASSET_STORE_MAX_PATH];
    int initialized;
} YaatAssetStore;

int yaat_asset_store_init(YaatAssetStore *store, const char *base_dir);
void yaat_asset_store_shutdown(YaatAssetStore *store);
int yaat_asset_read_all(YaatAssetStore *store, const char *logical_path,
                        unsigned char **bytes, unsigned long *size);
int yaat_asset_exists(YaatAssetStore *store, const char *logical_path);

#endif
