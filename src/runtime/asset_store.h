#ifndef YAAT_RUNTIME_ASSET_STORE_H
#define YAAT_RUNTIME_ASSET_STORE_H

/* Smoke-test-only packed/loose asset helper.
   Runtime code uses runtime/asset_loader.h as the canonical asset loader API. */
#define YAAT_TEST_ASSET_STORE_MAX_ROOT 260
#define YAAT_TEST_ASSET_STORE_MAX_PATH 128
#define YAAT_TEST_ASSET_STORE_MAX_DATA 1024

typedef struct YaatTestAssetReadResult {
    int ok;
    char error[160];
    unsigned char data[YAAT_TEST_ASSET_STORE_MAX_DATA + 1];
    int size;
    char source[YAAT_TEST_ASSET_STORE_MAX_ROOT];
} YaatTestAssetReadResult;

typedef struct YaatTestAssetStore {
    char root[YAAT_TEST_ASSET_STORE_MAX_ROOT];
} YaatTestAssetStore;

void yaat_test_asset_store_init(YaatTestAssetStore *store, const char *root);
int yaat_test_asset_normalize_path(const char *input, char *output, int output_size);
void yaat_test_asset_store_read(YaatTestAssetStore *store, const char *asset_path,
                                YaatTestAssetReadResult *result);

#endif /* YAAT_RUNTIME_ASSET_STORE_H */
