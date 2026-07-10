#ifndef YAAT_RUNTIME_ASSET_LOADER_H
#define YAAT_RUNTIME_ASSET_LOADER_H

#include <stddef.h>

#define YAAT_ASSET_MAX_PATH 128
#define YAAT_ASSET_MAX_NAME 64
#define YAAT_ASSET_MAX_OBJECTS 32
#define YAAT_ASSET_MAX_HOTSPOTS 32
#define YAAT_ASSET_MAX_INVENTORY_ITEMS 64
#define YAAT_ASSET_MAX_ANIMATION_FRAMES 16
#define YAAT_ASSET_MAX_ANIMATIONS 8

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
    char animation[YAAT_ASSET_MAX_NAME];
    int animation_fps;
    int animation_frame_count;
    char animation_frames[YAAT_ASSET_MAX_ANIMATION_FRAMES][YAAT_ASSET_MAX_PATH];
    int x;
    int y;
    int width;
    int height;
    int visible;
    YaatTransparency transparency;
    int transparent_color_enabled;
    unsigned long transparent_color;
} YaatRuntimeObject;

typedef struct YaatRuntimeHotspot {
    char id[YAAT_ASSET_MAX_NAME];
    char name[YAAT_ASSET_MAX_NAME];
    char cursor[YAAT_ASSET_MAX_NAME];
    char script_event[YAAT_ASSET_MAX_NAME];
    char action[YAAT_ASSET_MAX_NAME];
    char target_room[YAAT_ASSET_MAX_NAME];
    int target_x;
    int target_y;
    int has_target_x;
    int has_target_y;
    int x;
    int y;
    int width;
    int height;
} YaatRuntimeHotspot;

typedef struct YaatAnimationFrame {
    char path[YAAT_ASSET_MAX_PATH];
    int x;
    int y;
    int width;
    int height;
    int duration_ms;
} YaatAnimationFrame;

typedef struct YaatAnimationClip {
    char id[YAAT_ASSET_MAX_NAME];
    int frame_count;
    int loop;
    int default_frame_ms;
    YaatAnimationFrame frames[YAAT_ASSET_MAX_ANIMATION_FRAMES];
} YaatAnimationClip;

typedef struct YaatRuntimePlayer {
    char idle[YAAT_ASSET_MAX_PATH];
    char walk_left[YAAT_ASSET_MAX_PATH];
    char walk_right[YAAT_ASSET_MAX_PATH];
    int animation_count;
    YaatAnimationClip animations[YAAT_ASSET_MAX_ANIMATIONS];
} YaatRuntimePlayer;

typedef struct YaatRuntimeRoom {
    char id[YAAT_ASSET_MAX_NAME];
    char label[YAAT_ASSET_MAX_NAME];
    char background[YAAT_ASSET_MAX_PATH];
    char walkmask[YAAT_ASSET_MAX_PATH];
    char room_path[YAAT_ASSET_MAX_PATH];
    int width;
    int height;
    int near_y;
    double near_scale;
    int far_y;
    double far_scale;
    int object_count;
    YaatRuntimeObject objects[YAAT_ASSET_MAX_OBJECTS];
    int hotspot_count;
    YaatRuntimeHotspot hotspots[YAAT_ASSET_MAX_HOTSPOTS];
} YaatRuntimeRoom;

typedef struct YaatRuntimeInventoryItem {
    char id[YAAT_ASSET_MAX_NAME];
    char name[YAAT_ASSET_MAX_NAME];
    char icon[YAAT_ASSET_MAX_PATH];
    char frames[YAAT_ASSET_MAX_ANIMATION_FRAMES][YAAT_ASSET_MAX_PATH];
    int timing_ms[YAAT_ASSET_MAX_ANIMATION_FRAMES];
    int frame_count;
    char description[160];
    char script[YAAT_ASSET_MAX_PATH];
    int stackable;
} YaatRuntimeInventoryItem;

typedef struct YaatRuntimeInventory {
    int item_count;
    YaatRuntimeInventoryItem items[YAAT_ASSET_MAX_INVENTORY_ITEMS];
} YaatRuntimeInventory;

typedef struct YaatRuntimeLoadResult {
    int ok;
    char error[160];
    YaatRuntimePlayer player;
    YaatRuntimeRoom room;
    YaatRuntimeInventory inventory;
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

void yaat_runtime_load_inventory_from_store(YaatAssetStore *store,
                                           const char *path,
                                           YaatRuntimeInventory *inventory);
int yaat_runtime_animation_frame_index(const int *timing_ms, int frame_count,
                                       unsigned long elapsed_ms);

void yaat_runtime_load_start_room_from_store(YaatAssetStore *store,
                                             YaatRuntimeLoadResult *result);
void yaat_runtime_load_room_from_store(YaatAssetStore *store,
                                       const char *room_id,
                                       YaatRuntimeLoadResult *result);
void yaat_runtime_load_start_room(const char *game_ini_path,
                                  YaatRuntimeLoadResult *result);

#endif
