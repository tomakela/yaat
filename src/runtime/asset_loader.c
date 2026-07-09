#include "runtime/asset_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAAT_LINE_MAX 256

typedef struct YaatGameConfig {
    char first_room[YAAT_ASSET_MAX_NAME];
    char rooms_path[YAAT_ASSET_MAX_PATH];
} YaatGameConfig;

static void yaat_copy_string(char *dst, int dst_size, const char *src)
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

static void yaat_set_error(YaatRuntimeLoadResult *result, const char *message,
                           const char *path)
{
    if (result == 0) {
        return;
    }
    if (path != 0 && path[0] != '\0') {
        sprintf(result->error, "%s: %s", message, path);
    } else {
        yaat_copy_string(result->error, sizeof(result->error), message);
    }
    result->ok = 0;
}

static char *yaat_trim(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';
    return text;
}

static int yaat_bool_value(const char *value, int default_value)
{
    if (value == 0 || value[0] == '\0') {
        return default_value;
    }
    if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
        strcmp(value, "1") == 0) {
        return 1;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0 ||
        strcmp(value, "0") == 0) {
        return 0;
    }
    return default_value;
}

static void yaat_join_path(char *dst, int dst_size, const char *left,
                           const char *right)
{
    int len;

    yaat_copy_string(dst, dst_size, left);
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

static void yaat_load_game_config(const char *path, YaatGameConfig *config,
                                  YaatRuntimeLoadResult *result)
{
    FILE *file;
    char line[YAAT_LINE_MAX];
    char section[YAAT_ASSET_MAX_NAME];

    yaat_copy_string(config->rooms_path, sizeof(config->rooms_path), "rooms");
    config->first_room[0] = '\0';
    section[0] = '\0';

    file = fopen(path, "r");
    if (file == 0) {
        yaat_set_error(result, "Missing game metadata", path);
        return;
    }

    while (fgets(line, sizeof(line), file) != 0) {
        char *text;
        char *equals;

        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') {
            continue;
        }
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            if (close != 0) {
                *close = '\0';
                yaat_copy_string(section, sizeof(section), text + 1);
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) {
            continue;
        }
        *equals = '\0';
        if (strcmp(section, "game") == 0 &&
            strcmp(yaat_trim(text), "first_room") == 0) {
            yaat_copy_string(config->first_room, sizeof(config->first_room),
                             yaat_trim(equals + 1));
        } else if (strcmp(section, "paths") == 0 &&
                   strcmp(yaat_trim(text), "rooms") == 0) {
            yaat_copy_string(config->rooms_path, sizeof(config->rooms_path),
                             yaat_trim(equals + 1));
        }
    }
    fclose(file);

    if (config->first_room[0] == '\0') {
        yaat_set_error(result, "game.ini is missing game.first_room", path);
    }
}

static void yaat_load_room_ini(const char *path, YaatRuntimeRoom *room,
                               YaatRuntimeLoadResult *result)
{
    FILE *file;
    char line[YAAT_LINE_MAX];
    char section[YAAT_ASSET_MAX_NAME];

    section[0] = '\0';
    room->width = 320;
    room->height = 200;

    file = fopen(path, "r");
    if (file == 0) {
        yaat_set_error(result, "Missing room metadata", path);
        return;
    }

    while (fgets(line, sizeof(line), file) != 0) {
        char *text;
        char *equals;

        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') {
            continue;
        }
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            if (close != 0) {
                *close = '\0';
                yaat_copy_string(section, sizeof(section), text + 1);
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) {
            continue;
        }
        *equals = '\0';
        text = yaat_trim(text);
        ++equals;
        if (strcmp(section, "id") == 0 && strcmp(text, "name") == 0) {
            yaat_copy_string(room->id, sizeof(room->id), yaat_trim(equals));
        } else if (strcmp(section, "id") == 0 && strcmp(text, "label") == 0) {
            yaat_copy_string(room->label, sizeof(room->label), yaat_trim(equals));
        } else if (strcmp(section, "display") == 0 && strcmp(text, "width") == 0) {
            room->width = atoi(yaat_trim(equals));
        } else if (strcmp(section, "display") == 0 && strcmp(text, "height") == 0) {
            room->height = atoi(yaat_trim(equals));
        } else if (strcmp(section, "display") == 0 && strcmp(text, "background") == 0) {
            yaat_copy_string(room->background, sizeof(room->background), yaat_trim(equals));
        }
    }
    fclose(file);

    if (room->id[0] == '\0' || room->background[0] == '\0') {
        yaat_set_error(result, "room.ini is missing required id/background", path);
    }
}


static void yaat_parse_rect(const char *value, int *x, int *y, int *width,
                            int *height)
{
    int parsed_x;
    int parsed_y;
    int parsed_width;
    int parsed_height;

    if (value == 0) {
        return;
    }

    parsed_x = 0;
    parsed_y = 0;
    parsed_width = 0;
    parsed_height = 0;
    if (sscanf(value, "%d,%d,%d,%d", &parsed_x, &parsed_y, &parsed_width,
               &parsed_height) == 4) {
        *x = parsed_x;
        *y = parsed_y;
        *width = parsed_width;
        *height = parsed_height;
    }
}

static void yaat_load_room_objects(const char *path, YaatRuntimeRoom *room)
{
    FILE *file;
    char line[YAAT_LINE_MAX];
    YaatRuntimeObject *object;

    file = fopen(path, "r");
    if (file == 0) {
        return;
    }

    object = 0;
    while (fgets(line, sizeof(line), file) != 0) {
        char *text;
        char *equals;

        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') {
            continue;
        }
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            if (close != 0 && room->object_count < YAAT_ASSET_MAX_OBJECTS) {
                *close = '\0';
                object = &room->objects[room->object_count++];
                memset(object, 0, sizeof(*object));
                object->visible = 1;
                yaat_copy_string(object->id, sizeof(object->id), text + 1);
            }
            continue;
        }
        if (object == 0) {
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) {
            continue;
        }
        *equals = '\0';
        text = yaat_trim(text);
        ++equals;
        if (strcmp(text, "name") == 0) {
            yaat_copy_string(object->name, sizeof(object->name), yaat_trim(equals));
        } else if (strcmp(text, "sprite") == 0) {
            yaat_copy_string(object->sprite, sizeof(object->sprite), yaat_trim(equals));
        } else if (strcmp(text, "x") == 0) {
            object->x = atoi(yaat_trim(equals));
        } else if (strcmp(text, "y") == 0) {
            object->y = atoi(yaat_trim(equals));
        } else if (strcmp(text, "width") == 0) {
            object->width = atoi(yaat_trim(equals));
        } else if (strcmp(text, "height") == 0) {
            object->height = atoi(yaat_trim(equals));
        } else if (strcmp(text, "visible") == 0) {
            object->visible = yaat_bool_value(yaat_trim(equals), 1);
        }
    }
    fclose(file);
}

static void yaat_load_room_hotspots(const char *path, YaatRuntimeRoom *room)
{
    FILE *file;
    char line[YAAT_LINE_MAX];
    YaatRuntimeHotspot *hotspot;

    file = fopen(path, "r");
    if (file == 0) {
        return;
    }

    hotspot = 0;
    while (fgets(line, sizeof(line), file) != 0) {
        char *text;
        char *equals;

        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') {
            continue;
        }
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            hotspot = 0;
            if (close != 0 && room->hotspot_count < YAAT_ASSET_MAX_HOTSPOTS) {
                *close = '\0';
                hotspot = &room->hotspots[room->hotspot_count++];
                memset(hotspot, 0, sizeof(*hotspot));
                yaat_copy_string(hotspot->id, sizeof(hotspot->id), text + 1);
            }
            continue;
        }
        if (hotspot == 0) {
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) {
            continue;
        }
        *equals = '\0';
        text = yaat_trim(text);
        ++equals;
        if (strcmp(text, "name") == 0) {
            yaat_copy_string(hotspot->name, sizeof(hotspot->name), yaat_trim(equals));
        } else if (strcmp(text, "rect") == 0) {
            yaat_parse_rect(yaat_trim(equals), &hotspot->x, &hotspot->y,
                            &hotspot->width, &hotspot->height);
        } else if (strcmp(text, "cursor") == 0) {
            yaat_copy_string(hotspot->cursor, sizeof(hotspot->cursor), yaat_trim(equals));
        } else if (strcmp(text, "script_event") == 0) {
            yaat_copy_string(hotspot->script_event, sizeof(hotspot->script_event),
                             yaat_trim(equals));
        }
    }
    fclose(file);
}

void yaat_runtime_load_start_room(const char *game_ini_path,
                                  YaatRuntimeLoadResult *result)
{
    YaatGameConfig config;
    char game_dir[YAAT_ASSET_MAX_PATH];
    char rooms_root[YAAT_ASSET_MAX_PATH];
    char room_dir[YAAT_ASSET_MAX_PATH];
    char room_ini[YAAT_ASSET_MAX_PATH];
    char objects_ini[YAAT_ASSET_MAX_PATH];
    char hotspots_ini[YAAT_ASSET_MAX_PATH];
    char *slash;
    char *backslash;

    if (result == 0) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->ok = 1;

    yaat_load_game_config(game_ini_path, &config, result);
    if (!result->ok) {
        return;
    }

    yaat_copy_string(game_dir, sizeof(game_dir), game_ini_path);
    slash = strrchr(game_dir, '/');
    backslash = strrchr(game_dir, '\\');
    if (backslash != 0 && (slash == 0 || backslash > slash)) {
        slash = backslash;
    }
    if (slash != 0) {
        *slash = '\0';
    } else {
        yaat_copy_string(game_dir, sizeof(game_dir), ".");
    }

    yaat_join_path(rooms_root, sizeof(rooms_root), game_dir, config.rooms_path);
    yaat_join_path(room_dir, sizeof(room_dir), rooms_root, config.first_room);
    yaat_join_path(room_ini, sizeof(room_ini), room_dir, "room.ini");
    yaat_join_path(objects_ini, sizeof(objects_ini), room_dir, "objects.ini");
    yaat_join_path(hotspots_ini, sizeof(hotspots_ini), room_dir, "hotspots.ini");

    yaat_copy_string(result->room.room_path, sizeof(result->room.room_path), room_dir);
    yaat_load_room_ini(room_ini, &result->room, result);
    if (!result->ok) {
        return;
    }
    yaat_load_room_objects(objects_ini, &result->room);
    yaat_load_room_hotspots(hotspots_ini, &result->room);
}
