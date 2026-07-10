#ifndef YAAT_RUNTIME_ASSET_LOADER_H
#define YAAT_RUNTIME_ASSET_LOADER_H

#include <stddef.h>

#define YAAT_ASSET_MAX_PATH 128
#define YAAT_ASSET_MAX_NAME 64
#define YAAT_ASSET_MAX_OBJECTS 32
#define YAAT_ASSET_MAX_HOTSPOTS 32

typedef struct YaatAssetBuffer {
    unsigned char *data;
    unsigned long size;
    char logical_path[YAAT_ASSET_MAX_PATH];
    char source[YAAT_ASSET_MAX_PATH];
} YaatAssetBuffer;

typedef enum YaatTransparencyMode {
    YAAT_TRANSPARENCY_NONE = 0,
    YAAT_TRANSPARENCY_COLOR_KEY,
    YAAT_TRANSPARENCY_ALPHA,
    YAAT_TRANSPARENCY_MASK
} YaatTransparencyMode;

typedef struct YaatTransparency {
    YaatTransparencyMode mode;
    unsigned long color_key;
    char mask[YAAT_ASSET_MAX_PATH];
} YaatTransparency;

typedef struct YaatRuntimeObject {
    char id[YAAT_ASSET_MAX_NAME];
    char name[YAAT_ASSET_MAX_NAME];
    char sprite[YAAT_ASSET_MAX_PATH];
    int x;
    int y;
    int width;
    int height;
    int visible;
    YaatTransparency transparency;
} YaatRuntimeObject;

typedef struct YaatRuntimeHotspot {
    char id[YAAT_ASSET_MAX_NAME];
    char name[YAAT_ASSET_MAX_NAME];
    char cursor[YAAT_ASSET_MAX_NAME];
    char script_event[YAAT_ASSET_MAX_NAME];
    int x;
    int y;
    int width;
    int height;
} YaatRuntimeHotspot;

typedef struct YaatRuntimeRoom {
    char id[YAAT_ASSET_MAX_NAME];
    char label[YAAT_ASSET_MAX_NAME];
    char background[YAAT_ASSET_MAX_PATH];
    char room_path[YAAT_ASSET_MAX_PATH];
    int width;
    int height;
    int object_count;
    YaatRuntimeObject objects[YAAT_ASSET_MAX_OBJECTS];
    int hotspot_count;
    YaatRuntimeHotspot hotspots[YAAT_ASSET_MAX_HOTSPOTS];
} YaatRuntimeRoom;

typedef struct YaatRuntimeLoadResult {
    int ok;
    char error[160];
    YaatRuntimeRoom room;
} YaatRuntimeLoadResult;

typedef struct YaatAssetStore {
    char root_path[YAAT_ASSET_MAX_PATH];
    char loose_root[YAAT_ASSET_MAX_PATH];
    char source[YAAT_ASSET_MAX_PATH];
} YaatAssetStore;

void yaat_asset_store_init(YaatAssetStore *store, const char *root_path);
int yaat_asset_read_all(YaatAssetStore *store, const char *logical_path,
                        unsigned char **data, size_t *size);

void yaat_asset_store_init_loose(YaatAssetStore *store, const char *loose_root);
int yaat_asset_store_load(YaatAssetStore *store, const char *logical_path,
                          YaatAssetBuffer *buffer);
void yaat_asset_buffer_free(YaatAssetBuffer *buffer);

void yaat_runtime_load_start_room_from_store(YaatAssetStore *store,
                                             YaatRuntimeLoadResult *result);
void yaat_runtime_load_start_room(const char *game_ini_path,
                                  YaatRuntimeLoadResult *result);

#endif
