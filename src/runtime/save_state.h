#ifndef YAAT_RUNTIME_SAVE_STATE_H
#define YAAT_RUNTIME_SAVE_STATE_H

#include "script_parser.h"
#include "script_package.h"
#include "runtime/asset_loader.h"

#define YAAT_SAVE_STATE_VERSION 1
#define YAAT_SAVE_SLOT_COUNT 3
#define YAAT_SAVE_PATH "yaat_save_state_slot1.txt"
#define YAAT_MAX_SAVED_RUNTIME_OBJECTS (YAAT_MAX_ROOMS * YAAT_ASSET_MAX_OBJECTS)

typedef struct YaatSavedRuntimeObjectState {
    char room_id[YAAT_ASSET_MAX_NAME];
    char object_id[YAAT_ASSET_MAX_NAME];
    int x;
    int y;
    int width;
    int height;
    int visible;
    int transparency_mode;
    unsigned long transparency_color_key;
    int transparent_color_enabled;
    unsigned long transparent_color;
} YaatSavedRuntimeObjectState;

typedef enum YaatSaveMenuMode {
    YAAT_SAVE_MENU_CLOSED,
    YAAT_SAVE_MENU_SAVE,
    YAAT_SAVE_MENU_LOAD
} YaatSaveMenuMode;

typedef struct YaatSaveSlotInfo {
    int exists;
    char label[32];
    char room[YAAT_ASSET_MAX_NAME];
    char timestamp[32];
    unsigned long play_time_ms;
} YaatSaveSlotInfo;

#endif
