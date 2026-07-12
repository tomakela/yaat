/* Standalone YAAT packed/loose asset store smoke tests. */
#include "runtime/asset_store.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL: %s\n", name);
        failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static void expect_read(const char *name, YaatTestAssetStore *store,
                        const char *path, const char *want)
{
    YaatTestAssetReadResult result;
    yaat_test_asset_store_read(store, path, &result);
    if (!result.ok || result.size != (int)strlen(want) || memcmp(result.data, want, strlen(want)) != 0) {
        printf("FAIL: %s: got ok=%d size=%d text='%s' error='%s' source='%s'\n",
               name, result.ok, result.size, result.data, result.error, result.source);
        failures++;
    } else {
        printf("ok: %s from %s\n", name, result.source);
    }
}

static void expect_missing(YaatTestAssetStore *store)
{
    YaatTestAssetReadResult result;
    yaat_test_asset_store_read(store, "missing.txt", &result);
    expect_true("missing asset fails clearly", !result.ok && strstr(result.error, "Missing asset") != 0);
}

static void expect_unsafe(YaatTestAssetStore *store, const char *path)
{
    YaatTestAssetReadResult result;
    yaat_test_asset_store_read(store, path, &result);
    expect_true(path, !result.ok && strstr(result.error, "Unsafe asset path") != 0);
}

int main(void)
{
    YaatTestAssetStore store;
    char normalized[YAAT_TEST_ASSET_STORE_MAX_PATH];

    yaat_test_asset_store_init(&store, "tests/fixtures/assets");

    expect_read("game.dat supplies base game.ini", &store, "game.ini", "base game ini\n");
    expect_read("loose game overrides every archive", &store, "data/value.txt", "loose wins\n");

    yaat_test_asset_store_init(&store, "tests/fixtures/assets_patched");
    expect_read("patch0001 overrides the same file again", &store, "data/value.txt", "patch one\n");
    yaat_test_asset_store_init(&store, "tests/fixtures/assets_patch0");
    expect_read("patch0000 overrides game.dat", &store, "data/value.txt", "patch zero\n");

    expect_missing(&store);
    expect_unsafe(&store, "../x");
    expect_unsafe(&store, "/x");
    expect_unsafe(&store, "C:\\x");
    expect_true("mixed slashes normalize safely",
                yaat_test_asset_normalize_path("safe\\mixed/path.txt", normalized, sizeof(normalized)) &&
                strcmp(normalized, "safe/mixed/path.txt") == 0);
    expect_true("mixed slash traversal is rejected",
                !yaat_test_asset_normalize_path("safe\\..\\x", normalized, sizeof(normalized)));

    yaat_test_asset_store_init(&store, "tests/fixtures/assets_duplicate");
    expect_read("duplicate logical paths use last entry in archive", &store, "dup.txt", "second\n");

    if (failures != 0) {
        printf("asset store smoke: %d failure(s)\n", failures);
        return 1;
    }
    printf("asset store smoke: all checks passed\n");
    return 0;
}
