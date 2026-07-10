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

static void yaat_join_path(char *dst, int dst_size, const char *left,
                           const char *right);

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


void yaat_asset_store_init(YaatAssetStore *store, const char *root_path)
{
    if (store == 0) {
        return;
    }
    yaat_copy_string(store->root_path, sizeof(store->root_path),
                     root_path != 0 && root_path[0] != '\0' ? root_path : ".");
}

int yaat_asset_read_all(YaatAssetStore *store, const char *logical_path,
                        unsigned char **data, size_t *size)
{
    char physical_path[YAAT_ASSET_MAX_PATH * 2];
    FILE *file;
    long file_size;
    unsigned char *buffer;

    if (data == 0 || size == 0) {
        return 0;
    }
    *data = 0;
    *size = 0;
    if (store == 0 || logical_path == 0 || logical_path[0] == '\0' ||
        strstr(logical_path, "..") != 0) {
        return 0;
    }

    yaat_join_path(physical_path, sizeof(physical_path), store->root_path,
                   logical_path);
    file = fopen(physical_path, "rb");
    if (file == 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    buffer = (unsigned char *)malloc((size_t)file_size + 1);
    if (buffer == 0) {
        fclose(file);
        return 0;
    }
    if (fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(buffer);
        fclose(file);
        return 0;
    }
    fclose(file);
    buffer[file_size] = '\0';
    *data = buffer;
    *size = (size_t)file_size;
    return 1;
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



typedef struct YaatIniReader {
    const char *data;
    unsigned long size;
    unsigned long offset;
} YaatIniReader;

void yaat_asset_store_init_loose(YaatAssetStore *store, const char *loose_root)
{
    if (store == 0) {
        return;
    }
    yaat_copy_string(store->loose_root, sizeof(store->loose_root),
                     loose_root != 0 ? loose_root : "game");
    yaat_copy_string(store->source, sizeof(store->source), store->loose_root);
    if (store->source[0] != '\0' &&
        store->source[strlen(store->source) - 1] != '/' &&
        store->source[strlen(store->source) - 1] != '\\') {
        strncat(store->source, "/", sizeof(store->source) - 1 - strlen(store->source));
    }
}

int yaat_asset_store_load(YaatAssetStore *store, const char *logical_path,
                          YaatAssetBuffer *buffer)
{
    char physical_path[YAAT_ASSET_MAX_PATH];
    FILE *file;
    long size;

    if (buffer == 0) {
        return 0;
    }
    memset(buffer, 0, sizeof(*buffer));
    if (store == 0 || logical_path == 0) {
        return 0;
    }
    yaat_copy_string(buffer->logical_path, sizeof(buffer->logical_path), logical_path);
    yaat_copy_string(buffer->source, sizeof(buffer->source), store->source);
    yaat_join_path(physical_path, sizeof(physical_path), store->loose_root, logical_path);
    file = fopen(physical_path, "rb");
    if (file == 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    buffer->data = (unsigned char *)malloc((unsigned long)size + 1);
    if (buffer->data == 0) {
        fclose(file);
        return 0;
    }
    buffer->size = (unsigned long)size;
    if (size > 0 && fread(buffer->data, 1, (unsigned long)size, file) != (unsigned long)size) {
        yaat_asset_buffer_free(buffer);
        fclose(file);
        return 0;
    }
    buffer->data[buffer->size] = '\0';
    fclose(file);
    return 1;
}

void yaat_asset_buffer_free(YaatAssetBuffer *buffer)
{
    if (buffer == 0) {
        return;
    }
    if (buffer->data != 0) {
        free(buffer->data);
    }
    memset(buffer, 0, sizeof(*buffer));
}

static int yaat_ini_read_line(YaatIniReader *reader, char *line, int line_size)
{
    int out;

    if (reader == 0 || line == 0 || line_size <= 0 || reader->offset >= reader->size) {
        return 0;
    }
    out = 0;
    while (reader->offset < reader->size) {
        char ch = reader->data[reader->offset++];
        if (ch == '\n') {
            break;
        }
        if (ch == '\r') {
            if (reader->offset < reader->size && reader->data[reader->offset] == '\n') {
                ++reader->offset;
            }
            break;
        }
        if (out < line_size - 1) {
            line[out++] = ch;
        }
    }
    line[out] = '\0';
    return 1;
}

static void yaat_format_asset_error_path(char *dst, int dst_size,
                                         const char *logical_path,
                                         const char *source)
{
    yaat_copy_string(dst, dst_size, logical_path);
    if (source != 0 && source[0] != '\0' && (int)strlen(dst) < dst_size - 10) {
        strncat(dst, " (from ", dst_size - 1 - strlen(dst));
        strncat(dst, source, dst_size - 1 - strlen(dst));
        strncat(dst, ")", dst_size - 1 - strlen(dst));
    }
}
static void yaat_load_game_config(YaatAssetStore *store, const char *path,
                                  int require_first_room,
                                  YaatGameConfig *config,
                                  YaatRuntimeLoadResult *result)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    char section[YAAT_ASSET_MAX_NAME];
    char error_path[YAAT_ASSET_MAX_PATH + YAAT_ASSET_MAX_PATH];

    yaat_copy_string(config->rooms_path, sizeof(config->rooms_path), "rooms");
    config->first_room[0] = '\0';
    section[0] = '\0';

    if (!yaat_asset_store_load(store, path, &buffer)) {
        yaat_format_asset_error_path(error_path, sizeof(error_path), path,
                                     store != 0 ? store->source : 0);
        yaat_set_error(result, "Missing game metadata", error_path);
        return;
    }
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;

    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
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
    yaat_asset_buffer_free(&buffer);

    if (require_first_room && config->first_room[0] == '\0') {
        yaat_format_asset_error_path(error_path, sizeof(error_path), path,
                                     store != 0 ? store->source : 0);
        yaat_set_error(result, "game.ini is missing game.first_room", error_path);
    }
}

static void yaat_load_room_ini(YaatAssetStore *store, const char *path, YaatRuntimeRoom *room,
                               YaatRuntimeLoadResult *result)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    char section[YAAT_ASSET_MAX_NAME];
    char error_path[YAAT_ASSET_MAX_PATH + YAAT_ASSET_MAX_PATH];

    section[0] = '\0';
    room->width = 320;
    room->height = 200;

    if (!yaat_asset_store_load(store, path, &buffer)) {
        yaat_format_asset_error_path(error_path, sizeof(error_path), path,
                                     store != 0 ? store->source : 0);
        yaat_set_error(result, "Missing room metadata", error_path);
        return;
    }
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;

    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
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
    yaat_asset_buffer_free(&buffer);

    if (room->id[0] == '\0' || room->background[0] == '\0') {
        yaat_format_asset_error_path(error_path, sizeof(error_path), path,
                                     store != 0 ? store->source : 0);
        yaat_set_error(result, "room.ini is missing required id/background", error_path);
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

static void yaat_load_room_objects(YaatAssetStore *store, const char *path, YaatRuntimeRoom *room)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    YaatRuntimeObject *object;

    if (!yaat_asset_store_load(store, path, &buffer)) {
        return;
    }
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;

    object = 0;
    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
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
    yaat_asset_buffer_free(&buffer);
}

static void yaat_load_room_hotspots(YaatAssetStore *store, const char *path, YaatRuntimeRoom *room)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    YaatRuntimeHotspot *hotspot;

    if (!yaat_asset_store_load(store, path, &buffer)) {
        return;
    }
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;

    hotspot = 0;
    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
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
    yaat_asset_buffer_free(&buffer);
}

void yaat_runtime_load_room_from_store(YaatAssetStore *store,
                                       const char *room_id,
                                       YaatRuntimeLoadResult *result)
{
    YaatGameConfig config;
    char room_dir[YAAT_ASSET_MAX_PATH];
    char room_ini[YAAT_ASSET_MAX_PATH];
    char objects_ini[YAAT_ASSET_MAX_PATH];
    char hotspots_ini[YAAT_ASSET_MAX_PATH];

    if (result == 0) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->ok = 1;

    if (room_id == 0 || room_id[0] == '\0') {
        yaat_set_error(result, "Missing room id", 0);
        return;
    }

    yaat_load_game_config(store, "game.ini", 0, &config, result);
    if (!result->ok) {
        return;
    }

    if (strchr(room_id, '/') != 0 || strchr(room_id, '\\') != 0) {
        yaat_copy_string(room_dir, sizeof(room_dir), room_id);
    } else {
        yaat_join_path(room_dir, sizeof(room_dir), config.rooms_path, room_id);
    }
    yaat_join_path(room_ini, sizeof(room_ini), room_dir, "room.ini");
    yaat_join_path(objects_ini, sizeof(objects_ini), room_dir, "objects.ini");
    yaat_join_path(hotspots_ini, sizeof(hotspots_ini), room_dir, "hotspots.ini");

    yaat_copy_string(result->room.room_path, sizeof(result->room.room_path), room_dir);
    yaat_load_room_ini(store, room_ini, &result->room, result);
    if (!result->ok) {
        return;
    }
    yaat_load_room_objects(store, objects_ini, &result->room);
    yaat_load_room_hotspots(store, hotspots_ini, &result->room);
}

void yaat_runtime_load_start_room_from_store(YaatAssetStore *store,
                                             YaatRuntimeLoadResult *result)
{
    YaatGameConfig config;

    if (result == 0) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->ok = 1;

    yaat_load_game_config(store, "game.ini", 1, &config, result);
    if (!result->ok) {
        return;
    }
    yaat_runtime_load_room_from_store(store, config.first_room, result);
}

void yaat_runtime_load_start_room(const char *game_ini_path,
                                  YaatRuntimeLoadResult *result)
{
    YaatAssetStore store;
    char game_dir[YAAT_ASSET_MAX_PATH];
    char *slash;
    char *backslash;

    yaat_copy_string(game_dir, sizeof(game_dir), game_ini_path);
    slash = strrchr(game_dir, '/');
    backslash = strrchr(game_dir, '\\');
    if (backslash != 0 && (slash == 0 || backslash > slash)) {
        slash = backslash;
    }
    if (slash != 0) {
        *slash = '\0';
    } else {
        yaat_copy_string(game_dir, sizeof(game_dir), "game");
    }
    yaat_asset_store_init_loose(&store, game_dir);
    yaat_runtime_load_start_room_from_store(&store, result);
}
