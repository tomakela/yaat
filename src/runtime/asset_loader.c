#include "runtime/asset_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAAT_LINE_MAX 256

typedef struct YaatGameConfig {
    char first_room[YAAT_ASSET_MAX_NAME];
    char rooms_path[YAAT_ASSET_MAX_PATH];
    YaatRuntimePlayer player;
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
    yaat_copy_string(store->root_path, sizeof(store->root_path),
                     loose_root != 0 ? loose_root : "game");
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
static YaatAnimationClip *yaat_find_or_add_animation(YaatRuntimePlayer *player,
                                                     const char *id);
static void yaat_parse_player_animation_frames(YaatAnimationClip *clip, char *value);
static void yaat_parse_animation_frame_value(YaatAnimationClip *clip, int index,
                                             const char *value);
static void yaat_default_player_animations(YaatRuntimePlayer *player);

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
    char frames[YAAT_LINE_MAX];
    YaatAnimationClip *clip;

    memset(config, 0, sizeof(*config));
    yaat_copy_string(config->rooms_path, sizeof(config->rooms_path), "rooms");
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
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') continue;
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            if (close != 0) {
                *close = '\0';
                yaat_copy_string(section, sizeof(section), text + 1);
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        text = yaat_trim(text);
        ++equals;

        if (strcmp(section, "game") == 0 && strcmp(text, "first_room") == 0) {
            yaat_copy_string(config->first_room, sizeof(config->first_room), yaat_trim(equals));
        } else if (strcmp(section, "paths") == 0 && strcmp(text, "rooms") == 0) {
            yaat_copy_string(config->rooms_path, sizeof(config->rooms_path), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "idle") == 0) {
            yaat_copy_string(config->player.idle, sizeof(config->player.idle), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "idle_left") == 0) {
            yaat_copy_string(config->player.idle_left, sizeof(config->player.idle_left), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "idle_right") == 0) {
            yaat_copy_string(config->player.idle_right, sizeof(config->player.idle_right), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "idle_up") == 0) {
            yaat_copy_string(config->player.idle_up, sizeof(config->player.idle_up), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "idle_down") == 0) {
            yaat_copy_string(config->player.idle_down, sizeof(config->player.idle_down), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "walk_left") == 0) {
            yaat_copy_string(config->player.walk_left, sizeof(config->player.walk_left), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "walk_right") == 0) {
            yaat_copy_string(config->player.walk_right, sizeof(config->player.walk_right), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "walk_up") == 0) {
            yaat_copy_string(config->player.walk_up, sizeof(config->player.walk_up), yaat_trim(equals));
        } else if (strcmp(section, "player") == 0 && strcmp(text, "walk_down") == 0) {
            yaat_copy_string(config->player.walk_down, sizeof(config->player.walk_down), yaat_trim(equals));
        } else if (strncmp(section, "player.animation.", 17) == 0) {
            clip = yaat_find_or_add_animation(&config->player, section + 17);
            if (clip != 0) {
                if (strcmp(text, "frame_ms") == 0) clip->default_frame_ms = atoi(yaat_trim(equals));
                else if (strcmp(text, "loop") == 0) clip->loop = yaat_bool_value(yaat_trim(equals), 1);
                else if (strcmp(text, "frames") == 0) {
                    yaat_copy_string(frames, sizeof(frames), yaat_trim(equals));
                    yaat_parse_player_animation_frames(clip, frames);
                } else if (strncmp(text, "frame", 5) == 0) {
                    yaat_parse_animation_frame_value(clip, atoi(text + 5), yaat_trim(equals));
                }
            }
        }
    }
    yaat_asset_buffer_free(&buffer);

    if (config->player.idle[0] == '\0') yaat_copy_string(config->player.idle, sizeof(config->player.idle), "graphics/sprites/player_idle.bmp");
    if (config->player.idle_left[0] == '\0') yaat_copy_string(config->player.idle_left, sizeof(config->player.idle_left), config->player.idle);
    if (config->player.idle_right[0] == '\0') yaat_copy_string(config->player.idle_right, sizeof(config->player.idle_right), config->player.idle);
    if (config->player.idle_up[0] == '\0') yaat_copy_string(config->player.idle_up, sizeof(config->player.idle_up), config->player.idle);
    if (config->player.idle_down[0] == '\0') yaat_copy_string(config->player.idle_down, sizeof(config->player.idle_down), config->player.idle);
    if (config->player.walk_left[0] == '\0') yaat_copy_string(config->player.walk_left, sizeof(config->player.walk_left), "graphics/sprites/player_walk_left.bmp");
    if (config->player.walk_right[0] == '\0') yaat_copy_string(config->player.walk_right, sizeof(config->player.walk_right), "graphics/sprites/player_walk_right.bmp");
    if (config->player.walk_up[0] == '\0') yaat_copy_string(config->player.walk_up, sizeof(config->player.walk_up), config->player.idle_up);
    if (config->player.walk_down[0] == '\0') yaat_copy_string(config->player.walk_down, sizeof(config->player.walk_down), config->player.idle_down);
    if (config->player.animation_count == 0) yaat_default_player_animations(&config->player);

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
    room->near_y = 200;
    room->near_scale = 1.0;
    room->zmask[0] = '\0';
    room->far_y = 0;
    room->far_scale = 1.0;

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
        } else if (strcmp(section, "scale") == 0 && strcmp(text, "near_y") == 0) {
            room->near_y = atoi(yaat_trim(equals));
        } else if (strcmp(section, "scale") == 0 && strcmp(text, "near_scale") == 0) {
            room->near_scale = atof(yaat_trim(equals));
        } else if (strcmp(section, "scale") == 0 && strcmp(text, "far_y") == 0) {
            room->far_y = atoi(yaat_trim(equals));
        } else if (strcmp(section, "scale") == 0 && strcmp(text, "far_scale") == 0) {
            room->far_scale = atof(yaat_trim(equals));
        } else if (strcmp(section, "entry") == 0 && strcmp(text, "x") == 0) {
            room->entry_x = atoi(yaat_trim(equals));
            room->has_entry_x = 1;
        } else if (strcmp(section, "entry") == 0 && strcmp(text, "y") == 0) {
            room->entry_y = atoi(yaat_trim(equals));
            room->has_entry_y = 1;
        } else if (strcmp(section, "entry") == 0 &&
                   (strcmp(text, "direction") == 0 || strcmp(text, "facing") == 0)) {
            yaat_copy_string(room->entry_direction, sizeof(room->entry_direction), yaat_trim(equals));
        } else if (strcmp(section, "display") == 0 && strcmp(text, "walkmask") == 0) {
            yaat_copy_string(room->walkmask, sizeof(room->walkmask), yaat_trim(equals));
        } else if (strcmp(section, "display") == 0 &&
                   (strcmp(text, "zmask") == 0 || strcmp(text, "depth_mask") == 0)) {
            yaat_copy_string(room->zmask, sizeof(room->zmask), yaat_trim(equals));
        } else if ((strcmp(section, "audio") == 0 || strcmp(section, "display") == 0) &&
                   strcmp(text, "music") == 0) {
            yaat_copy_string(room->music, sizeof(room->music), yaat_trim(equals));
        }
    }
    yaat_asset_buffer_free(&buffer);

    if (room->zmask[0] == '\0') {
        yaat_copy_string(room->zmask, sizeof(room->zmask), "zmask.bmp");
    }

    if (room->id[0] == '\0' || room->background[0] == '\0') {
        yaat_format_asset_error_path(error_path, sizeof(error_path), path,
                                     store != 0 ? store->source : 0);
        yaat_set_error(result, "room.ini is missing required id/background", error_path);
    }
}



static int yaat_parse_semicolon_paths(const char *value,
                                      char paths[][YAAT_ASSET_MAX_PATH],
                                      int max_paths)
{
    char copy[YAAT_LINE_MAX];
    char *part;
    int count;

    if (value == 0 || max_paths <= 0) return 0;
    yaat_copy_string(copy, sizeof(copy), value);
    count = 0;
    part = strtok(copy, ";");
    while (part != 0 && count < max_paths) {
        part = yaat_trim(part);
        if (part[0] != '\0') {
            yaat_copy_string(paths[count], YAAT_ASSET_MAX_PATH, part);
            ++count;
        }
        part = strtok(0, ";");
    }
    return count;
}

static int yaat_parse_semicolon_ints(const char *value, int *items,
                                     int max_items)
{
    char copy[YAAT_LINE_MAX];
    char *part;
    int count;

    if (value == 0 || items == 0 || max_items <= 0) return 0;
    yaat_copy_string(copy, sizeof(copy), value);
    count = 0;
    part = strtok(copy, ";");
    while (part != 0 && count < max_items) {
        part = yaat_trim(part);
        items[count++] = atoi(part);
        part = strtok(0, ";");
    }
    return count;
}

int yaat_runtime_animation_frame_index(const int *timing_ms, int frame_count,
                                       unsigned long elapsed_ms)
{
    int i;
    unsigned long total;
    unsigned long cursor;
    unsigned long duration;

    if (frame_count <= 1 || timing_ms == 0) return 0;
    total = 0;
    for (i = 0; i < frame_count; ++i) {
        total += (unsigned long)(timing_ms[i] > 0 ? timing_ms[i] : 1);
    }
    if (total == 0) return 0;
    cursor = elapsed_ms % total;
    for (i = 0; i < frame_count; ++i) {
        duration = (unsigned long)(timing_ms[i] > 0 ? timing_ms[i] : 1);
        if (cursor < duration) return i;
        cursor -= duration;
    }
    return frame_count - 1;
}


static void yaat_parse_dialog_choices(YaatRuntimeDialogNode *node, const char *value)
{
    char copy[YAAT_LINE_MAX];
    char *part;
    if (node == 0 || value == 0) return;
    node->choice_count = 0;
    yaat_copy_string(copy, sizeof(copy), value);
    part = strtok(copy, ",;");
    while (part != 0 && node->choice_count < YAAT_ASSET_MAX_DIALOG_CHOICES) {
        part = yaat_trim(part);
        if (part[0] != '\0') {
            yaat_copy_string(node->choice_ids[node->choice_count],
                             YAAT_ASSET_MAX_NAME, part);
            ++node->choice_count;
        }
        part = strtok(0, ",;");
    }
}

YaatRuntimeDialogNode *yaat_runtime_dialog_find_node(YaatRuntimeDialog *dialog,
                                                     const char *node_id)
{
    int i;
    if (dialog == 0 || node_id == 0 || node_id[0] == '\0') return 0;
    for (i = 0; i < dialog->node_count; ++i) {
        if (strcmp(dialog->nodes[i].id, node_id) == 0) return &dialog->nodes[i];
    }
    return 0;
}

YaatRuntimeDialogNode *yaat_runtime_dialog_find_choice(YaatRuntimeDialog *dialog,
                                                       const char *choice_id)
{
    return yaat_runtime_dialog_find_node(dialog, choice_id);
}

int yaat_runtime_load_dialog_from_store(YaatAssetStore *store,
                                        const char *dialog_id,
                                        YaatRuntimeDialog *dialog)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char path[YAAT_ASSET_MAX_PATH];
    char filename[YAAT_ASSET_MAX_PATH];
    char line[YAAT_LINE_MAX];
    YaatRuntimeDialogNode *node;

    if (dialog == 0) return 0;
    memset(dialog, 0, sizeof(*dialog));
    if (dialog_id == 0 || dialog_id[0] == '\0' || strstr(dialog_id, "..") != 0 ||
        strchr(dialog_id, '/') != 0 || strchr(dialog_id, '\\') != 0) return 0;
    yaat_copy_string(dialog->id, sizeof(dialog->id), dialog_id);
    yaat_copy_string(filename, sizeof(filename), dialog_id);
    if (strstr(filename, ".ini") == 0) {
        strncat(filename, ".ini", sizeof(filename) - 1 - strlen(filename));
    }
    yaat_join_path(path, sizeof(path), "dialogs", filename);
    if (!yaat_asset_store_load(store, path, &buffer)) return 0;

    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;
    node = 0;
    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
        char *text;
        char *equals;
        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') continue;
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            node = 0;
            if (close != 0 && dialog->node_count < YAAT_ASSET_MAX_DIALOG_NODES) {
                *close = '\0';
                node = &dialog->nodes[dialog->node_count++];
                memset(node, 0, sizeof(*node));
                yaat_copy_string(node->id, sizeof(node->id), yaat_trim(text + 1));
            }
            continue;
        }
        if (node == 0) continue;
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        text = yaat_trim(text);
        equals = yaat_trim(equals + 1);
        if (strcmp(text, "speaker") == 0) yaat_copy_string(node->speaker, sizeof(node->speaker), equals);
        else if (strcmp(text, "text") == 0) yaat_copy_string(node->text, sizeof(node->text), equals);
        else if (strcmp(text, "choices") == 0) yaat_parse_dialog_choices(node, equals);
        else if (strcmp(text, "reply") == 0) yaat_copy_string(node->reply, sizeof(node->reply), equals);
        else if (strcmp(text, "next") == 0) yaat_copy_string(node->next, sizeof(node->next), equals);
        else if (strcmp(text, "event") == 0 || strcmp(text, "script_event") == 0) yaat_copy_string(node->event, sizeof(node->event), equals);
    }
    yaat_asset_buffer_free(&buffer);
    return dialog->node_count > 0;
}
void yaat_runtime_load_inventory_from_store(YaatAssetStore *store,
                                            const char *path,
                                            YaatRuntimeInventory *inventory)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    YaatRuntimeInventoryItem *item;

    if (inventory == 0) return;
    memset(inventory, 0, sizeof(*inventory));
    if (!yaat_asset_store_load(store, path, &buffer)) return;
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;
    item = 0;
    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
        char *text;
        char *equals;
        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') continue;
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            item = 0;
            if (close != 0 && inventory->item_count < YAAT_ASSET_MAX_INVENTORY_ITEMS) {
                *close = '\0';
                item = &inventory->items[inventory->item_count++];
                memset(item, 0, sizeof(*item));
                item->stackable = 1;
                yaat_copy_string(item->id, sizeof(item->id), text + 1);
            }
            continue;
        }
        if (item == 0) continue;
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        text = yaat_trim(text);
        ++equals;
        if (strcmp(text, "name") == 0) yaat_copy_string(item->name, sizeof(item->name), yaat_trim(equals));
        else if (strcmp(text, "icon") == 0) yaat_copy_string(item->icon, sizeof(item->icon), yaat_trim(equals));
        else if (strcmp(text, "frames") == 0) item->frame_count = yaat_parse_semicolon_paths(yaat_trim(equals), item->frames, YAAT_ASSET_MAX_ANIMATION_FRAMES);
        else if (strcmp(text, "timing_ms") == 0) yaat_parse_semicolon_ints(yaat_trim(equals), item->timing_ms, YAAT_ASSET_MAX_ANIMATION_FRAMES);
        else if (strcmp(text, "description") == 0) yaat_copy_string(item->description, sizeof(item->description), yaat_trim(equals));
        else if (strcmp(text, "script") == 0) yaat_copy_string(item->script, sizeof(item->script), yaat_trim(equals));
        else if (strcmp(text, "stackable") == 0) item->stackable = yaat_bool_value(yaat_trim(equals), 1);
    }
    yaat_asset_buffer_free(&buffer);
}

void yaat_runtime_state_init(YaatRuntimeState *state)
{
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    yaat_copy_string(state->language, sizeof(state->language), "en");
    yaat_copy_string(state->strings.language, sizeof(state->strings.language), "en");
}

void yaat_runtime_state_set_language(YaatRuntimeState *state, const char *language)
{
    if (state == 0) return;
    yaat_copy_string(state->language, sizeof(state->language),
                     language != 0 && language[0] != '\0' ? language : "en");
}

int yaat_runtime_load_strings_from_store(YaatAssetStore *store, const char *language,
                                         YaatRuntimeStrings *strings)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char path[YAAT_ASSET_MAX_PATH];
    char line[YAAT_LINE_MAX];
    const char *lang;

    if (strings == 0) return 0;
    memset(strings, 0, sizeof(*strings));
    lang = language != 0 && language[0] != '\0' ? language : "en";
    yaat_copy_string(strings->language, sizeof(strings->language), lang);
    sprintf(path, "strings/%s.ini", lang);
    if (!yaat_asset_store_load(store, path, &buffer)) return 0;
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;
    while (yaat_ini_read_line(&reader, line, sizeof(line))) {
        char *text;
        char *equals;
        text = yaat_trim(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#' || text[0] == '[') continue;
        equals = strchr(text, '=');
        if (equals == 0 || strings->string_count >= YAAT_ASSET_MAX_STRINGS) continue;
        *equals = '\0';
        text = yaat_trim(text);
        equals = yaat_trim(equals + 1);
        if (text[0] == '\0') continue;
        yaat_copy_string(strings->strings[strings->string_count].id,
                         sizeof(strings->strings[0].id), text);
        yaat_copy_string(strings->strings[strings->string_count].text,
                         sizeof(strings->strings[0].text), equals);
        ++strings->string_count;
    }
    yaat_asset_buffer_free(&buffer);
    return 1;
}

const char *yaat_runtime_lookup_string(const YaatRuntimeStrings *strings,
                                       const char *id,
                                       const char *english_text)
{
    int i;
    if (strings != 0 && id != 0 && id[0] != '\0') {
        for (i = 0; i < strings->string_count; ++i) {
            if (strcmp(strings->strings[i].id, id) == 0) return strings->strings[i].text;
        }
    }
    return english_text != 0 ? english_text : "";
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


static void yaat_parse_animation_frames(YaatRuntimeObject *object, const char *value)
{
    char frames[YAAT_LINE_MAX];
    char *frame;
    char *next;

    if (object == 0 || value == 0) {
        return;
    }
    object->animation_frame_count = 0;
    yaat_copy_string(frames, sizeof(frames), value);
    frame = frames;
    while (frame != 0 && object->animation_frame_count < YAAT_ASSET_MAX_ANIMATION_FRAMES) {
        next = strchr(frame, ';');
        if (next != 0) {
            *next = '\0';
            ++next;
        }
        frame = yaat_trim(frame);
        if (frame[0] != '\0') {
            yaat_copy_string(object->animation_frames[object->animation_frame_count],
                             sizeof(object->animation_frames[object->animation_frame_count]),
                             frame);
            ++object->animation_frame_count;
        }
        frame = next;
    }
}

static unsigned long yaat_parse_color_value(const char *value, unsigned long default_value)
{
    const char *text;
    unsigned long color;

    if (value == 0) return default_value;
    text = value;
    while (*text == ' ' || *text == '\t') ++text;
    if (text[0] == '#' && sscanf(text + 1, "%lx", &color) == 1) return color & 0x00ffffffUL;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X') && sscanf(text + 2, "%lx", &color) == 1) return color & 0x00ffffffUL;
    if (sscanf(text, "%lx", &color) == 1) return color & 0x00ffffffUL;
    return default_value;
}

static void yaat_parse_transparency_mode(YaatTransparency *transparency, const char *value)
{
    const char *mode;

    if (transparency == 0) return;
    mode = value != 0 ? value : "";
    if (strcmp(mode, "none") == 0) transparency->mode = YAAT_TRANSPARENCY_NONE;
    else if (strcmp(mode, "color-key") == 0 || strcmp(mode, "color_key") == 0) transparency->mode = YAAT_TRANSPARENCY_COLOR_KEY;
    else if (strcmp(mode, "alpha") == 0) transparency->mode = YAAT_TRANSPARENCY_ALPHA;
    else if (strcmp(mode, "mask") == 0) transparency->mode = YAAT_TRANSPARENCY_MASK;
}

static YaatAnimationClip *yaat_find_or_add_animation(YaatRuntimePlayer *player,
                                                     const char *id)
{
    int i;

    if (player == 0 || id == 0 || id[0] == '\0') {
        return 0;
    }
    for (i = 0; i < player->animation_count; ++i) {
        if (strcmp(player->animations[i].id, id) == 0) {
            return &player->animations[i];
        }
    }
    if (player->animation_count >= YAAT_ASSET_MAX_ANIMATIONS) {
        return 0;
    }
    i = player->animation_count++;
    memset(&player->animations[i], 0, sizeof(player->animations[i]));
    yaat_copy_string(player->animations[i].id,
                     sizeof(player->animations[i].id), id);
    player->animations[i].loop = 1;
    player->animations[i].default_frame_ms = 150;
    return &player->animations[i];
}

static void yaat_add_animation_frame(YaatAnimationClip *clip, const char *path)
{
    YaatAnimationFrame *frame;

    if (clip == 0 || path == 0 || path[0] == '\0' ||
        clip->frame_count >= YAAT_ASSET_MAX_ANIMATION_FRAMES) {
        return;
    }
    frame = &clip->frames[clip->frame_count++];
    memset(frame, 0, sizeof(*frame));
    frame->duration_ms = clip->default_frame_ms > 0 ?
                         clip->default_frame_ms : 150;
    frame->step_pixels = 0;
    yaat_copy_string(frame->path, sizeof(frame->path), path);
}

static void yaat_parse_player_animation_frames(YaatAnimationClip *clip, char *value)
{
    char *token;

    token = strtok(value, ";,");
    while (token != 0) {
        yaat_add_animation_frame(clip, yaat_trim(token));
        token = strtok(0, ";,");
    }
}

static void yaat_parse_animation_frame_value(YaatAnimationClip *clip, int index,
                                             const char *value)
{
    char copy[YAAT_LINE_MAX];
    char *token;
    char *field;
    int part;
    YaatAnimationFrame *frame;

    if (clip == 0 || index < 0 || index >= YAAT_ASSET_MAX_ANIMATION_FRAMES ||
        value == 0) {
        return;
    }
    while (clip->frame_count <= index) {
        yaat_add_animation_frame(clip, " ");
    }
    frame = &clip->frames[index];
    yaat_copy_string(copy, sizeof(copy), value);
    token = strtok(copy, "|");
    part = 0;
    while (token != 0) {
        field = yaat_trim(token);
        if (part == 0) {
            yaat_copy_string(frame->path, sizeof(frame->path), field);
        } else if (part == 1) {
            yaat_parse_rect(field, &frame->x, &frame->y,
                            &frame->width, &frame->height);
        } else if (part == 2) {
            frame->duration_ms = atoi(field);
        } else if (part == 3) {
            frame->step_pixels = atoi(field);
        }
        ++part;
        token = strtok(0, "|");
    }
}

static void yaat_default_player_animations(YaatRuntimePlayer *player)
{
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "idle_left"),
                             player->idle_left);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "idle_right"),
                             player->idle_right);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "idle_up"),
                             player->idle_up);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "idle_down"),
                             player->idle_down);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "idle"),
                             player->idle_down[0] != '\0' ? player->idle_down : player->idle);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "walk_left"),
                             player->walk_left);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "walk_right"),
                             player->walk_right);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "walk_up"),
                             player->walk_up);
    yaat_add_animation_frame(yaat_find_or_add_animation(player, "walk_down"),
                             player->walk_down);
}

static YaatRuntimeHotspot *yaat_find_or_add_room_hotspot(YaatRuntimeRoom *room,
                                                         const char *id)
{
    int i;

    if (room == 0 || id == 0 || id[0] == '\0') {
        return 0;
    }
    for (i = 0; i < room->hotspot_count; ++i) {
        if (strcmp(room->hotspots[i].id, id) == 0) {
            return &room->hotspots[i];
        }
    }
    if (room->hotspot_count >= YAAT_ASSET_MAX_HOTSPOTS) {
        return 0;
    }
    i = room->hotspot_count++;
    memset(&room->hotspots[i], 0, sizeof(room->hotspots[i]));
    yaat_copy_string(room->hotspots[i].id, sizeof(room->hotspots[i].id), id);
    yaat_copy_string(room->hotspots[i].cursor, sizeof(room->hotspots[i].cursor), "arrow");
    return &room->hotspots[i];
}

static void yaat_parse_required_flag(YaatRuntimeHotspot *hotspot,
                                     const char *value)
{
    char copy[YAAT_LINE_MAX];
    char *colon;
    char *name;

    if (hotspot == 0 || value == 0) {
        return;
    }
    yaat_copy_string(copy, sizeof(copy), value);
    colon = strchr(copy, ':');
    if (colon != 0) {
        *colon = '\0';
        hotspot->required_flag_value = yaat_bool_value(yaat_trim(colon + 1), 1);
    } else {
        hotspot->required_flag_value = 1;
    }
    name = yaat_trim(copy);
    yaat_copy_string(hotspot->required_flag, sizeof(hotspot->required_flag), name);
    hotspot->has_required_flag = hotspot->required_flag[0] != '\0';
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
                object->animation_fps = 0;
                object->transparency.mode = YAAT_TRANSPARENCY_ALPHA;
                object->transparency.color_key = 0x00ff00ffUL;
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
        } else if (strcmp(text, "animation") == 0) {
            yaat_copy_string(object->animation, sizeof(object->animation), yaat_trim(equals));
        } else if (strcmp(text, "animation_frames") == 0) {
            yaat_parse_animation_frames(object, yaat_trim(equals));
        } else if (strcmp(text, "animation_fps") == 0) {
            object->animation_fps = atoi(yaat_trim(equals));
        } else if (strcmp(text, "x") == 0) {
            object->x = atoi(yaat_trim(equals));
        } else if (strcmp(text, "y") == 0) {
            object->y = atoi(yaat_trim(equals));
        } else if (strcmp(text, "width") == 0) {
            object->width = atoi(yaat_trim(equals));
        } else if (strcmp(text, "height") == 0) {
            object->height = atoi(yaat_trim(equals));
        } else if (strcmp(text, "walk_x") == 0 || strcmp(text, "use_x") == 0) {
            object->walk_x = atoi(yaat_trim(equals));
            object->has_walk_x = 1;
        } else if (strcmp(text, "walk_y") == 0 || strcmp(text, "use_y") == 0) {
            object->walk_y = atoi(yaat_trim(equals));
            object->has_walk_y = 1;
        } else if (strcmp(text, "visible") == 0) {
            object->visible = yaat_bool_value(yaat_trim(equals), 1);
        } else if (strcmp(text, "inventory_item") == 0 || strcmp(text, "item") == 0) {
            yaat_copy_string(object->inventory_item, sizeof(object->inventory_item), yaat_trim(equals));
        } else if (strcmp(text, "script_event") == 0) {
            yaat_copy_string(object->script_event, sizeof(object->script_event), yaat_trim(equals));
        } else if (strcmp(text, "transparency") == 0) {
            yaat_parse_transparency_mode(&object->transparency, yaat_trim(equals));
        } else if (strcmp(text, "transparent_color") == 0 || strcmp(text, "color_key") == 0) {
            object->transparency.mode = YAAT_TRANSPARENCY_COLOR_KEY;
            object->transparency.color_key = yaat_parse_color_value(yaat_trim(equals), object->transparency.color_key);
        } else if (strcmp(text, "mask") == 0 || strcmp(text, "mask_bitmap") == 0) {
            object->transparency.mode = YAAT_TRANSPARENCY_MASK;
            yaat_copy_string(object->transparency.mask, sizeof(object->transparency.mask), yaat_trim(equals));
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
                yaat_copy_string(hotspot->cursor, sizeof(hotspot->cursor), "arrow");
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
        } else if (strcmp(text, "action") == 0) {
            yaat_copy_string(hotspot->action, sizeof(hotspot->action), yaat_trim(equals));
        } else if (strcmp(text, "target_room") == 0) {
            yaat_copy_string(hotspot->target_room, sizeof(hotspot->target_room), yaat_trim(equals));
        } else if (strcmp(text, "target_x") == 0) {
            hotspot->target_x = atoi(yaat_trim(equals));
            hotspot->has_target_x = 1;
        } else if (strcmp(text, "target_y") == 0) {
            hotspot->target_y = atoi(yaat_trim(equals));
            hotspot->has_target_y = 1;
        } else if (strcmp(text, "target_direction") == 0 ||
                   strcmp(text, "direction") == 0 ||
                   strcmp(text, "facing") == 0 || strcmp(text, "face") == 0) {
            yaat_copy_string(hotspot->target_direction,
                             sizeof(hotspot->target_direction), yaat_trim(equals));
        } else if (strcmp(text, "walk_x") == 0 || strcmp(text, "use_x") == 0) {
            hotspot->walk_x = atoi(yaat_trim(equals));
            hotspot->has_walk_x = 1;
        } else if (strcmp(text, "walk_y") == 0 || strcmp(text, "use_y") == 0) {
            hotspot->walk_y = atoi(yaat_trim(equals));
            hotspot->has_walk_y = 1;
        }
    }
    yaat_asset_buffer_free(&buffer);
}

static void yaat_load_room_exits(YaatAssetStore *store, const char *path,
                                 YaatRuntimeRoom *room)
{
    YaatAssetBuffer buffer;
    YaatIniReader reader;
    char line[YAAT_LINE_MAX];
    char section[YAAT_ASSET_MAX_NAME];
    YaatRuntimeHotspot *hotspot;

    if (!yaat_asset_store_load(store, path, &buffer)) {
        return;
    }
    reader.data = (const char *)buffer.data;
    reader.size = buffer.size;
    reader.offset = 0;
    section[0] = '\0';
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
        equals = yaat_trim(equals);

        if (hotspot == 0) {
            if (strcmp(text, "hotspot") == 0) {
                hotspot = yaat_find_or_add_room_hotspot(room, equals);
            } else {
                hotspot = yaat_find_or_add_room_hotspot(room, section);
            }
        }
        if (hotspot == 0) {
            continue;
        }

        if (strcmp(text, "hotspot") == 0) {
            hotspot = yaat_find_or_add_room_hotspot(room, equals);
            continue;
        }
        if (strcmp(text, "to") == 0 || strcmp(text, "target_room") == 0) {
            yaat_copy_string(hotspot->action, sizeof(hotspot->action), "change_room");
            yaat_copy_string(hotspot->target_room, sizeof(hotspot->target_room), equals);
        } else if (strcmp(text, "target_x") == 0) {
            hotspot->target_x = atoi(equals);
            hotspot->has_target_x = 1;
        } else if (strcmp(text, "target_y") == 0) {
            hotspot->target_y = atoi(equals);
            hotspot->has_target_y = 1;
        } else if (strcmp(text, "target_direction") == 0 ||
                   strcmp(text, "direction") == 0 ||
                   strcmp(text, "facing") == 0 || strcmp(text, "face") == 0) {
            yaat_copy_string(hotspot->target_direction,
                             sizeof(hotspot->target_direction), equals);
        } else if (strcmp(text, "walk_x") == 0 || strcmp(text, "use_x") == 0) {
            hotspot->walk_x = atoi(equals);
            hotspot->has_walk_x = 1;
        } else if (strcmp(text, "walk_y") == 0 || strcmp(text, "use_y") == 0) {
            hotspot->walk_y = atoi(equals);
            hotspot->has_walk_y = 1;
        } else if (strcmp(text, "x") == 0) {
            hotspot->x = atoi(equals);
        } else if (strcmp(text, "y") == 0) {
            hotspot->y = atoi(equals);
        } else if (strcmp(text, "width") == 0) {
            hotspot->width = atoi(equals);
        } else if (strcmp(text, "height") == 0) {
            hotspot->height = atoi(equals);
        } else if (strcmp(text, "rect") == 0) {
            yaat_parse_rect(equals, &hotspot->x, &hotspot->y,
                            &hotspot->width, &hotspot->height);
        } else if (strcmp(text, "requires_flag") == 0) {
            yaat_parse_required_flag(hotspot, equals);
        }
    }
    yaat_asset_buffer_free(&buffer);
}

static void yaat_runtime_load_room_assets(YaatAssetStore *store, const char *rooms_path,
                                        const char *room_id,
                                        YaatRuntimeLoadResult *result)
{
    char room_dir[YAAT_ASSET_MAX_PATH];
    char room_ini[YAAT_ASSET_MAX_PATH];
    char objects_ini[YAAT_ASSET_MAX_PATH];
    char hotspots_ini[YAAT_ASSET_MAX_PATH];
    char exits_ini[YAAT_ASSET_MAX_PATH];

    yaat_join_path(room_dir, sizeof(room_dir), rooms_path, room_id);
    yaat_join_path(room_ini, sizeof(room_ini), room_dir, "room.ini");
    yaat_join_path(objects_ini, sizeof(objects_ini), room_dir, "objects.ini");
    yaat_join_path(hotspots_ini, sizeof(hotspots_ini), room_dir, "hotspots.ini");
    yaat_join_path(exits_ini, sizeof(exits_ini), room_dir, "exits.ini");

    yaat_copy_string(result->room.room_path, sizeof(result->room.room_path), room_dir);
    yaat_load_room_ini(store, room_ini, &result->room, result);
    if (!result->ok) return;
    yaat_load_room_objects(store, objects_ini, &result->room);
    yaat_load_room_hotspots(store, hotspots_ini, &result->room);
    yaat_load_room_exits(store, exits_ini, &result->room);
    yaat_runtime_load_inventory_from_store(store, "inventory/items.ini", &result->inventory);
}

void yaat_runtime_load_room_from_store(YaatAssetStore *store,
                                       const char *room_id,
                                       YaatRuntimeLoadResult *result)
{
    YaatGameConfig config;

    if (result == 0) return;
    memset(result, 0, sizeof(*result));
    result->ok = 1;
    if (room_id == 0 || room_id[0] == '\0' || strchr(room_id, '/') != 0 ||
        strchr(room_id, '\\') != 0) {
        yaat_set_error(result, "Invalid room id", room_id);
        return;
    }
    yaat_load_game_config(store, "game.ini", 0, &config, result);
    if (!result->ok) return;
    result->player = config.player;
    yaat_runtime_load_room_assets(store, config.rooms_path, room_id, result);
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
    result->player = config.player;
    yaat_runtime_load_room_assets(store, config.rooms_path, config.first_room, result);
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
