#ifndef YAAT_RUNTIME_ASSET_STORE_H
#define YAAT_RUNTIME_ASSET_STORE_H

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
