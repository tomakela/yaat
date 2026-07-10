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

#endif
