#include "save_state.h"

#ifdef YAAT_EMBEDDED_MODULE
static YaatSavedRuntimeObjectState g_saved_runtime_objects[YAAT_MAX_SAVED_RUNTIME_OBJECTS];
static int g_saved_runtime_object_count;
static int g_suppress_runtime_state_capture;

static int yaat_saved_runtime_object_index(const char *room_id,
                                           const char *object_id)
{
    int i;
    for (i = 0; i < g_saved_runtime_object_count; ++i) {
        if (strcmp(g_saved_runtime_objects[i].room_id, room_id) == 0 &&
            strcmp(g_saved_runtime_objects[i].object_id, object_id) == 0) {
            return i;
        }
    }
    return -1;
}

static void yaat_capture_runtime_object_state(void)
{
    int i;

    if (g_suppress_runtime_state_capture) return;
    if (!g_runtime_load.ok || g_runtime_load.room.id[0] == '\0') return;
    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
        int idx;
        if (object->id[0] == '\0') continue;
        idx = yaat_saved_runtime_object_index(g_runtime_load.room.id, object->id);
        if (idx < 0) {
            if (g_saved_runtime_object_count >= YAAT_MAX_SAVED_RUNTIME_OBJECTS) continue;
            idx = g_saved_runtime_object_count++;
            memset(&g_saved_runtime_objects[idx], 0, sizeof(g_saved_runtime_objects[idx]));
            yaat_copy(g_saved_runtime_objects[idx].room_id,
                      sizeof(g_saved_runtime_objects[idx].room_id),
                      g_runtime_load.room.id, strlen(g_runtime_load.room.id));
            yaat_copy(g_saved_runtime_objects[idx].object_id,
                      sizeof(g_saved_runtime_objects[idx].object_id),
                      object->id, strlen(object->id));
        }
        g_saved_runtime_objects[idx].x = object->x;
        g_saved_runtime_objects[idx].y = object->y;
        g_saved_runtime_objects[idx].width = object->width;
        g_saved_runtime_objects[idx].height = object->height;
        g_saved_runtime_objects[idx].visible = object->visible;
        g_saved_runtime_objects[idx].transparency_mode = (int)object->transparency.mode;
        g_saved_runtime_objects[idx].transparency_color_key = object->transparency.color_key;
        g_saved_runtime_objects[idx].transparent_color_enabled = object->transparent_color_enabled;
        g_saved_runtime_objects[idx].transparent_color = object->transparent_color;
    }
}

static void yaat_apply_runtime_object_state(void)
{
    int i;

    if (!g_runtime_load.ok || g_runtime_load.room.id[0] == '\0') return;
    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
        int idx;
        if (object->id[0] == '\0') continue;
        idx = yaat_saved_runtime_object_index(g_runtime_load.room.id, object->id);
        if (idx < 0) continue;
        object->x = g_saved_runtime_objects[idx].x;
        object->y = g_saved_runtime_objects[idx].y;
        object->width = g_saved_runtime_objects[idx].width;
        object->height = g_saved_runtime_objects[idx].height;
        object->visible = g_saved_runtime_objects[idx].visible;
        object->transparency.mode =
            (YaatTransparencyMode)g_saved_runtime_objects[idx].transparency_mode;
        object->transparency.color_key =
            g_saved_runtime_objects[idx].transparency_color_key;
        object->transparent_color_enabled =
            g_saved_runtime_objects[idx].transparent_color_enabled;
        object->transparent_color = g_saved_runtime_objects[idx].transparent_color;
    }
}

static int yaat_room_index_by_id(const char *id)
{
    int i;
    for (i = 0; i < g_room_count; ++i) if (strcmp(g_rooms[i].id, id) == 0) return i;
    return -1;
}

static void yaat_runtime_request_room_assets(const char *room_id);


static void yaat_save_slot_path(int slot, char *path, size_t path_size)
{
    if (path_size == 0) return;
    if (slot <= 0) {
        yaat_copy(path, path_size, YAAT_SAVE_PATH, strlen(YAAT_SAVE_PATH));
        return;
    }
    sprintf(path, "yaat_save_state_slot%d.txt", slot + 1);
}

static void yaat_default_save_slot_label(int slot, char *label, size_t label_size)
{
    if (label_size == 0) return;
    sprintf(label, "Slot %d", slot + 1);
}

static void yaat_current_timestamp(char *timestamp, size_t timestamp_size)
{
    SYSTEMTIME time;
    if (timestamp_size == 0) return;
    GetLocalTime(&time);
    sprintf(timestamp, "%04u-%02u-%02u %02u:%02u",
            (unsigned)time.wYear, (unsigned)time.wMonth, (unsigned)time.wDay,
            (unsigned)time.wHour, (unsigned)time.wMinute);
}

static const char *yaat_current_room_id(void)
{
    return (g_current_room >= 0 && g_current_room < g_room_count) ?
           g_rooms[g_current_room].id : "";
}

static const char *yaat_current_room_label(void)
{
    if (g_runtime_load.ok && g_runtime_load.room.label[0] != '\0') return g_runtime_load.room.label;
    return yaat_current_room_id();
}

static void yaat_get_ui_string(const char *key, const char *fallback,
                               char *out, size_t out_size)
{
    FILE *file;
    char line[256];
    size_t key_len;

    if (out_size == 0) return;
    yaat_copy(out, out_size, fallback, strlen(fallback));
    file = fopen("game/strings/en.ini", "r");
    if (file == 0) return;
    key_len = strlen(key);
    while (fgets(line, sizeof(line), file) != 0) {
        char *text = yaat_trim_text(line);
        if (strncmp(text, key, key_len) == 0 && text[key_len] == '=') {
            char *value = yaat_trim_text(text + key_len + 1);
            yaat_copy(out, out_size, value, strlen(value));
            break;
        }
    }
    fclose(file);
}

static void yaat_read_save_slot_info(int slot, YaatSaveSlotInfo *info)
{
    FILE *file;
    char path[64];
    char key[64];
    int version;

    memset(info, 0, sizeof(*info));
    yaat_default_save_slot_label(slot, info->label, sizeof(info->label));
    yaat_save_slot_path(slot, path, sizeof(path));
    file = fopen(path, "r");
    if (file == 0) return;
    if (fscanf(file, "%63s %d", key, &version) == 2 &&
        strcmp(key, "YAAT_SCRIPT_STATE") == 0 && version == YAAT_SAVE_STATE_VERSION) {
        info->exists = 1;
        while (fscanf(file, "%63s", key) == 1) {
            if (strcmp(key, "meta_label") == 0) {
                if (fscanf(file, "%31s", info->label) != 1) info->label[0] = '\0';
            } else if (strcmp(key, "meta_room") == 0) {
                if (fscanf(file, "%63s", info->room) != 1) info->room[0] = '\0';
            } else if (strcmp(key, "meta_play_time_ms") == 0) {
                fscanf(file, "%lu", &info->play_time_ms);
            } else if (strcmp(key, "meta_timestamp") == 0) {
                if (fscanf(file, "%31s", info->timestamp) == 1) {
                    char time_part[16];
                    if (fscanf(file, "%15s", time_part) == 1 &&
                        strlen(info->timestamp) + 1 + strlen(time_part) < sizeof(info->timestamp)) {
                        strcat(info->timestamp, " ");
                        strcat(info->timestamp, time_part);
                    }
                }
            } else if (strcmp(key, "player") == 0) {
                break;
            }
        }
    }
    fclose(file);
    if (info->label[0] == '\0') yaat_default_save_slot_label(slot, info->label, sizeof(info->label));
}

static int yaat_save_script_state(const char *path, const char *slot_label)
{
    FILE *file;
    int i;
    int j;

    yaat_capture_runtime_object_state();
    file = fopen(path, "w");
    if (file == 0) return 0;

    fprintf(file, "YAAT_SCRIPT_STATE %d\n", YAAT_SAVE_STATE_VERSION);
    {
        char timestamp[32];
        yaat_current_timestamp(timestamp, sizeof(timestamp));
        fprintf(file, "meta_label %s\n", slot_label != 0 && slot_label[0] != '\0' ? slot_label : "Slot");
        fprintf(file, "meta_room %s\n", yaat_current_room_id());
        fprintf(file, "meta_play_time_ms %lu\n", g_animation_clock_ms);
        fprintf(file, "meta_timestamp %s\n", timestamp);
    }
    fprintf(file, "player %d %d %d %d %d %s\n", g_player_x, g_player_y,
            g_target_x, g_target_y, g_player_facing_right,
            g_player_animation_id);
    fprintf(file, "current_room %s\n",
            (g_current_room >= 0 && g_current_room < g_room_count) ?
            g_rooms[g_current_room].id : "");
    fprintf(file, "selected_verb %s\n", g_selected_verb);
    fprintf(file, "selected_inventory %s\n",
            g_selected_inventory[0] != '\0' ? g_selected_inventory : "-");

    fprintf(file, "vars %d\n", g_var_count);
    for (i = 0; i < g_var_count; ++i) {
        fprintf(file, "var %s %d\n", g_vars[i].name, g_vars[i].value.bool_value);
    }

    fprintf(file, "inventory %d\n", g_inventory_count);
    for (i = 0; i < g_inventory_count; ++i) {
        fprintf(file, "item %s\n", g_inventory[i]);
    }

    fprintf(file, "entities\n");
    for (i = 0; i < g_room_count; ++i) {
        for (j = 0; j < g_rooms[i].entity_count; ++j) {
            YaatEntity *entity = &g_rooms[i].entities[j];
            fprintf(file, "entity %s %s %d %d %d %d %d\n",
                    g_rooms[i].id, entity->id, entity->x, entity->y,
                    entity->w, entity->h, entity->visible);
        }
    }

    fprintf(file, "runtime_objects %d\n", g_saved_runtime_object_count);
    for (i = 0; i < g_saved_runtime_object_count; ++i) {
        YaatSavedRuntimeObjectState *object = &g_saved_runtime_objects[i];
        fprintf(file, "runtime_object %s %s %d %d %d %d %d %d %lu %d %lu\n",
                object->room_id, object->object_id, object->x, object->y,
                object->width, object->height, object->visible,
                object->transparency_mode, object->transparency_color_key,
                object->transparent_color_enabled, object->transparent_color);
    }

    fclose(file);
    return 1;
}

static int yaat_load_script_state(const char *path)
{
    FILE *file;
    char key[64];
    char value[YAAT_ASSET_MAX_NAME];
    int version;
    int expected;
    int i;

    file = fopen(path, "r");
    if (file == 0) return 0;
    if (fscanf(file, "%63s %d", key, &version) != 2 ||
        strcmp(key, "YAAT_SCRIPT_STATE") != 0 || version != YAAT_SAVE_STATE_VERSION) {
        fclose(file);
        return 0;
    }

    g_var_count = 0;
    g_inventory_count = 0;
    g_selected_inventory[0] = '\0';
    g_saved_runtime_object_count = 0;

    while (fscanf(file, "%63s", key) == 1) {
        if (strncmp(key, "meta_", 5) == 0) {
            char discard[128];
            fgets(discard, sizeof(discard), file);
        } else if (strcmp(key, "player") == 0) {
            fscanf(file, "%d %d %d %d %d %63s", &g_player_x, &g_player_y,
                   &g_target_x, &g_target_y, &g_player_facing_right,
                   g_player_animation_id);
        } else if (strcmp(key, "current_room") == 0) {
            if (fscanf(file, "%63s", value) == 1) {
                int idx = yaat_room_index_by_id(value);
                if (idx >= 0) g_current_room = idx;
            }
        } else if (strcmp(key, "selected_verb") == 0) {
            if (fscanf(file, "%31s", g_selected_verb) != 1) g_selected_verb[0] = '\0';
        } else if (strcmp(key, "selected_inventory") == 0) {
            if (fscanf(file, "%31s", g_selected_inventory) == 1 &&
                strcmp(g_selected_inventory, "-") == 0) {
                g_selected_inventory[0] = '\0';
            }
        } else if (strcmp(key, "vars") == 0) {
            fscanf(file, "%d", &expected);
            (void)expected;
        } else if (strcmp(key, "var") == 0) {
            int bool_value;
            if (fscanf(file, "%31s %d", value, &bool_value) == 2) {
                yaat_set_var(value, bool_value);
            }
        } else if (strcmp(key, "inventory") == 0) {
            fscanf(file, "%d", &expected);
            (void)expected;
        } else if (strcmp(key, "item") == 0) {
            if (fscanf(file, "%31s", value) == 1) yaat_take_inventory(value);
        } else if (strcmp(key, "entities") == 0) {
            continue;
        } else if (strcmp(key, "entity") == 0) {
            char room_id[32];
            char entity_id[32];
            int room_index;
            int x;
            int y;
            int w;
            int h;
            int visible;
            if (fscanf(file, "%31s %31s %d %d %d %d %d", room_id, entity_id,
                       &x, &y, &w, &h, &visible) == 7) {
                room_index = yaat_room_index_by_id(room_id);
                if (room_index >= 0) {
                    YaatEntity *entity = yaat_entity_by_id(&g_rooms[room_index], entity_id);
                    if (entity != 0) {
                        entity->x = x;
                        entity->y = y;
                        entity->w = w;
                        entity->h = h;
                        entity->visible = visible;
                    }
                }
            }
        } else if (strcmp(key, "runtime_objects") == 0) {
            fscanf(file, "%d", &expected);
            (void)expected;
        } else if (strcmp(key, "runtime_object") == 0) {
            if (g_saved_runtime_object_count < YAAT_MAX_SAVED_RUNTIME_OBJECTS) {
                YaatSavedRuntimeObjectState *object =
                    &g_saved_runtime_objects[g_saved_runtime_object_count];
                if (fscanf(file, "%63s %63s %d %d %d %d %d %d %lu %d %lu",
                           object->room_id, object->object_id, &object->x,
                           &object->y, &object->width, &object->height,
                           &object->visible, &object->transparency_mode,
                           &object->transparency_color_key,
                           &object->transparent_color_enabled,
                           &object->transparent_color) == 11) {
                    ++g_saved_runtime_object_count;
                }
            }
        }
    }
    fclose(file);

    if (g_room_count > 0) {
        g_suppress_runtime_state_capture = 1;
        yaat_runtime_request_room_assets(g_rooms[g_current_room].id);
        g_suppress_runtime_state_capture = 0;
        yaat_apply_runtime_object_state();
    }
    for (i = 0; i < g_inventory_count; ++i) {
        if (strcmp(g_inventory[i], g_selected_inventory) == 0) break;
    }
    if (i == g_inventory_count) g_selected_inventory[0] = '\0';
    return 1;
}


#endif
