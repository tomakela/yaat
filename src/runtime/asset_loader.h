#ifndef YAAT_RUNTIME_ASSET_LOADER_H
#define YAAT_RUNTIME_ASSET_LOADER_H

#define YAAT_ASSET_MAX_PATH 128
#define YAAT_ASSET_MAX_NAME 64
#define YAAT_ASSET_MAX_OBJECTS 32

typedef struct YaatRuntimeObject {
    char id[YAAT_ASSET_MAX_NAME];
    char name[YAAT_ASSET_MAX_NAME];
    char sprite[YAAT_ASSET_MAX_PATH];
    int x;
    int y;
    int width;
    int height;
    int visible;
} YaatRuntimeObject;

typedef struct YaatRuntimeRoom {
    char id[YAAT_ASSET_MAX_NAME];
    char label[YAAT_ASSET_MAX_NAME];
    char background[YAAT_ASSET_MAX_PATH];
    char room_path[YAAT_ASSET_MAX_PATH];
    int width;
    int height;
    int object_count;
    YaatRuntimeObject objects[YAAT_ASSET_MAX_OBJECTS];
} YaatRuntimeRoom;

typedef struct YaatRuntimeLoadResult {
    int ok;
    char error[160];
    YaatRuntimeRoom room;
} YaatRuntimeLoadResult;

void yaat_runtime_load_start_room(const char *game_ini_path,
                                  YaatRuntimeLoadResult *result);

#endif
