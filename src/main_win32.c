#include <windows.h>

#include "platform/win32/gdi_renderer.h"
#include "platform/win32/audio_winmm.h"
#include "script_parser.h"
#include "script_bytecode.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "runtime/asset_loader.h"
#include "runtime/navigation.h"
#include "runtime/save_state.h"
#include "runtime/dialogue_runtime.h"
#include "platform/win32/bitmap_assets.h"

#define YAAT_WINDOW_CLASS_NAME "YAATWindowClass"
#define YAAT_WINDOW_TITLE "YAAT"
#ifndef IDC_HAND
#define IDC_HAND IDC_ARROW
#endif
#define YAAT_BACKBUFFER_WIDTH 320
#define YAAT_BACKBUFFER_HEIGHT 240
#define YAAT_PLAYFIELD_HEIGHT 200
#define YAAT_PLAYER_WIDTH 18
#define YAAT_PLAYER_HEIGHT 34
#define YAAT_PLAYER_SPEED_PIXELS 4
#define YAAT_FRAME_TIMER_ID 1
#define YAAT_FRAME_TIMER_MS 16
#define YAAT_SPLASH_DURATION_MS 2000
#define YAAT_SPLASH_TITLE "YAAT"
#define YAAT_SPLASH_SUBTITLE "Yet Another Adventure Tool"
#define YAAT_MAX_VERBS 8
#define YAAT_VERB_BUTTON_WIDTH 50
#define YAAT_VERB_BUTTON_HEIGHT 13
#define YAAT_INVENTORY_SLOT_SIZE 20
#define YAAT_MAX_RUNTIME_OBJECT_MUTATIONS 64
#define YAAT_COMMAND_FEEDBACK_MAX 256

typedef struct YaatRuntimeObjectMutation {
    char room_id[YAAT_ASSET_MAX_NAME];
    char object_id[YAAT_ASSET_MAX_NAME];
    int has_visible;
    int visible;
    int has_position;
    int x;
    int y;
    int has_sprite;
    char sprite[YAAT_ASSET_MAX_PATH];
    int has_animation;
    char animation[YAAT_ASSET_MAX_NAME];
} YaatRuntimeObjectMutation;

static YaatGdiRenderer g_renderer;
static int g_renderer_ready;
static int g_player_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_player_y = YAAT_PLAYFIELD_HEIGHT / 2;
static int g_target_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_target_y = YAAT_PLAYFIELD_HEIGHT / 2;
static int g_player_facing_right = 1;
static int g_player_facing_vertical = 1; /* -1 away/up, 1 toward/down, 0 sideways */
static char g_player_animation_id[YAAT_ASSET_MAX_NAME] = "idle";
static int g_player_animation_frame;
static unsigned long g_player_animation_elapsed_ms;
static int g_cursor_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_cursor_y = YAAT_PLAYFIELD_HEIGHT / 2;
static YaatRoom g_rooms[YAAT_MAX_ROOMS];
static int g_room_count;
static int g_current_room;
static YaatCommand g_commands[YAAT_MAX_COMMANDS];
static int g_command_count;
static YaatEvent g_global_events[YAAT_MAX_GLOBAL_EVENTS];
static int g_global_event_count;
static int g_script_call_depth;
static YaatVar g_vars[YAAT_MAX_VARS];
static int g_var_count;
static char g_inventory[YAAT_MAX_INVENTORY][32];
static int g_inventory_count;
static YaatRuntimeLoadResult g_runtime_load;
static YaatRuntimeState g_runtime_state;
static YaatAssetStore g_asset_store;
static YaatAssetStore g_runtime_asset_store;
static YaatWinmmAudio g_audio;
static YaatRuntimeHotspot g_runtime_hotspots[YAAT_MAX_RUNTIME_HOTSPOTS];
static int g_runtime_hotspot_count;
static char g_cursor_state[32] = "arrow";
static char g_verbs[YAAT_MAX_VERBS][32];
static int g_verb_count;
static char g_selected_verb[32];
static char g_selected_inventory[32];
static int g_fullscreen;
static RECT g_windowed_rect;
static DWORD g_windowed_style;
static int g_shake_duration_ms;
static int g_shake_magnitude;
static int g_shake_elapsed_ms;
static int g_shake_offset_x;
static int g_shake_offset_y;
static YaatRuntimeObjectMutation g_runtime_object_mutations[YAAT_MAX_RUNTIME_OBJECT_MUTATIONS];
static int g_runtime_object_mutation_count;
static int g_pending_room_change;
static char g_pending_room_change_hotspot_id[YAAT_ASSET_MAX_NAME];
static char g_suppressed_room_change_hotspot_id[YAAT_ASSET_MAX_NAME];
static int g_pending_interaction;
static int g_pending_interaction_x;
static int g_pending_interaction_y;
static int g_player_visible = 1;
static int g_cutscene_overlay_visible;
static char g_cutscene_overlay_text[YAAT_TEXT_MAX];
static int g_cutscene_overlay_remaining_ms;
static int g_script_wait_remaining_ms;
static int g_script_resume_active;
static int g_script_resume_first;
static int g_script_resume_count;
static int g_script_resume_index;
static int g_splash_remaining_ms = YAAT_SPLASH_DURATION_MS;
static YaatSavedRuntimeObjectState g_saved_runtime_objects[YAAT_MAX_SAVED_RUNTIME_OBJECTS];
static int g_saved_runtime_object_count;
static int g_suppress_runtime_state_capture;
static char g_dialogue_speaker[32];
static char g_dialogue_text[YAAT_TEXT_MAX];
static int g_dialogue_visible;
static int g_dialogue_choice_visible;
static YaatRuntimeDialog g_active_dialog;
static char g_active_dialog_node[YAAT_ASSET_MAX_NAME];

typedef enum YaatHoverTargetKind {
    YAAT_HOVER_EMPTY = 0,
    YAAT_HOVER_OBJECT,
    YAAT_HOVER_HOTSPOT,
    YAAT_HOVER_INVENTORY
} YaatHoverTargetKind;

static YaatHoverTargetKind g_hover_target_kind = YAAT_HOVER_EMPTY;
static char g_hover_target_id[YAAT_ASSET_MAX_NAME];
static char g_hover_target_name[YAAT_ASSET_MAX_NAME];
static char g_command_feedback[YAAT_COMMAND_FEEDBACK_MAX];
static char g_command_feedback_override[YAAT_COMMAND_FEEDBACK_MAX];
static int g_command_feedback_override_remaining_ms;

static YaatSaveMenuMode g_save_menu_mode;
static int g_save_menu_selected_slot;

typedef struct YaatViewport { int x; int y; int width; int height; } YaatViewport;
static YaatBitmap g_background_bitmap;
static YaatBitmap g_player_bitmap;
static unsigned long g_animation_clock_ms;
static YaatBitmap g_walkmask_bitmap;
static YaatBitmap g_font_bitmap;
static int g_player_transparent_color_enabled;
static unsigned long g_player_transparent_color;

#define YAAT_FONT_PATH "graphics/ui/font.bmp"
#define YAAT_FONT_GLYPH_FIRST 32
#define YAAT_FONT_GLYPH_COUNT 96
#define YAAT_FONT_COLUMNS 16
#define YAAT_FONT_CELL_WIDTH 8
#define YAAT_FONT_CELL_HEIGHT 8
#define YAAT_FONT_TRANSPARENT 0x00ff00ffUL

static void yaat_runtime_join_path(char *dst, size_t dst_size,
                                   const char *left, const char *right);
static const char *yaat_runtime_logical_path(const char *path);
static void yaat_draw_bitmap_transparent(YaatBitmap *bitmap, int dst_x, int dst_y,
                                         const YaatTransparency *transparency,
                                         const char *mask_base_path);
static void yaat_draw_bitmap_transparent_scaled(YaatBitmap *bitmap, int dst_x, int dst_y,
                                                int dst_width, int dst_height,
                                                const YaatTransparency *transparency,
                                                const char *mask_base_path);
static char *yaat_trim_text(char *text);
static int yaat_load_bmp(YaatBitmap *bitmap, const char *path);
static int yaat_player_motion_complete(void);
static void yaat_unload_bitmap(YaatBitmap *bitmap);

static void yaat_longclick_game(int x, int y);
static int yaat_runtime_longclick_game(int x, int y);
static void yaat_execute_entity_verb(YaatEntity *entity, const char *verb,
                                     const char *item, const char *default_verb);
static void yaat_click_game(int x, int y, int immediate_room_change);
static void yaat_draw_splash_screen(void);
static YaatRuntimeHotspot *yaat_runtime_hotspot_by_id(const char *id);
static void yaat_pending_room_change_maybe_complete(void);
static void yaat_room_change_region_maybe_enter(void);
static void yaat_pending_interaction_maybe_complete(void);
static void yaat_update_script_timers(void);
static void yaat_open_save_menu(YaatSaveMenuMode mode);
static const char *yaat_active_verb(void);
static void yaat_update_command_feedback(void);
static void yaat_update_hover_target_at(int backbuffer_x, int backbuffer_y);
static const char *yaat_player_walk_animation_for_delta(int dx, int dy);
static const char *yaat_player_idle_animation(void);
static void yaat_player_face_direction(const char *direction);
static int yaat_player_current_step_pixels(void);
static YaatEntity *yaat_entity_by_id(YaatRoom *room, const char *id);
static YaatRuntimeObject *yaat_runtime_object_by_id(const char *id);
static YaatEntity *yaat_entity_by_id_any_room(const char *id);
static int yaat_room_index_by_id(const char *id);
static int yaat_dialogue_position_for_speaker(int *dialogue_x, int *dialogue_y);

static int yaat_clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}


static int yaat_shake_sample(int seed, int magnitude)
{
    unsigned long value;
    if (magnitude <= 0) return 0;
    value = (unsigned long)seed * 1103515245UL + 12345UL;
    return (int)((value >> 16) % (unsigned long)((magnitude * 2) + 1)) - magnitude;
}


static double yaat_room_scale_for_y(const YaatRuntimeRoom *room, int y)
{
    double t;

    if (room == 0 || room->near_y == room->far_y) {
        return 1.0;
    }
    t = (double)(y - room->far_y) / (double)(room->near_y - room->far_y);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return room->far_scale + ((room->near_scale - room->far_scale) * t);
}

static void yaat_start_shake(int duration_ms, int magnitude)
{
    g_shake_duration_ms = yaat_clamp_int(duration_ms, 0, 10000);
    g_shake_magnitude = yaat_clamp_int(magnitude, 0, 32);
    g_shake_elapsed_ms = 0;
    g_shake_offset_x = 0;
    g_shake_offset_y = 0;
}

static void yaat_update_shake(void)
{
    int remaining;
    int magnitude;

    if (g_shake_elapsed_ms >= g_shake_duration_ms || g_shake_magnitude <= 0) {
        g_shake_duration_ms = 0;
        g_shake_elapsed_ms = 0;
        g_shake_offset_x = 0;
        g_shake_offset_y = 0;
        return;
    }

    g_shake_elapsed_ms += YAAT_FRAME_TIMER_MS;
    if (g_shake_elapsed_ms >= g_shake_duration_ms) {
        g_shake_offset_x = 0;
        g_shake_offset_y = 0;
        return;
    }

    remaining = g_shake_duration_ms - g_shake_elapsed_ms;
    magnitude = (g_shake_magnitude * remaining) / g_shake_duration_ms;
    if (magnitude < 1) magnitude = 1;
    g_shake_offset_x = yaat_clamp_int(yaat_shake_sample(g_shake_elapsed_ms + 17, magnitude), -32, 32);
    g_shake_offset_y = yaat_clamp_int(yaat_shake_sample(g_shake_elapsed_ms + 53, magnitude), -32, 32);
}

static double yaat_clamp_double(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void yaat_copy(char *dst, size_t dst_size, const char *src, size_t len)
{
    if (dst_size == 0) return;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void yaat_draw_rect(YaatGdiRenderer *renderer, int x, int y,
                           int width, int height, unsigned long color)
{
    int clipped_x0;
    int clipped_y0;
    int clipped_x1;
    int clipped_y1;
    int draw_x;
    int draw_y;
    unsigned long *row;

    if (renderer == 0 || renderer->pixels == 0 || width <= 0 || height <= 0) return;
    clipped_x0 = yaat_clamp_int(x, 0, renderer->width);
    clipped_y0 = yaat_clamp_int(y, 0, renderer->height);
    clipped_x1 = yaat_clamp_int(x + width, 0, renderer->width);
    clipped_y1 = yaat_clamp_int(y + height, 0, renderer->height);
    for (draw_y = clipped_y0; draw_y < clipped_y1; ++draw_y) {
        row = (unsigned long *)((unsigned char *)renderer->pixels + (draw_y * renderer->pitch));
        for (draw_x = clipped_x0; draw_x < clipped_x1; ++draw_x) row[draw_x] = color;
    }
}

static void yaat_draw_text_block(int x, int y, const char *text, unsigned long color)
{
    int i;
    int cx = x;
    int cy = y;
    int font_ready;

    font_ready = g_font_bitmap.pixels != 0 ||
                 yaat_load_bmp(&g_font_bitmap, YAAT_FONT_PATH);

    if (text == 0) return;
    for (i = 0; text[i] != '\0' && cy < YAAT_BACKBUFFER_HEIGHT - YAAT_FONT_CELL_HEIGHT; ++i) {
        unsigned char ch;

        if (text[i] == '\n' || cx > YAAT_BACKBUFFER_WIDTH - YAAT_FONT_CELL_WIDTH) {
            cx = x;
            cy += YAAT_FONT_CELL_HEIGHT;
            if (text[i] == '\n') continue;
        }
        ch = (unsigned char)text[i];
        if (ch != ' ') {
            if (font_ready) {
                int glyph_index;
                int glyph_x;
                int glyph_y;
                int row;

                if (ch < YAAT_FONT_GLYPH_FIRST ||
                    ch >= YAAT_FONT_GLYPH_FIRST + YAAT_FONT_GLYPH_COUNT) {
                    ch = '?';
                }
                glyph_index = (int)ch - YAAT_FONT_GLYPH_FIRST;
                glyph_x = (glyph_index % YAAT_FONT_COLUMNS) * YAAT_FONT_CELL_WIDTH;
                glyph_y = (glyph_index / YAAT_FONT_COLUMNS) * YAAT_FONT_CELL_HEIGHT;

                for (row = 0; row < YAAT_FONT_CELL_HEIGHT; ++row) {
                    int dst_y;
                    int col;

                    dst_y = cy + row;
                    if (dst_y < 0 || dst_y >= g_renderer.height) continue;
                    for (col = 0; col < YAAT_FONT_CELL_WIDTH; ++col) {
                        int dst_x;
                        unsigned long pixel;
                        unsigned long *dst_row;

                        dst_x = cx + col;
                        if (dst_x < 0 || dst_x >= g_renderer.width) continue;
                        pixel = g_font_bitmap.pixels[((glyph_y + row) * g_font_bitmap.width) + glyph_x + col] & 0x00ffffffUL;
                        if (pixel == YAAT_FONT_TRANSPARENT) continue;
                        dst_row = (unsigned long *)((unsigned char *)g_renderer.pixels +
                                  (dst_y * g_renderer.pitch));
                        dst_row[dst_x] = color;
                    }
                }
            } else {
                yaat_draw_rect(&g_renderer, cx, cy, 5, 7, color);
            }
        }
        cx += YAAT_FONT_CELL_WIDTH;
    }
}


static int yaat_parse_color(const char *text, unsigned long *color)
{
    unsigned int r;
    unsigned int g;
    unsigned int b;
    char *end;
    unsigned long parsed;

    if (text == 0 || color == 0) return 0;
    while (*text == ' ' || *text == '\t') ++text;
    if (text[0] == '#') {
        if (sscanf(text + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            *color = (b & 0xff) | ((g & 0xff) << 8) | ((r & 0xff) << 16);
            return 1;
        }
        return 0;
    }
    if (sscanf(text, "%u,%u,%u", &r, &g, &b) == 3) {
        *color = (b & 0xff) | ((g & 0xff) << 8) | ((r & 0xff) << 16);
        return 1;
    }
    parsed = strtoul(text, &end, 0);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (end != text && *end == '\0') {
        *color = parsed & 0x00ffffffUL;
        return 1;
    }
    return 0;
}

static unsigned long yaat_hash_color(const char *text, unsigned long fallback)
{
    unsigned long hash;
    int i;

    if (text == 0 || text[0] == '\0') {
        return fallback;
    }

    hash = 2166136261UL;
    for (i = 0; text[i] != '\0'; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= 16777619UL;
    }

    return 0x00404040UL | (hash & 0x007f7f7fUL);
}

static unsigned short yaat_read_le16(const unsigned char *data)
{
    return (unsigned short)(data[0] | (data[1] << 8));
}

static unsigned long yaat_read_le32(const unsigned char *data)
{
    return ((unsigned long)data[0]) |
           ((unsigned long)data[1] << 8) |
           ((unsigned long)data[2] << 16) |
           ((unsigned long)data[3] << 24);
}

static long yaat_read_le32_signed(const unsigned char *data)
{
    return (long)yaat_read_le32(data);
}

#define YAAT_EMBEDDED_MODULE
#include "platform/win32/bitmap_assets.c"
#undef YAAT_EMBEDDED_MODULE

static int yaat_draw_runtime_zmask(void)
{
    YaatBitmap zmask_bitmap;
    YaatTransparency transparency;
    char path[YAAT_ASSET_MAX_PATH * 2];

    if (g_runtime_load.room.room_path[0] == '\0' ||
        g_runtime_load.room.zmask[0] == '\0') {
        return 0;
    }
    yaat_runtime_join_path(path, sizeof(path),
                           yaat_runtime_logical_path(g_runtime_load.room.room_path),
                           g_runtime_load.room.zmask);
    memset(&zmask_bitmap, 0, sizeof(zmask_bitmap));
    if (!yaat_load_bmp(&zmask_bitmap, path)) {
        return 0;
    }
    transparency.mode = YAAT_TRANSPARENCY_COLOR_KEY;
    transparency.color_key = 0x00ff00ffUL;
    transparency.mask[0] = '\0';
    yaat_draw_bitmap_transparent(&zmask_bitmap, g_shake_offset_x, g_shake_offset_y,
                                 &transparency,
                                 yaat_runtime_logical_path(g_runtime_load.room.room_path));
    yaat_unload_bitmap(&zmask_bitmap);
    return 1;
}

static int yaat_draw_runtime_background(void)
{
    char path[YAAT_ASSET_MAX_PATH * 2];
    if (g_runtime_load.room.room_path[0] == '\0' ||
        g_runtime_load.room.background[0] == '\0') {
        return 0;
    }

    yaat_runtime_join_path(path, sizeof(path),
                           yaat_runtime_logical_path(g_runtime_load.room.room_path),
                           g_runtime_load.room.background);
    if (!yaat_load_bmp(&g_background_bitmap, path)) {
        return 0;
    }

    yaat_draw_bitmap(&g_background_bitmap, g_shake_offset_x, g_shake_offset_y);

    return 1;
}

static int yaat_load_runtime_walkmask(void)
{
    char path[YAAT_ASSET_MAX_PATH * 2];

    if (!g_runtime_load.ok || g_runtime_load.room.room_path[0] == '\0' ||
        g_runtime_load.room.walkmask[0] == '\0') {
        yaat_unload_bitmap(&g_walkmask_bitmap);
        return 0;
    }

    yaat_runtime_join_path(path, sizeof(path),
                           yaat_runtime_logical_path(g_runtime_load.room.room_path),
                           g_runtime_load.room.walkmask);
    return yaat_load_bmp(&g_walkmask_bitmap, path);
}

static int yaat_is_walkable_at(int x, int y)
{
    int mask_x;
    int mask_y;
    unsigned long pixel;
    unsigned long red;
    unsigned long green;
    unsigned long blue;

    if (!g_runtime_load.ok || g_runtime_load.room.walkmask[0] == '\0') {
        return 1;
    }
    if (!yaat_load_runtime_walkmask()) {
        return 1;
    }

    if (x < 0 || y < 0 || x >= YAAT_BACKBUFFER_WIDTH || y >= YAAT_PLAYFIELD_HEIGHT) {
        return 0;
    }

    mask_x = x;
    mask_y = y;
    if (g_walkmask_bitmap.width != YAAT_BACKBUFFER_WIDTH) {
        mask_x = (x * g_walkmask_bitmap.width) / YAAT_BACKBUFFER_WIDTH;
    }
    if (g_walkmask_bitmap.height != YAAT_PLAYFIELD_HEIGHT) {
        mask_y = (y * g_walkmask_bitmap.height) / YAAT_PLAYFIELD_HEIGHT;
    }
    if (mask_x < 0 || mask_y < 0 ||
        mask_x >= g_walkmask_bitmap.width || mask_y >= g_walkmask_bitmap.height) {
        return 0;
    }

    pixel = g_walkmask_bitmap.pixels[(mask_y * g_walkmask_bitmap.width) + mask_x];
    red = (pixel >> 16) & 0xff;
    green = (pixel >> 8) & 0xff;
    blue = pixel & 0xff;
    return red + green + blue >= 128;
}

#define YAAT_EMBEDDED_MODULE
#include "runtime/navigation.c"
#undef YAAT_EMBEDDED_MODULE

static void yaat_draw_player_placeholder(void)
{
    int shadow_x;
    int shadow_y;
    int body_x;
    int body_y;

    shadow_x = g_player_x - (YAAT_PLAYER_WIDTH / 2) - 2 + g_shake_offset_x;
    shadow_y = g_player_y + 7 + g_shake_offset_y;
    body_x = g_player_x - (YAAT_PLAYER_WIDTH / 2) + g_shake_offset_x;
    body_y = g_player_y - YAAT_PLAYER_HEIGHT + g_shake_offset_y;

    yaat_draw_rect(&g_renderer, shadow_x, shadow_y, YAAT_PLAYER_WIDTH + 4, 5,
                   0x00664f38UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y, 10, 10, 0x005a3a24UL);
    yaat_draw_rect(&g_renderer, body_x + 3, body_y + 9, 12, 15,
                   0x002f5f9eUL);
    yaat_draw_rect(&g_renderer, body_x, body_y + 12, 4, 12, 0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 14, body_y + 12, 4, 12,
                   0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y + 24, 4, 10,
                   0x001f2430UL);
    yaat_draw_rect(&g_renderer, body_x + 10, body_y + 24, 4, 10,
                   0x001f2430UL);
}


static YaatRuntimeInventoryItem *yaat_find_runtime_inventory_item(const char *id)
{
    int i;

    if (id == 0) return 0;
    for (i = 0; i < g_runtime_load.inventory.item_count; ++i) {
        if (strcmp(g_runtime_load.inventory.items[i].id, id) == 0) {
            return &g_runtime_load.inventory.items[i];
        }
    }
    return 0;
}


static const char *yaat_runtime_inventory_item_display_name(const char *id)
{
    YaatRuntimeInventoryItem *item = yaat_find_runtime_inventory_item(id);
    if (item != 0 && item->name[0] != '\0') return item->name;
    return id != 0 ? id : "";
}

static const char *yaat_hover_target_display_name(void)
{
    return g_hover_target_name[0] != '\0' ? g_hover_target_name : g_hover_target_id;
}

static void yaat_set_hover_target(YaatHoverTargetKind kind, const char *id,
                                  const char *name)
{
    g_hover_target_kind = kind;
    yaat_copy(g_hover_target_id, sizeof(g_hover_target_id), id != 0 ? id : "",
              strlen(id != 0 ? id : ""));
    yaat_copy(g_hover_target_name, sizeof(g_hover_target_name),
              (name != 0 && name[0] != '\0') ? name : (id != 0 ? id : ""),
              strlen((name != 0 && name[0] != '\0') ? name : (id != 0 ? id : "")));
    yaat_update_command_feedback();
}

static void yaat_format_command_feedback(const char *verb, const char *object,
                                         const char *target)
{
    char display_verb[32];
    yaat_copy(display_verb, sizeof(display_verb), verb != 0 ? verb : "walk",
              strlen(verb != 0 ? verb : "walk"));
    if (display_verb[0] != '\0') {
        display_verb[0] = (char)toupper((unsigned char)display_verb[0]);
    }
    if (strcmp(display_verb, "Look") == 0) {
        yaat_copy(display_verb, sizeof(display_verb), "Look at", strlen("Look at"));
    }
    if (object != 0 && object[0] != '\0' && target != 0 && target[0] != '\0') {
        sprintf(g_command_feedback, "%s %s with %s",
                display_verb, object, target);
    } else if (target != 0 && target[0] != '\0') {
        sprintf(g_command_feedback, "%s %s",
                display_verb, target);
    } else if (object != 0 && object[0] != '\0') {
        sprintf(g_command_feedback, "%s %s",
                display_verb, object);
    } else {
        sprintf(g_command_feedback, "%s", display_verb);
    }
    g_command_feedback[sizeof(g_command_feedback) - 1] = '\0';
}

static void yaat_set_look_command_feedback(const char *target)
{
    yaat_format_command_feedback("look", "", target != 0 ? target : "");
    yaat_copy(g_command_feedback_override, sizeof(g_command_feedback_override),
              g_command_feedback, strlen(g_command_feedback));
    g_command_feedback_override_remaining_ms = 1000;
}

static void yaat_update_command_feedback(void)
{
    const char *verb = yaat_active_verb();
    const char *hover_name = yaat_hover_target_display_name();
    const char *target = g_hover_target_kind != YAAT_HOVER_EMPTY ? hover_name : "";

    if (g_command_feedback_override[0] != '\0') {
        yaat_copy(g_command_feedback, sizeof(g_command_feedback),
                  g_command_feedback_override, strlen(g_command_feedback_override));
        return;
    }
    g_command_feedback_override[0] = '\0';
    g_command_feedback_override_remaining_ms = 0;

    if (strcmp(verb, "use") == 0 && g_selected_inventory[0] != '\0') {
        yaat_format_command_feedback(verb,
                                     yaat_runtime_inventory_item_display_name(g_selected_inventory),
                                     target);
    } else {
        yaat_format_command_feedback(verb, "", target);
    }
}

static const char *yaat_inventory_item_icon_path(YaatRuntimeInventoryItem *item,
                                                 unsigned long elapsed_ms)
{
    int timings[YAAT_ASSET_MAX_ANIMATION_FRAMES];
    int i;
    int frame;

    if (item == 0) return "";
    if (item->frame_count > 0) {
        for (i = 0; i < item->frame_count; ++i) {
            timings[i] = item->timing_ms[i] > 0 ? item->timing_ms[i] : 250;
        }
        frame = yaat_runtime_animation_frame_index(timings, item->frame_count,
                                                   elapsed_ms);
        return item->frames[frame];
    }
    return item->icon;
}

static void yaat_draw_inventory_bar(void)
{
    int i;
    int slot_x;
    int slot_y;
    unsigned long elapsed_ms;

    yaat_draw_rect(&g_renderer, 0, YAAT_PLAYFIELD_HEIGHT,
                   YAAT_BACKBUFFER_WIDTH, YAAT_BACKBUFFER_HEIGHT - YAAT_PLAYFIELD_HEIGHT,
                   0x00101018UL);
    elapsed_ms = GetTickCount();
    for (i = 0; i < g_inventory_count; ++i) {
        YaatRuntimeInventoryItem *item;
        YaatBitmap icon_bitmap;
        const char *icon_path;
        char logical_path[YAAT_ASSET_MAX_PATH * 2];

        slot_x = 8 + (i * 38);
        slot_y = YAAT_PLAYFIELD_HEIGHT + 4;
        yaat_draw_rect(&g_renderer, slot_x, slot_y, 34, 32, 0x00404048UL);
        yaat_draw_rect(&g_renderer, slot_x + 1, slot_y + 1, 32, 30, 0x00202028UL);

        item = yaat_find_runtime_inventory_item(g_inventory[i]);
        icon_path = yaat_inventory_item_icon_path(item, elapsed_ms);
        memset(&icon_bitmap, 0, sizeof(icon_bitmap));
        if (icon_path[0] != '\0') {
            yaat_runtime_join_path(logical_path, sizeof(logical_path),
                                   "inventory", icon_path);
            if (yaat_load_bmp(&icon_bitmap, logical_path)) {
                yaat_draw_bitmap(&icon_bitmap,
                                 slot_x + 17 - (icon_bitmap.width / 2),
                                 slot_y + 16 - (icon_bitmap.height / 2));
                yaat_unload_bitmap(&icon_bitmap);
                continue;
            }
        }
        yaat_draw_rect(&g_renderer, slot_x + 8, slot_y + 8, 18, 16,
                       yaat_hash_color(g_inventory[i], 0x00a07030UL));
    }
}

static void yaat_load_player_sprite_metadata(void)
{
    unsigned char *buffer;
    size_t buffer_size;
    char *line;

    g_player_transparent_color_enabled = 0;
    g_player_transparent_color = 0;
    if (!yaat_asset_read_all(&g_asset_store, "graphics/sprites/player.ini",
                             &buffer, &buffer_size)) {
        return;
    }

    for (line = strtok((char *)buffer, "\n"); line != 0; line = strtok(0, "\n")) {
        char *text;
        char *equals;

        text = yaat_trim_text(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#' ||
            text[0] == '[') {
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) {
            continue;
        }
        *equals = '\0';
        text = yaat_trim_text(text);
        if (strcmp(text, "transparent_color") == 0 ||
            strcmp(text, "color_key") == 0) {
            g_player_transparent_color_enabled =
                yaat_parse_color(yaat_trim_text(equals + 1),
                                 &g_player_transparent_color);
        }
    }
    free(buffer);
}

static YaatAnimationClip *yaat_player_animation(const char *id)
{
    int i;

    for (i = 0; i < g_runtime_load.player.animation_count; ++i) {
        if (strcmp(g_runtime_load.player.animations[i].id, id) == 0) {
            return &g_runtime_load.player.animations[i];
        }
    }
    return 0;
}

static void yaat_set_player_animation(const char *id)
{
    if (id == 0 || strcmp(g_player_animation_id, id) == 0) {
        return;
    }
    yaat_copy(g_player_animation_id, sizeof(g_player_animation_id), id,
              strlen(id));
    g_player_animation_frame = 0;
    g_player_animation_elapsed_ms = 0;
}


static const char *yaat_player_walk_animation_for_delta(int dx, int dy)
{
    int abs_dx = dx < 0 ? -dx : dx;
    int abs_dy = dy < 0 ? -dy : dy;

    if (abs_dy > abs_dx) {
        g_player_facing_vertical = dy < 0 ? -1 : 1;
        return dy < 0 ? "walk_up" : "walk_down";
    }
    if (dx < 0) {
        g_player_facing_right = 0;
        g_player_facing_vertical = 0;
        return "walk_left";
    }
    if (dx > 0) {
        g_player_facing_right = 1;
        g_player_facing_vertical = 0;
        return "walk_right";
    }
    if (dy < 0) {
        g_player_facing_vertical = -1;
        return "walk_up";
    }
    if (dy > 0) {
        g_player_facing_vertical = 1;
        return "walk_down";
    }
    return g_player_facing_vertical < 0 ? "walk_up" :
           (g_player_facing_vertical > 0 ? "walk_down" :
            (g_player_facing_right ? "walk_right" : "walk_left"));
}

static const char *yaat_player_idle_animation(void)
{
    if (g_player_facing_vertical < 0) return "idle_up";
    if (g_player_facing_vertical > 0) return "idle_down";
    return g_player_facing_right ? "idle_right" : "idle_left";
}

static void yaat_player_face_direction(const char *direction)
{
    if (direction == 0 || direction[0] == '\0') return;
    if (strcmp(direction, "left") == 0 || strcmp(direction, "west") == 0) {
        g_player_facing_right = 0;
        g_player_facing_vertical = 0;
    } else if (strcmp(direction, "right") == 0 || strcmp(direction, "east") == 0) {
        g_player_facing_right = 1;
        g_player_facing_vertical = 0;
    } else if (strcmp(direction, "up") == 0 || strcmp(direction, "north") == 0 ||
               strcmp(direction, "away") == 0) {
        g_player_facing_vertical = -1;
    } else if (strcmp(direction, "down") == 0 || strcmp(direction, "south") == 0 ||
               strcmp(direction, "toward") == 0) {
        g_player_facing_vertical = 1;
    }
}

static int yaat_player_current_step_pixels(void)
{
    YaatAnimationClip *clip;
    YaatAnimationFrame *frame;

    clip = yaat_player_animation(g_player_animation_id);
    if (clip == 0 || clip->frame_count <= 0) return YAAT_PLAYER_SPEED_PIXELS;
    if (g_player_animation_frame >= clip->frame_count) g_player_animation_frame = 0;
    frame = &clip->frames[g_player_animation_frame];
    return frame->step_pixels > 0 ? frame->step_pixels : YAAT_PLAYER_SPEED_PIXELS;
}

static void yaat_draw_player(void)
{
    const char *animation_id;
    YaatAnimationClip *clip;
    YaatAnimationFrame *frame;
    YaatTransparency transparency;
    int draw_x;
    int draw_y;
    int frame_width;
    int frame_height;
    int scaled_width;
    int scaled_height;
    double scale;

    if (g_player_x != g_target_x || g_player_y != g_target_y) {
        animation_id = yaat_player_walk_animation_for_delta(g_target_x - g_player_x,
                                                            g_target_y - g_player_y);
    } else {
        animation_id = yaat_player_idle_animation();
    }
    yaat_set_player_animation(animation_id);
    clip = yaat_player_animation(g_player_animation_id);
    if (clip == 0 || clip->frame_count <= 0) {
        yaat_draw_player_placeholder();
        return;
    }
    if (g_player_animation_frame >= clip->frame_count) {
        g_player_animation_frame = 0;
    }

    frame = &clip->frames[g_player_animation_frame];
    memset(&g_player_bitmap, 0, sizeof(g_player_bitmap));
    if (!yaat_load_bmp(&g_player_bitmap, frame->path)) {
        yaat_draw_player_placeholder();
        return;
    }

    frame_width = frame->width > 0 ? frame->width : g_player_bitmap.width;
    frame_height = frame->height > 0 ? frame->height : g_player_bitmap.height;
    scale = yaat_room_scale_for_y(&g_runtime_load.room, g_player_y);
    if (scale < 0.05) scale = 0.05;
    scaled_width = (int)((double)frame_width * scale + 0.5);
    scaled_height = (int)((double)frame_height * scale + 0.5);
    if (scaled_width < 1) scaled_width = 1;
    if (scaled_height < 1) scaled_height = 1;
    draw_x = g_player_x - (scaled_width / 2) + g_shake_offset_x;
    draw_y = g_player_y - scaled_height + g_shake_offset_y;

    transparency.mode = g_player_transparent_color_enabled ? YAAT_TRANSPARENCY_COLOR_KEY : YAAT_TRANSPARENCY_ALPHA;
    transparency.color_key = g_player_transparent_color_enabled ? g_player_transparent_color : 0x00ff00ffUL;
    transparency.mask[0] = '\0';
    yaat_draw_bitmap_transparent_scaled(&g_player_bitmap, draw_x, draw_y,
                                        scaled_width, scaled_height,
                                        &transparency, "graphics/sprites");
    (void)frame;
    yaat_unload_bitmap(&g_player_bitmap);
}

static const char *yaat_runtime_object_sprite_for_time(YaatRuntimeObject *object)
{
    unsigned long frame_duration;
    unsigned long frame_index;

    if (object == 0) {
        return "";
    }
    if (object->animation_frame_count <= 0 || object->animation_fps <= 0) {
        return object->sprite;
    }
    frame_duration = 1000UL / (unsigned long)object->animation_fps;
    if (frame_duration == 0) {
        frame_duration = 1;
    }
    frame_index = (g_animation_clock_ms / frame_duration) %
                  (unsigned long)object->animation_frame_count;
    return object->animation_frames[frame_index];
}

typedef struct YaatRuntimeDrawLayer {
    int is_player;
    int depth_y;
    int object_index;
} YaatRuntimeDrawLayer;

static void yaat_draw_runtime_object(YaatRuntimeObject *object)
{
    unsigned long object_color;
    YaatBitmap object_bitmap;
    char object_path[YAAT_ASSET_MAX_PATH * 2];
    const char *object_sprite;

    if (object == 0 || !object->visible || object->width <= 0 || object->height <= 0) {
        return;
    }
    object_sprite = yaat_runtime_object_sprite_for_time(object);
    memset(&object_bitmap, 0, sizeof(object_bitmap));
    yaat_runtime_join_path(object_path, sizeof(object_path),
                           yaat_runtime_logical_path(g_runtime_load.room.room_path),
                           object_sprite);
    if (yaat_load_bmp(&object_bitmap, object_path)) {
        yaat_draw_bitmap_transparent(&object_bitmap,
                                     object->x + g_shake_offset_x,
                                     object->y + g_shake_offset_y,
                                     &object->transparency,
                                     yaat_runtime_logical_path(g_runtime_load.room.room_path));
        if (object->transparent_color_enabled) {
            yaat_draw_bitmap_keyed(&object_bitmap,
                                   object->x + g_shake_offset_x,
                                   object->y + g_shake_offset_y,
                                   object->transparent_color_enabled,
                                   object->transparent_color);
        }
        yaat_unload_bitmap(&object_bitmap);
        return;
    }
    object_color = yaat_hash_color(object_sprite, 0x002f5f9eUL);
    yaat_draw_rect(&g_renderer, object->x + g_shake_offset_x,
                   object->y + g_shake_offset_y,
                   object->width, object->height, 0x00202020UL);
    yaat_draw_rect(&g_renderer, object->x + 1 + g_shake_offset_x,
                   object->y + 1 + g_shake_offset_y,
                   object->width - 2, object->height - 2, object_color);
}

static void yaat_sort_runtime_layers(YaatRuntimeDrawLayer *layers, int count)
{
    int i;
    for (i = 1; i < count; ++i) {
        YaatRuntimeDrawLayer layer = layers[i];
        int j = i - 1;
        while (j >= 0 && layers[j].depth_y > layer.depth_y) {
            layers[j + 1] = layers[j];
            --j;
        }
        layers[j + 1] = layer;
    }
}

static void yaat_draw_runtime_room(void)
{
    int i;
    int layer_count;
    int floor_y;
    int background_drawn;
    unsigned long background_color;
    int dialogue_x;
    int dialogue_y;
    YaatRuntimeDrawLayer layers[YAAT_ASSET_MAX_OBJECTS + 1];

    background_color = yaat_hash_color(g_runtime_load.room.background,
                                       0x00d8c7a3UL);
    yaat_gdi_renderer_clear(&g_renderer, background_color);
    background_drawn = yaat_draw_runtime_background();

    if (!background_drawn) {
        floor_y = YAAT_BACKBUFFER_HEIGHT - 44;
        if (g_runtime_load.room.height > 0) {
            floor_y = (YAAT_BACKBUFFER_HEIGHT * 3) / 4;
        }
        yaat_draw_rect(&g_renderer, g_shake_offset_x, floor_y + g_shake_offset_y,
                       YAAT_BACKBUFFER_WIDTH,
                       YAAT_BACKBUFFER_HEIGHT - floor_y, 0x005f6f4aUL);

        yaat_draw_rect(&g_renderer, 12 + g_shake_offset_x,
                       12 + g_shake_offset_y, 128, 22, 0x00282828UL);
        yaat_draw_rect(&g_renderer, 14 + g_shake_offset_x,
                       14 + g_shake_offset_y, 124, 18, 0x00d8d0b8UL);
    }

    for (i = 0; i < g_runtime_load.room.hotspot_count; ++i) {
        YaatRuntimeHotspot *hotspot;
        unsigned long hotspot_color;

        hotspot = &g_runtime_load.room.hotspots[i];
        if (hotspot->width <= 0 || hotspot->height <= 0) {
            continue;
        }
        hotspot_color = yaat_hash_color(hotspot->cursor, 0x00c08020UL);
        yaat_draw_rect(&g_renderer, hotspot->x + g_shake_offset_x,
                       hotspot->y + g_shake_offset_y,
                       hotspot->width, hotspot->height, 0x00f0d020UL);
        yaat_draw_rect(&g_renderer, hotspot->x + 1 + g_shake_offset_x,
                       hotspot->y + 1 + g_shake_offset_y,
                       hotspot->width - 2, hotspot->height - 2,
                       hotspot_color);
    }

    layer_count = 0;
    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
        if (!object->visible || object->width <= 0 || object->height <= 0) {
            continue;
        }
        layers[layer_count].is_player = 0;
        layers[layer_count].object_index = i;
        layers[layer_count].depth_y = object->y + object->height;
        ++layer_count;
    }
    if (g_player_visible) {
        layers[layer_count].is_player = 1;
        layers[layer_count].object_index = -1;
        layers[layer_count].depth_y = g_player_y;
        ++layer_count;
    }
    yaat_sort_runtime_layers(layers, layer_count);
    for (i = 0; i < layer_count; ++i) {
        if (layers[i].is_player) {
            yaat_draw_player();
        } else {
            yaat_draw_runtime_object(&g_runtime_load.room.objects[layers[i].object_index]);
        }
    }

    yaat_draw_runtime_zmask();
    if (yaat_dialogue_position_for_speaker(&dialogue_x, &dialogue_y)) {
        yaat_draw_text_block(dialogue_x, dialogue_y, g_dialogue_text, 0x00ffffffUL);
    }
    yaat_draw_inventory_bar();
}

static void yaat_draw_cursor_placeholder(void)
{
    unsigned long outline_color;
    unsigned long fill_color;
    YaatBitmap cursor_bitmap;
    YaatTransparency transparency;
    char cursor_path[YAAT_ASSET_MAX_PATH * 2];

    memset(&cursor_bitmap, 0, sizeof(cursor_bitmap));
    yaat_runtime_join_path(cursor_path, sizeof(cursor_path), "graphics/cursors",
                           g_cursor_state);
    strncat(cursor_path, ".bmp", sizeof(cursor_path) - 1 - strlen(cursor_path));
    if (yaat_load_bmp(&cursor_bitmap, cursor_path)) {
        transparency.mode = YAAT_TRANSPARENCY_ALPHA;
        transparency.color_key = 0x00ff00ffUL;
        transparency.mask[0] = '\0';
        yaat_draw_bitmap_transparent(&cursor_bitmap, g_cursor_x, g_cursor_y,
                                     &transparency, "graphics/cursors");
        yaat_unload_bitmap(&cursor_bitmap);
        return;
    }

    outline_color = 0x00000000UL;
    fill_color = strcmp(g_cursor_state, "use") == 0 ? 0x00ffe070UL : 0x00ffffffUL;

    yaat_draw_rect(&g_renderer, g_cursor_x, g_cursor_y, 2, 14, outline_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 2, g_cursor_y + 2, 2, 10, outline_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 4, g_cursor_y + 4, 2, 8, outline_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 6, g_cursor_y + 6, 2, 6, outline_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 8, g_cursor_y + 8, 2, 4, outline_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 1, g_cursor_y + 1, 1, 11, fill_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 2, g_cursor_y + 3, 2, 7, fill_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 4, g_cursor_y + 5, 2, 5, fill_color);
    yaat_draw_rect(&g_renderer, g_cursor_x + 6, g_cursor_y + 7, 2, 3, fill_color);

    if (strcmp(g_cursor_state, "use") == 0) {
        yaat_draw_rect(&g_renderer, g_cursor_x + 9, g_cursor_y + 9, 5, 2,
                       outline_color);
        yaat_draw_rect(&g_renderer, g_cursor_x + 10, g_cursor_y + 10, 3, 1,
                       fill_color);
    }
}

static char *yaat_trim_text(char *text)
{
    char *end;

    while (*text != '\0' && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) {
        --end;
    }
    *end = '\0';
    return text;
}


static void yaat_add_verb(const char *verb)
{
    if (verb == 0 || verb[0] == '\0' || g_verb_count >= YAAT_MAX_VERBS) return;
    yaat_copy(g_verbs[g_verb_count], sizeof(g_verbs[g_verb_count]), verb, strlen(verb));
    ++g_verb_count;
}

static void yaat_load_default_verbs(void)
{
    g_verb_count = 0;
    yaat_add_verb("look");
    yaat_add_verb("use");
    yaat_add_verb("read");
    yaat_add_verb("talk");
    yaat_add_verb("take");
    yaat_add_verb("open");
    yaat_add_verb("close");
}

static void yaat_load_verbs(void)
{
    unsigned char *buffer;
    size_t buffer_size;
    char *line;

    yaat_load_default_verbs();
    if (!yaat_asset_read_all(&g_asset_store, "actions.ini", &buffer, &buffer_size)) return;

    g_verb_count = 0;
    for (line = strtok((char *)buffer, "\n"); line != 0;
         line = strtok(0, "\n")) {
        char *text = yaat_trim_text(line);
        char *equals;
        char *verb;
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#' || text[0] == '[') continue;
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        verb = yaat_trim_text(equals + 1);
        if (strncmp(yaat_trim_text(text), "verb", 4) == 0) yaat_add_verb(verb);
    }
    if (g_verb_count == 0) yaat_load_default_verbs();
    free(buffer);
}

static int yaat_verb_button_at(int x, int y)
{
    int i;
    if (y < YAAT_PLAYFIELD_HEIGHT) return -1;
    for (i = 0; i < g_verb_count; ++i) {
        int bx = 4 + (i % 3) * YAAT_VERB_BUTTON_WIDTH;
        int by = YAAT_PLAYFIELD_HEIGHT + 3 + (i / 3) * (YAAT_VERB_BUTTON_HEIGHT + 2);
        if (x >= bx && y >= by && x < bx + YAAT_VERB_BUTTON_WIDTH &&
            y < by + YAAT_VERB_BUTTON_HEIGHT) return i;
    }
    return -1;
}

static int yaat_inventory_slot_at(int x, int y)
{
    int i;
    int start_x = 8;
    int start_y = YAAT_PLAYFIELD_HEIGHT + 4;
    if (y < start_y || y >= start_y + YAAT_INVENTORY_SLOT_SIZE) return -1;
    for (i = 0; i < g_inventory_count; ++i) {
        int sx = start_x + (i * (YAAT_INVENTORY_SLOT_SIZE + 3));
        if (x >= sx && x < sx + YAAT_INVENTORY_SLOT_SIZE) return i;
    }
    return -1;
}

static void yaat_load_runtime_hotspots(void)
{
    char path[YAAT_ASSET_MAX_PATH];
    unsigned char *buffer;
    size_t buffer_size;
    char *line;
    YaatRuntimeHotspot *hotspot;

    g_runtime_hotspot_count = 0;
    if (!g_runtime_load.ok || g_runtime_load.room.room_path[0] == '\0') return;

    yaat_copy(path, sizeof(path),
              yaat_runtime_logical_path(g_runtime_load.room.room_path),
              strlen(yaat_runtime_logical_path(g_runtime_load.room.room_path)));
    if (strlen(path) + strlen("/hotspots.ini") >= sizeof(path)) return;
    strcat(path, "/hotspots.ini");

    if (!yaat_asset_read_all(&g_asset_store, path, &buffer, &buffer_size)) return;

    hotspot = 0;
    for (line = strtok((char *)buffer, "\n"); line != 0;
         line = strtok(0, "\n")) {
        char *text;
        char *equals;

        text = yaat_trim_text(line);
        if (text[0] == '\0' || text[0] == ';' || text[0] == '#') continue;
        if (text[0] == '[') {
            char *close = strchr(text, ']');
            if (close != 0 && g_runtime_hotspot_count < YAAT_MAX_RUNTIME_HOTSPOTS) {
                *close = '\0';
                hotspot = &g_runtime_hotspots[g_runtime_hotspot_count++];
                memset(hotspot, 0, sizeof(*hotspot));
                yaat_copy(hotspot->id, sizeof(hotspot->id), text + 1,
                          strlen(text + 1));
                yaat_copy(hotspot->cursor, sizeof(hotspot->cursor), "arrow", 5);
            }
            continue;
        }
        if (hotspot == 0) continue;
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        text = yaat_trim_text(text);
        ++equals;
        equals = yaat_trim_text(equals);
        if (strcmp(text, "rect") == 0) {
            sscanf(equals, "%d,%d,%d,%d", &hotspot->x, &hotspot->y,
                   &hotspot->width, &hotspot->height);
        } else if (strcmp(text, "cursor") == 0) {
            yaat_copy(hotspot->cursor, sizeof(hotspot->cursor), equals,
                      strlen(equals));
        }
    }
    free(buffer);
}

static YaatRuntimeHotspot *yaat_runtime_hotspot_at(int x, int y)
{
    int i;

    if (!g_runtime_load.ok) return 0;
    for (i = g_runtime_load.room.hotspot_count - 1; i >= 0; --i) {
        YaatRuntimeHotspot *hotspot = &g_runtime_load.room.hotspots[i];
        if (hotspot->width > 0 && hotspot->height > 0 && x >= hotspot->x &&
            y >= hotspot->y && x < hotspot->x + hotspot->width &&
            y < hotspot->y + hotspot->height) {
            return hotspot;
        }
    }
    return 0;
}


static YaatRuntimeObject *yaat_runtime_object_at(int x, int y)
{
    int i;

    if (!g_runtime_load.ok) return 0;
    for (i = g_runtime_load.room.object_count - 1; i >= 0; --i) {
        YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
        if (object->visible && object->width > 0 && object->height > 0 &&
            x >= object->x && y >= object->y &&
            x < object->x + object->width && y < object->y + object->height) {
            return object;
        }
    }
    return 0;
}

static YaatRuntimeHotspot *yaat_runtime_hotspot_by_id(const char *id)
{
    int i;

    if (!g_runtime_load.ok || id == 0 || id[0] == '\0') return 0;
    for (i = 0; i < g_runtime_load.room.hotspot_count; ++i) {
        if (strcmp(g_runtime_load.room.hotspots[i].id, id) == 0) {
            return &g_runtime_load.room.hotspots[i];
        }
    }
    return 0;
}

static void yaat_draw_error_scene(void)
{
    yaat_gdi_renderer_clear(&g_renderer, 0x00202030UL);
    yaat_draw_rect(&g_renderer, 24, 28, YAAT_BACKBUFFER_WIDTH - 48, 48,
                   0x00802020UL);
    yaat_draw_rect(&g_renderer, 28, 32, YAAT_BACKBUFFER_WIDTH - 56, 40,
                   0x00e0d0c0UL);
    yaat_draw_rect(&g_renderer, 40, 92, YAAT_BACKBUFFER_WIDTH - 80, 16,
                   0x00802020UL);
    yaat_draw_rect(&g_renderer, 40, 116, YAAT_BACKBUFFER_WIDTH - 120, 16,
                   0x00802020UL);
}


static int yaat_find_var(const char *name)
{
    int i;
    for (i = 0; i < g_var_count; ++i) if (strcmp(g_vars[i].name, name) == 0) return i;
    return -1;
}

static YaatValue yaat_get_var(const char *name)
{
    int idx = yaat_find_var(name);
    if (idx < 0) return yaat_value_bool(0);
    return g_vars[idx].value;
}

static int yaat_value_truthy(const YaatValue *value)
{
    if (!value) return 0;
    if (value->kind == YAAT_VALUE_BOOL) return value->bool_value != 0;
    if (value->kind == YAAT_VALUE_INT) return value->int_value != 0;
    return value->string_value[0] != '\0';
}

static int yaat_compare_values(const YaatValue *left, YaatConditionOp op, const YaatValue *right)
{
    int cmp = 0;
    if (op == YAAT_COND_TRUTHY) return yaat_value_truthy(left);
    if (!left || !right) return 0;
    if (left->kind == YAAT_VALUE_INT && right->kind == YAAT_VALUE_INT) {
        if (left->int_value < right->int_value) cmp = -1;
        else if (left->int_value > right->int_value) cmp = 1;
    } else if (left->kind == YAAT_VALUE_BOOL && right->kind == YAAT_VALUE_BOOL) {
        if (left->bool_value < right->bool_value) cmp = -1;
        else if (left->bool_value > right->bool_value) cmp = 1;
    } else {
        cmp = strcmp(left->string_value, right->string_value);
    }
    if (op == YAAT_COND_EQ) return cmp == 0;
    if (op == YAAT_COND_NE) return cmp != 0;
    if (op == YAAT_COND_LT) return cmp < 0;
    if (op == YAAT_COND_LTE) return cmp <= 0;
    if (op == YAAT_COND_GT) return cmp > 0;
    if (op == YAAT_COND_GTE) return cmp >= 0;
    return 0;
}

static void yaat_set_var_value(const char *name, const YaatValue *value)
{
    int idx = yaat_find_var(name);
    if (idx < 0 && g_var_count < YAAT_MAX_VARS) {
        idx = g_var_count++;
        yaat_copy(g_vars[idx].name, sizeof(g_vars[idx].name), name, strlen(name));
    }
    if (idx >= 0 && value) g_vars[idx].value = *value;
}

static void yaat_set_var(const char *name, int value)
{
    YaatValue var_value;

    var_value = yaat_value_bool(value);
    yaat_set_var_value(name, &var_value);
}

static int yaat_has_inventory(const char *item)
{
    int i;
    for (i = 0; i < g_inventory_count; ++i) if (strcmp(g_inventory[i], item) == 0) return 1;
    return 0;
}

static void yaat_take_inventory(const char *item)
{
    if (!yaat_has_inventory(item) && g_inventory_count < YAAT_MAX_INVENTORY) {
        yaat_copy(g_inventory[g_inventory_count++], 32, item, strlen(item));
    }
}

static void yaat_remove_inventory(const char *item)
{
    int i;
    int found;

    found = -1;
    for (i = 0; i < g_inventory_count; ++i) {
        if (strcmp(g_inventory[i], item) == 0) {
            int j;
            for (j = i; j + 1 < g_inventory_count; ++j) {
                memmove(g_inventory[j], g_inventory[j + 1], sizeof(g_inventory[j]));
            }
            if (g_inventory_count > 0) --g_inventory_count;
            g_inventory[g_inventory_count][0] = '\0';
            if (strcmp(g_selected_inventory, item) == 0) g_selected_inventory[0] = '\0';
            return;
        }
    }
    if (item == 0 || item[0] == '\0') return;
    for (i = 0; i < g_inventory_count; ++i) {
        if (strcmp(g_inventory[i], item) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return;
    for (i = found; i + 1 < g_inventory_count; ++i) {
        yaat_copy(g_inventory[i], 32, g_inventory[i + 1], strlen(g_inventory[i + 1]));
    }
    if (g_inventory_count > 0) {
        g_inventory_count--;
        g_inventory[g_inventory_count][0] = '\0';
    }
    if (strcmp(g_selected_inventory, item) == 0) g_selected_inventory[0] = '\0';
}

static int yaat_eval_condition(const YaatCommand *cmd)
{
    YaatValue value;

    if ((strcmp(cmd->a, "has") == 0 || strcmp(cmd->a, "inventory") == 0) && cmd->b[0] != '\0') {
        return yaat_has_inventory(cmd->b);
    }
    value = yaat_get_var(cmd->a);
    return yaat_value_truthy(&value);
}

static int yaat_runtime_hotspot_change_room_enabled(const YaatRuntimeHotspot *hotspot)
{
    YaatValue value;

    if (hotspot == 0 || strcmp(hotspot->action, "change_room") != 0 ||
        hotspot->target_room[0] == '\0') {
        return 0;
    }
    if (!hotspot->has_required_flag || hotspot->required_flag[0] == '\0') {
        return 1;
    }
    value = yaat_get_var(hotspot->required_flag);
    return yaat_value_truthy(&value) == hotspot->required_flag_value;
}

static void yaat_pending_room_change_clear(void)
{
    g_pending_room_change = 0;
    g_pending_room_change_hotspot_id[0] = '\0';
}

static int yaat_runtime_hotspot_contains_player(const YaatRuntimeHotspot *hotspot)
{
    return hotspot != 0 && hotspot->width > 0 && hotspot->height > 0 &&
           g_player_x >= hotspot->x && g_player_y >= hotspot->y &&
           g_player_x < hotspot->x + hotspot->width &&
           g_player_y < hotspot->y + hotspot->height;
}

static void yaat_suppressed_room_change_update(void)
{
    YaatRuntimeHotspot *hotspot;

    if (g_suppressed_room_change_hotspot_id[0] == '\0') return;
    hotspot = yaat_runtime_hotspot_by_id(g_suppressed_room_change_hotspot_id);
    if (!yaat_runtime_hotspot_contains_player(hotspot)) {
        g_suppressed_room_change_hotspot_id[0] = '\0';
    }
}

static int yaat_room_change_suppressed(const YaatRuntimeHotspot *hotspot)
{
    yaat_suppressed_room_change_update();
    return hotspot != 0 && g_suppressed_room_change_hotspot_id[0] != '\0' &&
           strcmp(g_suppressed_room_change_hotspot_id, hotspot->id) == 0;
}

static void yaat_suppress_room_change_until_player_leaves(const YaatRuntimeHotspot *hotspot)
{
    if (!yaat_runtime_hotspot_contains_player(hotspot)) return;
    yaat_copy(g_suppressed_room_change_hotspot_id,
              sizeof(g_suppressed_room_change_hotspot_id), hotspot->id,
              strlen(hotspot->id));
}

static void yaat_pending_interaction_clear(void)
{
    g_pending_interaction = 0;
    g_pending_interaction_x = 0;
    g_pending_interaction_y = 0;
}

static void yaat_pending_room_change_set(const YaatRuntimeHotspot *hotspot)
{
    if (hotspot == 0) {
        yaat_pending_room_change_clear();
        return;
    }
    g_pending_room_change = 1;
    yaat_copy(g_pending_room_change_hotspot_id,
              sizeof(g_pending_room_change_hotspot_id), hotspot->id,
              strlen(hotspot->id));
}

static void yaat_pending_interaction_set(int x, int y)
{
    g_pending_interaction = 1;
    g_pending_interaction_x = x;
    g_pending_interaction_y = y;
}

#define YAAT_EMBEDDED_MODULE
#include "runtime/save_state.c"
#undef YAAT_EMBEDDED_MODULE

static YaatEntity *yaat_entity_by_id(YaatRoom *room, const char *id)
{
    int i;
    for (i = 0; room && i < room->entity_count; ++i) if (strcmp(room->entities[i].id, id) == 0) return &room->entities[i];
    return 0;
}

static YaatEntity *yaat_entity_by_id_any_room(const char *id)
{
    int i;
    for (i = 0; i < g_room_count; ++i) {
        YaatEntity *entity = yaat_entity_by_id(&g_rooms[i], id);
        if (entity != 0) return entity;
    }
    return 0;
}

static YaatRuntimeObject *yaat_runtime_object_by_id(const char *id)
{
    int i;
    if (!g_runtime_load.ok || id == 0) return 0;
    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object;
        object = &g_runtime_load.room.objects[i];
        if (strcmp(object->id, id) == 0) return object;
    }
    return 0;
}

static YaatEvent *yaat_find_event(YaatEvent *events, int count, const char *name, const char *item);
static void yaat_execute_event(YaatEvent *event);
static int yaat_client_to_backbuffer(HWND window, int client_x, int client_y,
                                     int *backbuffer_x, int *backbuffer_y);

#define YAAT_EMBEDDED_MODULE
#include "runtime/dialogue_runtime.c"
#undef YAAT_EMBEDDED_MODULE

static YaatRuntimeObjectMutation *yaat_runtime_object_mutation(const char *room_id,
                                                              const char *object_id,
                                                              int create)
{
    int i;
    YaatRuntimeObjectMutation *mutation;
    if (room_id == 0 || object_id == 0 || room_id[0] == '\0' || object_id[0] == '\0') return 0;
    for (i = 0; i < g_runtime_object_mutation_count; ++i) {
        mutation = &g_runtime_object_mutations[i];
        if (strcmp(mutation->room_id, room_id) == 0 && strcmp(mutation->object_id, object_id) == 0) {
            return mutation;
        }
    }
    if (!create || g_runtime_object_mutation_count >= YAAT_MAX_RUNTIME_OBJECT_MUTATIONS) return 0;
    mutation = &g_runtime_object_mutations[g_runtime_object_mutation_count++];
    memset(mutation, 0, sizeof(*mutation));
    yaat_copy(mutation->room_id, sizeof(mutation->room_id), room_id, strlen(room_id));
    yaat_copy(mutation->object_id, sizeof(mutation->object_id), object_id, strlen(object_id));
    return mutation;
}




static void yaat_apply_runtime_object_mutations(void)
{
    int i;
    YaatRuntimeObject *object;
    YaatRuntimeObjectMutation *mutation;
    if (!g_runtime_load.ok || g_runtime_load.room.id[0] == '\0') return;
    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        object = &g_runtime_load.room.objects[i];
        mutation = yaat_runtime_object_mutation(g_runtime_load.room.id, object->id, 0);
        if (mutation == 0) continue;
        if (mutation->has_visible) object->visible = mutation->visible;
        if (mutation->has_position) {
            object->x = mutation->x;
            object->y = mutation->y;
        }
        if (mutation->has_sprite) {
            yaat_copy(object->sprite, sizeof(object->sprite), mutation->sprite, strlen(mutation->sprite));
            object->animation[0] = '\0';
            object->animation_frame_count = 0;
        }
        if (mutation->has_animation) {
            yaat_copy(object->animation, sizeof(object->animation), mutation->animation, strlen(mutation->animation));
        }
    }
}

static void yaat_set_object_visible(const char *id, int visible)
{
    YaatEntity *entity = yaat_entity_by_id_any_room(id);
    YaatRuntimeObject *object = yaat_runtime_object_by_id(id);
    YaatRuntimeObjectMutation *mutation;
    if (entity != 0) entity->visible = visible;
    if (object != 0) {
        mutation = yaat_runtime_object_mutation(g_runtime_load.room.id, id, 1);
        object->visible = visible;
        if (mutation != 0) {
            mutation->has_visible = 1;
            mutation->visible = visible;
        }
    }
}

static void yaat_move_object(const char *id, int x, int y)
{
    YaatEntity *entity = yaat_entity_by_id_any_room(id);
    YaatRuntimeObject *object = yaat_runtime_object_by_id(id);
    YaatRuntimeObjectMutation *mutation;
    if (entity != 0) {
        entity->x = x;
        entity->y = y;
    }
    if (object != 0) {
        mutation = yaat_runtime_object_mutation(g_runtime_load.room.id, id, 1);
        object->x = x;
        object->y = y;
        if (mutation != 0) {
            mutation->has_position = 1;
            mutation->x = x;
            mutation->y = y;
        }
    }
}

static void yaat_set_object_sprite(const char *id, const char *sprite)
{
    YaatRuntimeObject *object = yaat_runtime_object_by_id(id);
    YaatRuntimeObjectMutation *mutation;
    if (object != 0) {
        mutation = yaat_runtime_object_mutation(g_runtime_load.room.id, id, 1);
        yaat_copy(object->sprite, sizeof(object->sprite), sprite, strlen(sprite));
        object->animation[0] = '\0';
        object->animation_frame_count = 0;
        if (mutation != 0) {
            mutation->has_sprite = 1;
            mutation->has_animation = 0;
            yaat_copy(mutation->sprite, sizeof(mutation->sprite), sprite, strlen(sprite));
        }
    }
}

static void yaat_set_object_animation(const char *id, const char *animation)
{
    YaatRuntimeObject *object = yaat_runtime_object_by_id(id);
    YaatRuntimeObjectMutation *mutation;
    if (object != 0) {
        mutation = yaat_runtime_object_mutation(g_runtime_load.room.id, id, 1);
        yaat_copy(object->animation, sizeof(object->animation), animation, strlen(animation));
        g_animation_clock_ms = 0;
        if (mutation != 0) {
            mutation->has_animation = 1;
            mutation->has_sprite = 0;
            yaat_copy(mutation->animation, sizeof(mutation->animation), animation, strlen(animation));
        }
    }
}

static void yaat_deselect_action(void)
{
    g_selected_verb[0] = '\0';
    g_selected_inventory[0] = '\0';
    yaat_update_command_feedback();
}

static const char *yaat_active_verb(void)
{
    return g_selected_verb[0] != '\0' ? g_selected_verb : "walk";
}


static void yaat_default_inventory_action_sentence(const char *verb)
{
    if (verb == 0 || verb[0] == '\0' || strcmp(verb, "walk") == 0) {
        yaat_player_say("I can't walk there.");
    } else if (strcmp(verb, "use") == 0) {
        yaat_player_say("I can't use those together.");
    } else if (strcmp(verb, "look") == 0) {
        yaat_player_say("I don't see anything special about it.");
    } else if (strcmp(verb, "read") == 0) {
        yaat_player_say("There is nothing on it to read.");
    } else if (strcmp(verb, "take") == 0) {
        yaat_player_say("I already have it.");
    } else if (strcmp(verb, "talk") == 0) {
        yaat_player_say("It doesn't answer.");
    } else if (strcmp(verb, "open") == 0) {
        yaat_player_say("I can't open it.");
    } else if (strcmp(verb, "close") == 0) {
        yaat_player_say("I can't close it.");
    } else {
        yaat_player_say("I can't do that with it.");
    }
}

static void yaat_default_action_sentence(const char *verb)
{
    if (verb == 0 || verb[0] == '\0' || strcmp(verb, "walk") == 0) {
        yaat_player_say("I can't walk there.");
    } else if (strcmp(verb, "use") == 0) {
        yaat_player_say("I can't use that.");
    } else if (strcmp(verb, "look") == 0) {
        yaat_player_say("I don't see anything special.");
    } else if (strcmp(verb, "read") == 0) {
        yaat_player_say("There is nothing to read.");
    } else if (strcmp(verb, "take") == 0) {
        yaat_player_say("I can't take that.");
    } else if (strcmp(verb, "talk") == 0) {
        yaat_player_say("I can't talk to that.");
    } else if (strcmp(verb, "open") == 0) {
        yaat_player_say("I can't open that.");
    } else if (strcmp(verb, "close") == 0) {
        yaat_player_say("I can't close that.");
    } else {
        yaat_player_say("I can't do that.");
    }
}

static YaatEvent *yaat_find_event(YaatEvent *events, int count, const char *name, const char *item)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (strcmp(events[i].name, name) == 0 && (item == 0 || events[i].item[0] == '\0' || strcmp(events[i].item, item) == 0)) return &events[i];
    }
    return 0;
}

static void yaat_runtime_request_room_assets(const char *room_id);
static void yaat_enter_room(int room_index);
static void yaat_execute_event(YaatEvent *event);
static int yaat_client_to_backbuffer(HWND window, int client_x, int client_y,
                                     int *backbuffer_x, int *backbuffer_y);

#define YAAT_MAX_SCRIPT_CALL_DEPTH 16



static void yaat_execute_commands(int first, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        YaatCommand *cmd = &g_commands[first + i];
        if (cmd->kind == YAAT_CMD_SAY) {
            const char *text;
            yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), cmd->a, strlen(cmd->a));
            text = yaat_runtime_lookup_string(&g_runtime_state.strings, cmd->string_id, cmd->b);
            yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), text, strlen(text));
            g_dialogue_visible = 1;
        } else if (cmd->kind == YAAT_CMD_SET) {
            yaat_set_var_value(cmd->a, &cmd->value);
        } else if (cmd->kind == YAAT_CMD_GOTO) {
            int idx = yaat_room_index_by_id(cmd->a);
            if (idx >= 0) yaat_enter_room(idx);
        } else if (cmd->kind == YAAT_CMD_PLAY_SOUND) {
            yaat_winmm_audio_play_sound(&g_audio, cmd->a);
        } else if (cmd->kind == YAAT_CMD_TAKE) {
            yaat_take_inventory(cmd->a);
        } else if (cmd->kind == YAAT_CMD_PICKUP) {
            YaatEntity *entity = yaat_entity_by_id(&g_rooms[g_current_room], cmd->a);
            if (entity) entity->visible = 0;
            yaat_set_object_visible(cmd->a, 0);
            yaat_take_inventory(cmd->b);
        } else if (cmd->kind == YAAT_CMD_DROP) {
            YaatEntity *entity = yaat_entity_by_id(&g_rooms[g_current_room], cmd->b);
            if (yaat_has_inventory(cmd->a)) {
                yaat_remove_inventory(cmd->a);
                if (entity) entity->visible = 1;
            }
        } else if (cmd->kind == YAAT_CMD_REMOVE_INVENTORY || cmd->kind == YAAT_CMD_CONSUME) {
            yaat_remove_inventory(cmd->a);
        } else if (cmd->kind == YAAT_CMD_HIDE) {
            yaat_set_object_visible(cmd->a, 0);
        } else if (cmd->kind == YAAT_CMD_SHOW) {
            yaat_set_object_visible(cmd->a, 1);
        } else if (cmd->kind == YAAT_CMD_MOVE_OBJECT) {
            yaat_move_object(cmd->a, cmd->bool_value, cmd->int_value);
        } else if (cmd->kind == YAAT_CMD_SET_OBJECT_SPRITE) {
            yaat_set_object_sprite(cmd->a, cmd->b);
        } else if (cmd->kind == YAAT_CMD_ANIMATE_OBJECT) {
            yaat_set_object_animation(cmd->a, cmd->b);
        } else if (cmd->kind == YAAT_CMD_TITLE_CARD) {
            const char *text;
            text = yaat_runtime_lookup_string(&g_runtime_state.strings, cmd->string_id, cmd->a);
            yaat_copy(g_cutscene_overlay_text, sizeof(g_cutscene_overlay_text), text, strlen(text));
            g_cutscene_overlay_visible = 1;
            g_cutscene_overlay_remaining_ms = yaat_clamp_int(cmd->int_value, 0, 60000);
            if (g_cutscene_overlay_remaining_ms > 0) {
                g_script_resume_active = 1;
                g_script_resume_first = first;
                g_script_resume_count = count;
                g_script_resume_index = i + 1;
                return;
            }
        } else if (cmd->kind == YAAT_CMD_WAIT) {
            g_script_wait_remaining_ms = yaat_clamp_int(cmd->int_value, 0, 60000);
            if (g_script_wait_remaining_ms > 0) {
                g_script_resume_active = 1;
                g_script_resume_first = first;
                g_script_resume_count = count;
                g_script_resume_index = i + 1;
                return;
            }
        } else if (cmd->kind == YAAT_CMD_MOVE_PLAYER) {
            g_player_x = yaat_clamp_int(cmd->bool_value, YAAT_PLAYER_WIDTH / 2, YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
            g_player_y = yaat_clamp_int(cmd->int_value, YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1);
            g_target_x = g_player_x;
            g_target_y = g_player_y;
            yaat_clear_player_path();
        } else if (cmd->kind == YAAT_CMD_SET_PLAYER_VISIBLE) {
            g_player_visible = cmd->bool_value != 0;
        } else if (cmd->kind == YAAT_CMD_DIALOG) {
            yaat_start_dialogue(cmd->a);
        } else if (cmd->kind == YAAT_CMD_CHOICE) {
            yaat_select_dialogue_choice(cmd->a);
        } else if (cmd->kind == YAAT_CMD_IF) {
            YaatValue value;
            int matched;

            if (cmd->condition_op == YAAT_COND_TRUTHY) {
                matched = yaat_eval_condition(cmd);
            } else {
                value = yaat_get_var(cmd->a);
                matched = yaat_compare_values(&value, cmd->condition_op,
                                              &cmd->value);
            }
            if (matched) {
                yaat_execute_commands(cmd->first_child, cmd->child_count);
            } else {
                yaat_execute_commands(cmd->first_else_child,
                                      cmd->else_child_count);
            }
        } else if (cmd->kind == YAAT_CMD_CALL) {
            YaatEvent *event = yaat_find_event(g_global_events, g_global_event_count, cmd->a, 0);
            if (event && g_script_call_depth < YAAT_MAX_SCRIPT_CALL_DEPTH) {
                ++g_script_call_depth;
                yaat_execute_commands(event->first_command, event->command_count);
                --g_script_call_depth;
            }
        } else if (cmd->kind == YAAT_CMD_SHAKE) {
            yaat_start_shake(cmd->bool_value, cmd->int_value);
        }
    }
}

static void yaat_update_script_timers(void)
{
    int first;
    int count;
    int index;

    if (g_cutscene_overlay_remaining_ms > 0) {
        g_cutscene_overlay_remaining_ms -= YAAT_FRAME_TIMER_MS;
        if (g_cutscene_overlay_remaining_ms <= 0) {
            g_cutscene_overlay_remaining_ms = 0;
            g_cutscene_overlay_visible = 0;
        }
    }
    if (g_script_wait_remaining_ms > 0) {
        g_script_wait_remaining_ms -= YAAT_FRAME_TIMER_MS;
        if (g_script_wait_remaining_ms < 0) g_script_wait_remaining_ms = 0;
    }
    if (g_command_feedback_override_remaining_ms > 0) {
        g_command_feedback_override_remaining_ms -= YAAT_FRAME_TIMER_MS;
        if (g_command_feedback_override_remaining_ms <= 0) {
            g_command_feedback_override_remaining_ms = 0;
            g_command_feedback_override[0] = '\0';
        }
    }
    if (g_script_resume_active && g_script_wait_remaining_ms <= 0 &&
        g_cutscene_overlay_remaining_ms <= 0) {
        first = g_script_resume_first;
        count = g_script_resume_count;
        index = g_script_resume_index;
        g_script_resume_active = 0;
        if (index < count) yaat_execute_commands(first + index, count - index);
    }
}

static void yaat_execute_event(YaatEvent *event)
{
    if (event) {
        yaat_execute_commands(event->first_command, event->command_count);
        yaat_deselect_action();
    }
}

static void yaat_runtime_request_room_assets(const char *room_id)
{
    YaatRuntimeLoadResult load_result;

    if (room_id == 0 || room_id[0] == '\0') return;
    yaat_capture_runtime_object_state();
    yaat_runtime_load_room_from_store(&g_runtime_asset_store, room_id, &load_result);
    g_runtime_load = load_result;
    yaat_apply_runtime_object_mutations();
    yaat_load_runtime_hotspots();
    yaat_apply_runtime_object_state();
    if (g_runtime_load.ok && g_runtime_load.room.music[0] != '\0') {
        yaat_winmm_audio_play_music(&g_audio, g_runtime_load.room.music);
    } else {
        yaat_winmm_audio_stop_music(&g_audio);
    }
}

static void yaat_enter_room(int room_index)
{
    YaatEvent *enter_event;
    g_current_room = room_index;
    yaat_runtime_request_room_assets(g_rooms[g_current_room].id);
    g_player_x = g_runtime_load.room.has_entry_x ? g_runtime_load.room.entry_x : YAAT_BACKBUFFER_WIDTH / 2;
    g_player_y = g_runtime_load.room.has_entry_y ? g_runtime_load.room.entry_y : YAAT_PLAYFIELD_HEIGHT - 20;
    yaat_player_face_direction(g_runtime_load.room.entry_direction);
    g_target_x = g_player_x;
    g_target_y = g_player_y;
    yaat_clear_player_path();
    enter_event = yaat_find_event(g_rooms[g_current_room].events, g_rooms[g_current_room].event_count, "enter", 0);
    yaat_execute_event(enter_event);
}

static void yaat_offset_events(YaatEvent *events, int count, int command_base)
{
    int i;
    for (i = 0; i < count; ++i) events[i].first_command += command_base;
}

static void yaat_import_global_events(YaatScriptPackage *package, int command_base)
{
    int i;
    for (i = 0; i < package->global_event_count && g_global_event_count < YAAT_MAX_GLOBAL_EVENTS; ++i) {
        YaatEvent event = package->global_events[i];
        event.first_command += command_base;
        g_global_events[g_global_event_count++] = event;
    }
}

static void yaat_import_package(YaatScriptPackage *package)
{
    int i;
    int j;
    int command_base = g_command_count;
    if (!package) return;
    for (i = 0; i < package->var_count; ++i) yaat_set_var(package->vars[i].name, package->vars[i].value.bool_value);
    yaat_import_global_events(package, command_base);
    for (i = 0; i < package->var_count; ++i) yaat_set_var_value(package->vars[i].name, &package->vars[i].value);
    for (i = 0; i < package->command_count && g_command_count < YAAT_MAX_COMMANDS; ++i) {
        YaatCommand command = package->commands[i];
        if (command.child_count > 0) command.first_child += command_base;
        if (command.else_child_count > 0) command.first_else_child += command_base;
        g_commands[g_command_count++] = command;
    }
    for (i = 0; i < package->room_count && g_room_count < YAAT_MAX_ROOMS; ++i) {
        YaatRoom room = package->rooms[i];
        yaat_offset_events(room.events, room.event_count, command_base);
        for (j = 0; j < room.entity_count; ++j) yaat_offset_events(room.entities[j].events, room.entities[j].event_count, command_base);
        g_rooms[g_room_count++] = room;
    }
}

static void yaat_load_script_package(const char *bytecode_path, const char *source_path)
{
    YaatScriptPackage package;
    yaat_script_package_init(&package);
    if (yaat_bytecode_read_file(bytecode_path, &package) ||
        yaat_parse_script_file_into(&package, source_path)) {
        yaat_import_package(&package);
    }
}

static void yaat_load_script_file(const char *path)
{
    YaatScriptPackage package;
    yaat_script_package_init(&package);
    if (yaat_parse_script_file_into(&package, path)) {
        yaat_import_package(&package);
    }
}

static void yaat_load_inventory_scripts(void)
{
    int i;
    yaat_load_script_file("game/scripts/inventory.yaat");
    yaat_load_script_file("scripts/inventory.yaat");
    for (i = 0; i < g_runtime_load.inventory.item_count; ++i) {
        YaatRuntimeInventoryItem *item = &g_runtime_load.inventory.items[i];
        if (item->script[0] != '\0') yaat_load_script_file(item->script);
    }
}

static int yaat_script_path_already_loaded(char paths[][YAAT_ASSET_MAX_PATH], int count, const char *path)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (strcmp(paths[i], path) == 0) return 1;
    }
    return 0;
}

static void yaat_load_inventory_item_scripts(void)
{
    char loaded[YAAT_ASSET_MAX_INVENTORY_ITEMS][YAAT_ASSET_MAX_PATH];
    int loaded_count;
    int i;

    loaded_count = 0;
    for (i = 0; i < g_runtime_load.inventory.item_count; ++i) {
        YaatRuntimeInventoryItem *item;
        char path[YAAT_ASSET_MAX_PATH];
        item = &g_runtime_load.inventory.items[i];
        if (item->script[0] == '\0') continue;
        if (strncmp(item->script, "game/", 5) == 0 || strncmp(item->script, "game\\", 5) == 0) {
            yaat_copy(path, sizeof(path), item->script, strlen(item->script));
        } else {
            yaat_runtime_join_path(path, sizeof(path), "game", item->script);
        }
        if (yaat_script_path_already_loaded(loaded, loaded_count, path)) continue;
        yaat_load_script_file(path);
        if (loaded_count < YAAT_ASSET_MAX_INVENTORY_ITEMS) {
            yaat_copy(loaded[loaded_count], sizeof(loaded[loaded_count]), path, strlen(path));
            ++loaded_count;
        }
    }
}

static void yaat_load_demo(void)
{
    int room_index;

    yaat_load_script_package("game/scripts/startup.yaatbc", "game/scripts/startup.yaat");
    yaat_load_script_package("game/rooms/room000_start/script.yaatbc", "game/rooms/room000_start/script.yaat");
    yaat_load_script_package("game/rooms/room001_intro/script.yaatbc", "game/rooms/room001_intro/script.yaat");
    yaat_load_script_package("game/rooms/room002_exit/script.yaatbc", "game/rooms/room002_exit/script.yaat");
    yaat_load_script_file("scripts/startup.yaat");
    yaat_load_script_file("rooms/room000_start/script.yaat");
    yaat_load_script_file("rooms/room001_intro/script.yaat");
    yaat_load_script_file("rooms/room002_exit/script.yaat");
    yaat_load_inventory_scripts();
    yaat_load_inventory_item_scripts();
    yaat_load_player_sprite_metadata();
    room_index = g_runtime_load.ok ? yaat_room_index_by_id(g_runtime_load.room.id) : -1;
    if (room_index >= 0) {
        yaat_enter_room(room_index);
    } else if (g_room_count > 0) {
        yaat_enter_room(0);
    }
}

static void yaat_draw_verb_ui(void)
{
    int i;
    yaat_draw_rect(&g_renderer, 0, YAAT_PLAYFIELD_HEIGHT, YAAT_BACKBUFFER_WIDTH, 40, 0x00101018UL);
    for (i = 0; i < g_verb_count; ++i) {
        int bx = 4 + (i % 3) * YAAT_VERB_BUTTON_WIDTH;
        int by = YAAT_PLAYFIELD_HEIGHT + 3 + (i / 3) * (YAAT_VERB_BUTTON_HEIGHT + 2);
        unsigned long fill = strcmp(g_verbs[i], g_selected_verb) == 0 ? 0x00406090UL : 0x00282838UL;
        yaat_draw_rect(&g_renderer, bx, by, YAAT_VERB_BUTTON_WIDTH - 3, YAAT_VERB_BUTTON_HEIGHT, 0x00000000UL);
        yaat_draw_rect(&g_renderer, bx + 1, by + 1, YAAT_VERB_BUTTON_WIDTH - 5, YAAT_VERB_BUTTON_HEIGHT - 2, fill);
        yaat_draw_text_block(bx + 4, by + 3, g_verbs[i], 0x00f0f0f0UL);
    }
    yaat_update_command_feedback();
    yaat_draw_text_block(164, YAAT_PLAYFIELD_HEIGHT + 28, g_command_feedback, 0x00d8d8e8UL);

    for (i = 0; i < g_inventory_count; ++i) {
        int sx = 164 + (i * (YAAT_INVENTORY_SLOT_SIZE + 3));
        unsigned long fill = strcmp(g_inventory[i], g_selected_inventory) == 0 ? 0x00605020UL : 0x00303030UL;
        yaat_draw_rect(&g_renderer, sx, YAAT_PLAYFIELD_HEIGHT + 4, YAAT_INVENTORY_SLOT_SIZE, YAAT_INVENTORY_SLOT_SIZE, 0x00000000UL);
        yaat_draw_rect(&g_renderer, sx + 1, YAAT_PLAYFIELD_HEIGHT + 5, YAAT_INVENTORY_SLOT_SIZE - 2, YAAT_INVENTORY_SLOT_SIZE - 2, fill);
        yaat_draw_text_block(sx + 5, YAAT_PLAYFIELD_HEIGHT + 10, g_inventory[i], 0x00ffd060UL);
    }
}

static void yaat_draw_inventory_icons(void)
{
    int i;
    YaatTransparency transparency;

    transparency.mode = YAAT_TRANSPARENCY_ALPHA;
    transparency.color_key = 0x00ff00ffUL;
    transparency.mask[0] = '\0';
    for (i = 0; i < g_inventory_count; ++i) {
        YaatBitmap icon;
        char icon_path[YAAT_ASSET_MAX_PATH * 2];
        int slot_x;
        int slot_y;

        slot_x = 180 + (i * 22);
        slot_y = YAAT_PLAYFIELD_HEIGHT + 10;
        memset(&icon, 0, sizeof(icon));
        if (yaat_load_bmp(&icon, "graphics/ui/inventory_slot.bmp")) {
            yaat_draw_bitmap_transparent(&icon, slot_x - 2, slot_y - 2,
                                         &transparency, "graphics/ui");
            yaat_unload_bitmap(&icon);
        } else {
            yaat_draw_rect(&g_renderer, slot_x - 2, slot_y - 2, 20, 20, 0x00303038UL);
        }
        memset(&icon, 0, sizeof(icon));
        yaat_runtime_join_path(icon_path, sizeof(icon_path), "inventory/icons", g_inventory[i]);
        strncat(icon_path, ".bmp", sizeof(icon_path) - 1 - strlen(icon_path));
        if (yaat_load_bmp(&icon, icon_path)) {
            yaat_draw_bitmap_transparent(&icon, slot_x, slot_y, &transparency,
                                         "inventory/icons");
            yaat_unload_bitmap(&icon);
        } else {
            yaat_draw_rect(&g_renderer, slot_x, slot_y, 16, 16,
                           yaat_hash_color(g_inventory[i], 0x00d0b060UL));
        }
    }
}


static unsigned long yaat_entity_border_color(const YaatEntity *entity)
{
    if (entity != 0 && entity->kind == YAAT_ENTITY_OBJECT) return 0x00d4b24cUL;
    if (entity != 0 && entity->kind == YAAT_ENTITY_NPC) return 0x00935fd4UL;
    return 0x004e8bc4UL;
}

static unsigned long yaat_entity_fill_color(const YaatEntity *entity)
{
    if (entity != 0 && entity->kind == YAAT_ENTITY_OBJECT) return 0x00ffe090UL;
    if (entity != 0 && entity->kind == YAAT_ENTITY_NPC) return 0x00d8a8ffUL;
    return 0x008ec5ffUL;
}

static void yaat_draw_script_scene(void)
{
    int i;
    int dialogue_x;
    int dialogue_y;
    YaatRoom *room = &g_rooms[g_current_room];
    yaat_gdi_renderer_clear(&g_renderer, room->color);
    yaat_draw_rect(&g_renderer, g_shake_offset_x, YAAT_PLAYFIELD_HEIGHT - 44 + g_shake_offset_y, YAAT_BACKBUFFER_WIDTH, 44, 0x008a6f48UL);
    for (i = 0; i < room->entity_count; ++i) {
        YaatEntity *e = &room->entities[i];
        if (!e->visible) continue;
        yaat_draw_rect(&g_renderer, e->x + g_shake_offset_x, e->y + g_shake_offset_y, e->w, e->h, yaat_entity_border_color(e));
        yaat_draw_rect(&g_renderer, e->x + 2 + g_shake_offset_x, e->y + 2 + g_shake_offset_y, e->w - 4, e->h - 4, yaat_entity_fill_color(e));
    }
    yaat_draw_rect(&g_renderer, g_target_x - 5 + g_shake_offset_x, g_target_y - 1 + g_shake_offset_y, 11, 3, 0x000f3c70UL);
    yaat_draw_rect(&g_renderer, g_target_x - 1 + g_shake_offset_x, g_target_y - 5 + g_shake_offset_y, 3, 11, 0x000f3c70UL);
    if (g_player_visible) yaat_draw_player();
    if (yaat_dialogue_position_for_speaker(&dialogue_x, &dialogue_y)) {
        yaat_draw_text_block(dialogue_x, dialogue_y, g_dialogue_text, 0x00ffffffUL);
    } else if (g_dialogue_visible) {
        YaatEntity *speaker = yaat_entity_by_id(room, g_dialogue_speaker);
        if (speaker != 0 && speaker->visible) {
            dialogue_x = yaat_clamp_int(speaker->x - 42, 0, YAAT_BACKBUFFER_WIDTH - 120);
            dialogue_y = yaat_clamp_int(speaker->y - 16, 0, YAAT_PLAYFIELD_HEIGHT - 16);
            yaat_draw_text_block(dialogue_x, dialogue_y, g_dialogue_text, 0x00ffffffUL);
        }
    }
    yaat_draw_inventory_bar();
    yaat_draw_rect(&g_renderer, 0, YAAT_PLAYFIELD_HEIGHT, YAAT_BACKBUFFER_WIDTH, 40, 0x00101018UL);
    yaat_draw_inventory_icons();
    if (g_dialogue_visible) {
        yaat_draw_text_block(8, YAAT_PLAYFIELD_HEIGHT + 6, g_dialogue_speaker, 0x00ffd060UL);
        yaat_draw_text_block(70, YAAT_PLAYFIELD_HEIGHT + 6, g_dialogue_text, 0x00f0f0f0UL);
    } else {
        yaat_draw_text_block(8, YAAT_PLAYFIELD_HEIGHT + 12, "Click hotspots to play the demo", 0x00808080UL);
    }
}



static int yaat_save_menu_slot_at(int x, int y)
{
    int i;
    for (i = 0; i < YAAT_SAVE_SLOT_COUNT; ++i) {
        int row_y = 76 + i * 32;
        if (x >= 44 && x < 276 && y >= row_y && y < row_y + 26) return i;
    }
    return -1;
}

static void yaat_save_slot_label_for_menu(int slot, char *label, size_t label_size)
{
    char key[32];
    char fallback[32];
    sprintf(key, "ui.save.slot%d", slot + 1);
    yaat_default_save_slot_label(slot, fallback, sizeof(fallback));
    yaat_get_ui_string(key, fallback, label, label_size);
}

static void yaat_open_save_menu(YaatSaveMenuMode mode)
{
    g_save_menu_mode = mode;
    g_save_menu_selected_slot = 0;
}

static void yaat_close_save_menu(void)
{
    g_save_menu_mode = YAAT_SAVE_MENU_CLOSED;
}

static void yaat_save_menu_accept(void)
{
    char path[64];
    char label[32];
    YaatSaveSlotInfo info;

    if (g_save_menu_selected_slot < 0 || g_save_menu_selected_slot >= YAAT_SAVE_SLOT_COUNT) return;
    yaat_save_slot_path(g_save_menu_selected_slot, path, sizeof(path));
    if (g_save_menu_mode == YAAT_SAVE_MENU_SAVE) {
        yaat_save_slot_label_for_menu(g_save_menu_selected_slot, label, sizeof(label));
        yaat_save_script_state(path, label);
        yaat_close_save_menu();
    } else if (g_save_menu_mode == YAAT_SAVE_MENU_LOAD) {
        yaat_read_save_slot_info(g_save_menu_selected_slot, &info);
        if (info.exists && yaat_load_script_state(path)) yaat_close_save_menu();
    }
}

static void yaat_draw_save_menu(void)
{
    int i;
    char title[32];
    char prompt[96];
    char empty[32];
    char cancel[32];

    yaat_get_ui_string(g_save_menu_mode == YAAT_SAVE_MENU_SAVE ? "ui.save.title" : "ui.load.title",
                       g_save_menu_mode == YAAT_SAVE_MENU_SAVE ? "Save Game" : "Load Game",
                       title, sizeof(title));
    yaat_get_ui_string("ui.save.empty", "Empty", empty, sizeof(empty));
    yaat_get_ui_string("ui.save.cancel", "Esc: Cancel", cancel, sizeof(cancel));

    yaat_draw_rect(&g_renderer, 28, 38, 264, 164, 0x00000000UL);
    yaat_draw_rect(&g_renderer, 30, 40, 260, 160, 0x00202030UL);
    yaat_draw_rect(&g_renderer, 32, 42, 256, 16, 0x00406090UL);
    yaat_draw_text_block(40, 46, title, 0x00ffffffUL);

    for (i = 0; i < YAAT_SAVE_SLOT_COUNT; ++i) {
        YaatSaveSlotInfo info;
        char row[128];
        char slot_label[32];
        unsigned long fill = (i == g_save_menu_selected_slot) ? 0x005070a0UL : 0x00303040UL;
        yaat_read_save_slot_info(i, &info);
        yaat_save_slot_label_for_menu(i, slot_label, sizeof(slot_label));
        yaat_draw_rect(&g_renderer, 44, 76 + i * 32, 232, 26, 0x00000000UL);
        yaat_draw_rect(&g_renderer, 46, 78 + i * 32, 228, 22, fill);
        if (info.exists) {
            sprintf(row, "%s  %s", slot_label, info.timestamp[0] != '\0' ? info.timestamp : info.room);
            yaat_draw_text_block(52, 81 + i * 32, row, 0x00ffffffUL);
            sprintf(row, "%s  %lu:%02lu", info.room[0] != '\0' ? info.room : yaat_current_room_label(),
                    info.play_time_ms / 60000UL, (info.play_time_ms / 1000UL) % 60UL);
            yaat_draw_text_block(52, 90 + i * 32, row, 0x00c0c0c0UL);
        } else {
            sprintf(row, "%s  %s", slot_label, empty);
            yaat_draw_text_block(52, 85 + i * 32, row, 0x00c0c0c0UL);
        }
    }

    yaat_get_ui_string(g_save_menu_mode == YAAT_SAVE_MENU_SAVE ? "ui.save.prompt" : "ui.load.prompt",
                       g_save_menu_mode == YAAT_SAVE_MENU_SAVE ? "Enter/click: Save or overwrite" : "Enter/click: Load",
                       prompt, sizeof(prompt));
    yaat_draw_text_block(44, 178, prompt, 0x00ffd060UL);
    yaat_draw_text_block(190, 178, cancel, 0x00ffd060UL);
}

static void yaat_render_scene(void)
{
    if (g_splash_remaining_ms > 0) {
        yaat_draw_splash_screen();
        return;
    }
    if (g_runtime_load.ok) {
        yaat_draw_runtime_room();
    } else if (g_room_count > 0) {
        yaat_draw_script_scene();
    } else {
        yaat_draw_error_scene();
    }
    if (g_player_visible) yaat_draw_verb_ui();
    if (g_dialogue_visible) {
        yaat_draw_text_block(8, YAAT_PLAYFIELD_HEIGHT + 25, g_dialogue_speaker, 0x00ffd060UL);
        yaat_draw_text_block(70, YAAT_PLAYFIELD_HEIGHT + 25, g_dialogue_text, 0x00f0f0f0UL);
    }
    yaat_draw_dialogue_choices();
    if (g_cutscene_overlay_visible) {
        yaat_gdi_renderer_clear(&g_renderer, 0x00000000UL);
        yaat_draw_text_block(64, (YAAT_BACKBUFFER_HEIGHT / 2) - 8, g_cutscene_overlay_text, 0x00a0c8ffUL);
    }
    if (g_save_menu_mode != YAAT_SAVE_MENU_CLOSED) yaat_draw_save_menu();
    yaat_draw_cursor_placeholder();
}


static void yaat_draw_text_scaled(int x, int y, const char *text,
                                  unsigned long color, int scale)
{
    int i;
    int cx = x;
    int cy = y;
    int font_ready;

    if (text == 0) return;
    if (scale < 1) scale = 1;
    font_ready = g_font_bitmap.pixels != 0 ||
                 yaat_load_bmp(&g_font_bitmap, YAAT_FONT_PATH);

    for (i = 0; text[i] != '\0' && cy < YAAT_BACKBUFFER_HEIGHT - (YAAT_FONT_CELL_HEIGHT * scale); ++i) {
        unsigned char ch;

        if (text[i] == '\n' || cx > YAAT_BACKBUFFER_WIDTH - (YAAT_FONT_CELL_WIDTH * scale)) {
            cx = x;
            cy += YAAT_FONT_CELL_HEIGHT * scale;
            if (text[i] == '\n') continue;
        }
        ch = (unsigned char)text[i];
        if (ch != ' ') {
            if (font_ready) {
                int glyph_index;
                int glyph_x;
                int glyph_y;
                int row;

                if (ch < YAAT_FONT_GLYPH_FIRST ||
                    ch >= YAAT_FONT_GLYPH_FIRST + YAAT_FONT_GLYPH_COUNT) {
                    ch = '?';
                }
                glyph_index = (int)ch - YAAT_FONT_GLYPH_FIRST;
                glyph_x = (glyph_index % YAAT_FONT_COLUMNS) * YAAT_FONT_CELL_WIDTH;
                glyph_y = (glyph_index / YAAT_FONT_COLUMNS) * YAAT_FONT_CELL_HEIGHT;

                for (row = 0; row < YAAT_FONT_CELL_HEIGHT; ++row) {
                    int col;
                    int src_y = glyph_y + row;
                    for (col = 0; col < YAAT_FONT_CELL_WIDTH; ++col) {
                        unsigned long pixel = g_font_bitmap.pixels[(src_y * g_font_bitmap.width) + glyph_x + col] & 0x00ffffffUL;
                        if (pixel != YAAT_FONT_TRANSPARENT) {
                            yaat_draw_rect(&g_renderer, cx + (col * scale), cy + (row * scale),
                                           scale, scale, color);
                        }
                    }
                }
            } else {
                yaat_draw_rect(&g_renderer, cx, cy, YAAT_FONT_CELL_WIDTH * scale,
                               YAAT_FONT_CELL_HEIGHT * scale, color);
            }
        }
        cx += YAAT_FONT_CELL_WIDTH * scale;
    }
}

static void yaat_draw_splash_screen(void)
{
    int title_scale = 5;
    int subtitle_scale = 2;
    int title_width = (int)strlen(YAAT_SPLASH_TITLE) * YAAT_FONT_CELL_WIDTH * title_scale;
    int subtitle_width = (int)strlen(YAAT_SPLASH_SUBTITLE) * YAAT_FONT_CELL_WIDTH * subtitle_scale;
    int title_x = (YAAT_BACKBUFFER_WIDTH - title_width) / 2;
    int title_y = (YAAT_BACKBUFFER_HEIGHT / 2) - 40;
    int subtitle_x = (YAAT_BACKBUFFER_WIDTH - subtitle_width) / 2;

    yaat_gdi_renderer_clear(&g_renderer, 0x00000000UL);
    yaat_draw_text_scaled(title_x + 2, title_y + 2, YAAT_SPLASH_TITLE, 0x00303040UL, title_scale);
    yaat_draw_text_scaled(title_x, title_y, YAAT_SPLASH_TITLE, 0x00f0f0f0UL, title_scale);
    yaat_draw_text_scaled(subtitle_x, title_y + 52, YAAT_SPLASH_SUBTITLE, 0x00a0c8ffUL, subtitle_scale);
}

static void yaat_update_player(void)
{
    int dx;
    int dy;
    int next_x;
    int next_y;
    int moving;
    int moved;
    YaatAnimationClip *clip;
    YaatAnimationFrame *frame;

    g_target_x = yaat_clamp_int(g_target_x, YAAT_PLAYER_WIDTH / 2,
                                YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    g_target_y = yaat_clamp_int(g_target_y, YAAT_PLAYER_HEIGHT,
                                YAAT_PLAYFIELD_HEIGHT - 1);
    if (!yaat_is_walkable_at(g_target_x, g_target_y)) {
        g_target_x = g_player_x;
        g_target_y = g_player_y;
        yaat_clear_player_path();
    }
    dx = g_target_x - g_player_x;
    dy = g_target_y - g_player_y;
    yaat_set_player_animation(yaat_player_walk_animation_for_delta(dx, dy));
    {
        int step_pixels = yaat_player_current_step_pixels();
        if (dx > step_pixels) dx = step_pixels; else if (dx < -step_pixels) dx = -step_pixels;
        if (dy > step_pixels) dy = step_pixels; else if (dy < -step_pixels) dy = -step_pixels;
    }
    next_x = g_player_x + dx;
    next_y = g_player_y + dy;
    moved = 0;
    if (yaat_is_walkable_at(next_x, next_y)) {
        g_player_x = next_x;
        g_player_y = next_y;
        moved = dx != 0 || dy != 0;
    } else {
        if (dx != 0 && yaat_is_walkable_at(next_x, g_player_y)) {
            g_player_x = next_x;
            moved = 1;
        } else {
            g_target_x = g_player_x;
            yaat_clear_player_path();
        }
        if (dy != 0 && yaat_is_walkable_at(g_player_x, next_y)) {
            g_player_y = next_y;
            moved = 1;
        } else {
            g_target_y = g_player_y;
            yaat_clear_player_path();
        }
    }

    if (g_player_x == g_target_x && g_player_y == g_target_y) {
        yaat_advance_player_path();
        if (yaat_player_motion_complete()) {
            yaat_pending_room_change_maybe_complete();
            yaat_pending_interaction_maybe_complete();
        }
    }
    yaat_room_change_region_maybe_enter();

    moving = moved;
    if (moving) {
        yaat_set_player_animation(yaat_player_walk_animation_for_delta(dx, dy));
        clip = yaat_player_animation(g_player_animation_id);
        if (clip != 0 && clip->frame_count > 1) {
            if (g_player_animation_frame >= clip->frame_count) {
                g_player_animation_frame = 0;
            }
            frame = &clip->frames[g_player_animation_frame];
            g_player_animation_elapsed_ms += YAAT_FRAME_TIMER_MS;
            if (g_player_animation_elapsed_ms >=
                (unsigned long)(frame->duration_ms > 0 ?
                                frame->duration_ms : clip->default_frame_ms)) {
                g_player_animation_elapsed_ms = 0;
                ++g_player_animation_frame;
                if (g_player_animation_frame >= clip->frame_count) {
                    g_player_animation_frame = clip->loop ? 0 :
                                               clip->frame_count - 1;
                }
            }
        }
    } else {
        yaat_set_player_animation(yaat_player_idle_animation());
    }
    yaat_pending_room_change_maybe_complete();
}

static void yaat_nudge_player_target(int dx, int dy)
{
    int target_x;
    int target_y;

    yaat_pending_room_change_clear();
    if (g_path_waypoint_count > 0) {
        target_x = g_path_waypoint_x[g_path_waypoint_count - 1];
        target_y = g_path_waypoint_y[g_path_waypoint_count - 1];
    } else {
        target_x = g_target_x;
        target_y = g_target_y;
    }
    yaat_set_player_target(target_x + dx, target_y + dy);
}


static const char *yaat_runtime_logical_path(const char *path)
{
    if (path == 0) return "";
    if (strncmp(path, "game/", 5) == 0 || strncmp(path, "game\\", 5) == 0) {
        return path + 5;
    }
    return path;
}

static void yaat_runtime_join_path(char *dst, size_t dst_size, const char *left, const char *right)
{
    size_t len;
    yaat_copy(dst, dst_size, left != 0 ? left : "", strlen(left != 0 ? left : ""));
    len = strlen(dst);
    if (len > 0 && len < dst_size - 1 && dst[len - 1] != '/' && dst[len - 1] != '\\') {
        dst[len++] = '/';
        dst[len] = '\0';
    }
    if (len < dst_size - 1 && right != 0) {
        strncat(dst, right, dst_size - 1 - len);
    }
}

static int yaat_runtime_room_script_index(void)
{
    int idx;
    if (!g_runtime_load.ok) return g_current_room;
    idx = yaat_room_index_by_id(g_runtime_load.room.id);
    if (idx >= 0) return idx;
    return g_current_room;
}

static void yaat_execute_entity_verb(YaatEntity *entity, const char *verb,
                                     const char *item, const char *default_verb)
{
    YaatEvent *event;
    const char *fallback;

    if (entity == 0 || verb == 0 || verb[0] == '\0') return;
    event = 0;
    if (item != 0 && item[0] != '\0') {
        event = yaat_find_event(entity->events, entity->event_count, verb, item);
    }
    if (event == 0) event = yaat_find_event(entity->events, entity->event_count, verb, 0);
    fallback = (default_verb != 0 && default_verb[0] != '\0') ? default_verb : verb;
    if (event == 0 && strcmp(fallback, verb) != 0) {
        event = yaat_find_event(entity->events, entity->event_count, fallback, 0);
    }
    if (event != 0) {
        yaat_execute_event(event);
    } else {
        yaat_default_action_sentence(fallback);
        yaat_deselect_action();
    }
}

static void yaat_runtime_execute_entity_event(const char *entity_id, const char *script_event)
{
    char event_name[32];
    YaatRoom *room;
    YaatEntity *entity;

    if (g_room_count <= 0) return;
    room = &g_rooms[yaat_runtime_room_script_index()];
    entity = yaat_entity_by_id(room, entity_id);
    if (entity == 0) return;

    (void)script_event;
    yaat_copy(event_name, sizeof(event_name), yaat_active_verb(), strlen(yaat_active_verb()));
    if (strcmp(event_name, "use") == 0 && g_selected_inventory[0] != '\0') {
        YaatRuntimeHotspot *hotspot = yaat_runtime_hotspot_by_id(entity_id);
        yaat_execute_entity_verb(entity, event_name, g_selected_inventory, 0);
        yaat_suppress_room_change_until_player_leaves(hotspot);
    } else {
        yaat_execute_entity_verb(entity, event_name, 0, 0);
    }
}

static void yaat_runtime_execute_entity_longclick(const char *entity_id)
{
    YaatRoom *room;
    YaatEntity *entity;

    if (g_room_count <= 0) return;
    room = &g_rooms[yaat_runtime_room_script_index()];
    entity = yaat_entity_by_id(room, entity_id);
    yaat_execute_entity_verb(entity, "longclick", 0, "look");
}

static int yaat_runtime_ini_hit(const char *path, int x, int y, char *id,
                                size_t id_size, char *script_event,
                                size_t script_event_size)
{
    unsigned char *buffer;
    size_t buffer_size;
    char *line;
    char current_id[YAAT_ASSET_MAX_NAME];
    char current_event[32];
    int rx, ry, rw, rh;
    int has_rect;

    if (!yaat_asset_read_all(&g_asset_store, path, &buffer, &buffer_size)) return 0;
    current_id[0] = '\0';
    current_event[0] = '\0';
    rx = ry = rw = rh = has_rect = 0;

#define YAAT_RUNTIME_CHECK_HIT() \
    do { \
        if (current_id[0] != '\0' && has_rect && x >= rx && y >= ry && \
            x < rx + rw && y < ry + rh) { \
            yaat_copy(id, id_size, current_id, strlen(current_id)); \
            yaat_copy(script_event, script_event_size, \
                      current_event[0] != '\0' ? current_event : "on_click", \
                      strlen(current_event[0] != '\0' ? current_event : "on_click")); \
            free(buffer); \
            return 1; \
        } \
    } while (0)

    for (line = strtok((char *)buffer, "\n"); line != 0;
         line = strtok(0, "\n")) {
        char *text = line;
        char *equals;
        while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
        if (*text == '\0' || *text == ';' || *text == '#') continue;
        if (*text == '[') {
            char *close;
            YAAT_RUNTIME_CHECK_HIT();
            close = strchr(text, ']');
            if (close != 0) {
                *close = '\0';
                yaat_copy(current_id, sizeof(current_id), text + 1, strlen(text + 1));
                current_event[0] = '\0';
                rx = ry = rw = rh = has_rect = 0;
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == 0) continue;
        *equals = '\0';
        ++equals;
        if (strncmp(text, "rect", 4) == 0) {
            if (sscanf(equals, "%d,%d,%d,%d", &rx, &ry, &rw, &rh) == 4) has_rect = 1;
        } else if (strncmp(text, "x", 1) == 0 && text[1] == '\0') {
            rx = atoi(equals);
            has_rect = rw > 0 && rh > 0;
        } else if (strncmp(text, "y", 1) == 0 && text[1] == '\0') {
            ry = atoi(equals);
            has_rect = rw > 0 && rh > 0;
        } else if (strncmp(text, "width", 5) == 0) {
            rw = atoi(equals);
            has_rect = rw > 0 && rh > 0;
        } else if (strncmp(text, "height", 6) == 0) {
            rh = atoi(equals);
            has_rect = rw > 0 && rh > 0;
        } else if (strncmp(text, "script_event", 12) == 0) {
            char *end;
            while (*equals == ' ' || *equals == '\t') ++equals;
            end = equals + strlen(equals);
            while (end > equals && (*(end - 1) == '\r' || *(end - 1) == '\n' || *(end - 1) == ' ' || *(end - 1) == '\t')) --end;
            *end = '\0';
            yaat_copy(current_event, sizeof(current_event), equals, strlen(equals));
        }
    }
    YAAT_RUNTIME_CHECK_HIT();
    free(buffer);
    return 0;
#undef YAAT_RUNTIME_CHECK_HIT
}


static void yaat_runtime_change_room(const YaatRuntimeHotspot *hotspot)
{
    YaatRuntimeLoadResult next_load;
    YaatEvent *enter_event;
    int script_room_index;
    int player_x;
    int player_y;

    yaat_pending_interaction_clear();
    yaat_pending_room_change_clear();
    if (hotspot == 0 || hotspot->target_room[0] == '\0') return;
    player_x = hotspot->has_target_x ? hotspot->target_x : YAAT_BACKBUFFER_WIDTH / 2;
    player_y = hotspot->has_target_y ? hotspot->target_y : YAAT_PLAYFIELD_HEIGHT - 20;
    yaat_capture_runtime_object_state();
    yaat_runtime_load_room_from_store(&g_asset_store, hotspot->target_room, &next_load);
    if (!next_load.ok) return;

    g_runtime_load = next_load;
    yaat_apply_runtime_object_mutations();
    yaat_apply_runtime_object_state();
    script_room_index = yaat_room_index_by_id(g_runtime_load.room.id);
    if (script_room_index >= 0) g_current_room = script_room_index;
    g_player_x = player_x;
    g_player_y = player_y;
    yaat_player_face_direction(hotspot->target_direction);
    g_target_x = g_player_x;
    g_target_y = g_player_y;
    yaat_clear_player_path();
    if (script_room_index >= 0) {
        enter_event = yaat_find_event(g_rooms[g_current_room].events,
                                      g_rooms[g_current_room].event_count,
                                      "enter", 0);
        yaat_execute_event(enter_event);
    }
}

static void yaat_pending_room_change_maybe_complete(void)
{
    YaatRuntimeHotspot *hotspot;

    if (!g_pending_room_change || !g_runtime_load.ok) return;
    if (!yaat_player_motion_complete()) return;
    hotspot = yaat_runtime_hotspot_by_id(g_pending_room_change_hotspot_id);
    if (hotspot == 0 || !yaat_runtime_hotspot_change_room_enabled(hotspot)) {
        yaat_pending_room_change_clear();
        return;
    }
    yaat_runtime_change_room(hotspot);
}

static void yaat_room_change_region_maybe_enter(void)
{
    YaatRuntimeHotspot *hotspot;

    if (g_pending_room_change || g_pending_interaction || !g_runtime_load.ok) return;
    if (!yaat_player_motion_complete()) return;
    hotspot = yaat_runtime_hotspot_at(g_player_x, g_player_y);
    if (!yaat_runtime_hotspot_change_room_enabled(hotspot)) return;
    if (yaat_room_change_suppressed(hotspot)) return;
    yaat_runtime_change_room(hotspot);
}

static void yaat_pending_interaction_maybe_complete(void)
{
    int click_x;
    int click_y;

    if (!g_pending_interaction) return;
    if (!yaat_player_motion_complete()) return;

    click_x = g_pending_interaction_x;
    click_y = g_pending_interaction_y;
    yaat_pending_interaction_clear();
    yaat_click_game(click_x, click_y, 0);
}


static int yaat_runtime_longclick_game(int x, int y)
{
    int i;
    char id[YAAT_ASSET_MAX_NAME];
    char event_name[32];
    char path[YAAT_ASSET_MAX_PATH];
    YaatRuntimeRoom *room = &g_runtime_load.room;

    for (i = room->object_count - 1; i >= 0; --i) {
        YaatRuntimeObject *object = &room->objects[i];
        if (object->visible && x >= object->x && y >= object->y &&
            x < object->x + object->width && y < object->y + object->height) {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
            yaat_runtime_join_path(path, sizeof(path),
                                   yaat_runtime_logical_path(room->room_path),
                                   "objects.ini");
            if (!yaat_runtime_ini_hit(path, x, y, id, sizeof(id), event_name, sizeof(event_name))) {
                yaat_copy(id, sizeof(id), object->id, strlen(object->id));
            }
            yaat_set_look_command_feedback(object->name[0] != '\0' ? object->name : id);
            yaat_runtime_execute_entity_longclick(id);
            return 1;
        }
    }

    for (i = room->hotspot_count - 1; i >= 0; --i) {
        YaatRuntimeHotspot *hotspot = &room->hotspots[i];
        if (hotspot->width > 0 && hotspot->height > 0 && x >= hotspot->x &&
            y >= hotspot->y && x < hotspot->x + hotspot->width &&
            y < hotspot->y + hotspot->height) {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
            yaat_set_look_command_feedback(hotspot->name[0] != '\0' ? hotspot->name : hotspot->id);
            yaat_runtime_execute_entity_longclick(hotspot->id);
            return 1;
        }
    }
    return 0;
}

static int yaat_runtime_click_game(int x, int y, int immediate_room_change)
{
    int i;
    char id[YAAT_ASSET_MAX_NAME];
    char event_name[32];
    char path[YAAT_ASSET_MAX_PATH];
    YaatRuntimeRoom *room = &g_runtime_load.room;

    for (i = room->object_count - 1; i >= 0; --i) {
        YaatRuntimeObject *object = &room->objects[i];
        if (object->visible && x >= object->x && y >= object->y &&
            x < object->x + object->width && y < object->y + object->height) {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
            yaat_runtime_join_path(path, sizeof(path),
                                   yaat_runtime_logical_path(room->room_path),
                                   "objects.ini");
            if (!yaat_runtime_ini_hit(path, x, y, id, sizeof(id), event_name, sizeof(event_name))) {
                yaat_copy(id, sizeof(id), object->id, strlen(object->id));
                yaat_copy(event_name, sizeof(event_name), object->script_event[0] != '\0' ? object->script_event : "on_click", strlen(object->script_event[0] != '\0' ? object->script_event : "on_click"));
            }
            if (strcmp(yaat_active_verb(), "use") == 0 && g_selected_inventory[0] == '\0' && object->inventory_item[0] != '\0') {
                g_selected_verb[0] = '\0';
                yaat_runtime_execute_entity_event(id, event_name);
                yaat_copy(g_selected_verb, sizeof(g_selected_verb), "use", strlen("use"));
                yaat_copy(g_selected_inventory, sizeof(g_selected_inventory), object->inventory_item, strlen(object->inventory_item));
                yaat_update_hover_target_at(x, y);
            } else {
                yaat_runtime_execute_entity_event(id, event_name);
            }
            return 1;
        }
    }

    for (i = room->hotspot_count - 1; i >= 0; --i) {
        YaatRuntimeHotspot *hotspot = &room->hotspots[i];
        if (hotspot->width > 0 && hotspot->height > 0 && x >= hotspot->x &&
            y >= hotspot->y && x < hotspot->x + hotspot->width &&
            y < hotspot->y + hotspot->height) {
            if (yaat_runtime_hotspot_change_room_enabled(hotspot)) {
                if (immediate_room_change) {
                    yaat_pending_interaction_clear();
                    yaat_runtime_change_room(hotspot);
                } else {
                    yaat_pending_interaction_clear();
                    yaat_pending_room_change_set(hotspot);
                }
            } else {
                yaat_pending_interaction_clear();
                yaat_pending_room_change_clear();
                yaat_copy(event_name, sizeof(event_name),
                          hotspot->script_event[0] != '\0' ? hotspot->script_event : "on_click",
                          strlen(hotspot->script_event[0] != '\0' ? hotspot->script_event : "on_click"));
                yaat_runtime_execute_entity_event(hotspot->id, event_name);
            }
            return 1;
        }
    }
    return 0;
}

static YaatEvent *yaat_find_exact_event(YaatEvent *events, int count, const char *name, const char *item)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (strcmp(events[i].name, name) == 0 &&
            strcmp(events[i].item, item != 0 ? item : "") == 0) return &events[i];
    }
    return 0;
}

static YaatEvent *yaat_find_active_entity_event(YaatEntity *entity)
{
    YaatEvent *event;
    const char *verb;

    if (entity == 0) return 0;
    verb = yaat_active_verb();
    event = 0;
    if (strcmp(verb, "use") == 0 && g_selected_inventory[0] != '\0') {
        event = yaat_find_event(entity->events, entity->event_count, verb,
                                g_selected_inventory);
    }
    if (event == 0) {
        event = yaat_find_event(entity->events, entity->event_count, verb, 0);
    }
    return event;
}

static int yaat_should_walk_before_entity_event(YaatEntity *entity)
{
    YaatEvent *event;

    event = yaat_find_active_entity_event(entity);
    if (event == 0) {
        yaat_default_action_sentence(yaat_active_verb());
        yaat_deselect_action();
        return 0;
    }
    if (!event->walk_before) {
        yaat_execute_entity_verb(entity, yaat_active_verb(),
                                 strcmp(yaat_active_verb(), "use") == 0 ?
                                 g_selected_inventory : 0, 0);
        return 0;
    }
    return 1;
}

static void yaat_click_inventory_item(const char *item)
{
    YaatEntity *entity;
    YaatEvent *event = 0;
    YaatRuntimeInventoryItem *runtime_item;
    const char *verb;
    if (item == 0 || item[0] == '\0') return;
    yaat_pending_interaction_clear();
    verb = yaat_active_verb();
    if (strcmp(verb, "use") == 0 && g_selected_inventory[0] == '\0') {
        yaat_copy(g_selected_inventory, sizeof(g_selected_inventory), item, strlen(item));
        return;
    }
    entity = yaat_entity_by_id_any_room(item);
    if (entity != 0) {
        if (strcmp(verb, "use") == 0 && g_selected_inventory[0] != '\0') {
            event = yaat_find_event(entity->events, entity->event_count, verb, g_selected_inventory);
        }
        if (event == 0) event = yaat_find_event(entity->events, entity->event_count, verb, 0);
        if (event == 0 && strcmp(verb, "click") != 0) {
            event = yaat_find_event(entity->events, entity->event_count, "click", 0);
        }
    }
    entity = yaat_entity_by_id_any_room(item);
    if (strcmp(g_selected_verb, "use") == 0) {
        if (g_selected_inventory[0] != '\0' && strcmp(g_selected_inventory, item) != 0 && entity != 0) {
            event = yaat_find_exact_event(entity->events, entity->event_count, "use", g_selected_inventory);
            if (event != 0) {
                yaat_execute_event(event);
                return;
            }
        }
        yaat_copy(g_selected_inventory, sizeof(g_selected_inventory), item, strlen(item));
        return;
    }
    if (entity == 0) return;
    event = yaat_find_event(entity->events, entity->event_count, g_selected_verb, 0);
    if (event == 0 && strcmp(g_selected_verb, "click") != 0) {
        event = yaat_find_event(entity->events, entity->event_count, "click", 0);
    }
    if (event != 0) {
        yaat_execute_event(event);
    } else if (strcmp(verb, "look") == 0 && (runtime_item = yaat_find_runtime_inventory_item(item)) != 0 && runtime_item->description[0] != '\0') {
        yaat_set_look_command_feedback(yaat_runtime_inventory_item_display_name(item));
        yaat_player_say(runtime_item->description);
        yaat_deselect_action();
    } else {
        yaat_default_inventory_action_sentence(verb);
        yaat_deselect_action();
    }
}

static void yaat_longclick_inventory_item(const char *item)
{
    YaatEntity *entity;
    YaatRuntimeInventoryItem *runtime_item;

    if (item == 0 || item[0] == '\0') return;
    yaat_pending_interaction_clear();
    yaat_pending_room_change_clear();
    entity = yaat_entity_by_id_any_room(item);
    if (entity != 0) {
        yaat_set_look_command_feedback(yaat_runtime_inventory_item_display_name(item));
        yaat_execute_entity_verb(entity, "longclick", 0, "look");
    } else if ((runtime_item = yaat_find_runtime_inventory_item(item)) != 0 &&
               runtime_item->description[0] != '\0') {
        yaat_set_look_command_feedback(yaat_runtime_inventory_item_display_name(item));
        yaat_player_say(runtime_item->description);
        yaat_deselect_action();
    } else {
        yaat_set_look_command_feedback(yaat_runtime_inventory_item_display_name(item));
        yaat_default_inventory_action_sentence("look");
        yaat_deselect_action();
    }
}

static void yaat_longclick_game(int x, int y)
{
    int i;
    YaatRoom *room;
    if (g_runtime_load.ok) {
        yaat_runtime_longclick_game(x, y);
        return;
    }
    room = &g_rooms[g_current_room];
    for (i = room->entity_count - 1; i >= 0; --i) {
        YaatEntity *e = &room->entities[i];
        if (e->visible && x >= e->x && y >= e->y && x < e->x + e->w && y < e->y + e->h) {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
            yaat_set_look_command_feedback(e->name[0] != '\0' ? e->name : e->id);
            yaat_execute_entity_verb(e, "longclick", 0, "look");
            return;
        }
    }
}

static void yaat_click_game(int x, int y, int immediate_room_change)
{
    int i;
    YaatRoom *room;
    if (g_runtime_load.ok) {
        yaat_runtime_click_game(x, y, immediate_room_change);
        return;
    }
    room = &g_rooms[g_current_room];
    for (i = room->entity_count - 1; i >= 0; --i) {
        YaatEntity *e = &room->entities[i];
        if (e->visible && x >= e->x && y >= e->y && x < e->x + e->w && y < e->y + e->h) {
            YaatEvent *event = 0;
            yaat_pending_interaction_clear();
            if (strcmp(yaat_active_verb(), "use") == 0 && g_selected_inventory[0] != '\0') {
                event = yaat_find_event(e->events, e->event_count, g_selected_verb, g_selected_inventory);
            }
            if (!event) event = yaat_find_event(e->events, e->event_count, yaat_active_verb(), 0);
            if (event != 0) {
                yaat_execute_event(event);
            } else {
                yaat_default_action_sentence(yaat_active_verb());
                yaat_deselect_action();
            }
            return;
        }
    }
}

static void yaat_calculate_viewport(int client_width, int client_height,
                                    YaatViewport *viewport)
{
    int scaled_width;
    int scaled_height;

    if (viewport == 0) return;
    viewport->x = 0;
    viewport->y = 0;
    viewport->width = 0;
    viewport->height = 0;
    if (client_width <= 0 || client_height <= 0) return;

    scaled_width = client_width;
    scaled_height = (client_width * YAAT_BACKBUFFER_HEIGHT) / YAAT_BACKBUFFER_WIDTH;
    if (scaled_height > client_height) {
        scaled_height = client_height;
        scaled_width = (client_height * YAAT_BACKBUFFER_WIDTH) / YAAT_BACKBUFFER_HEIGHT;
    }
    if (scaled_width < 1) scaled_width = 1;
    if (scaled_height < 1) scaled_height = 1;

    viewport->width = scaled_width;
    viewport->height = scaled_height;
    viewport->x = (client_width - scaled_width) / 2;
    viewport->y = (client_height - scaled_height) / 2;
}

static void yaat_fill_letterbox_bars(HDC dc, const RECT *client_rect,
                                     const YaatViewport *viewport)
{
    HBRUSH black_brush;
    RECT bar_rect;

    if (dc == 0 || client_rect == 0 || viewport == 0) return;

    black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (black_brush == 0) return;

    if (viewport->y > client_rect->top) {
        bar_rect.left = client_rect->left;
        bar_rect.top = client_rect->top;
        bar_rect.right = client_rect->right;
        bar_rect.bottom = viewport->y;
        FillRect(dc, &bar_rect, black_brush);
    }
    if (viewport->y + viewport->height < client_rect->bottom) {
        bar_rect.left = client_rect->left;
        bar_rect.top = viewport->y + viewport->height;
        bar_rect.right = client_rect->right;
        bar_rect.bottom = client_rect->bottom;
        FillRect(dc, &bar_rect, black_brush);
    }
    if (viewport->x > client_rect->left) {
        bar_rect.left = client_rect->left;
        bar_rect.top = viewport->y;
        bar_rect.right = viewport->x;
        bar_rect.bottom = viewport->y + viewport->height;
        FillRect(dc, &bar_rect, black_brush);
    }
    if (viewport->x + viewport->width < client_rect->right) {
        bar_rect.left = viewport->x + viewport->width;
        bar_rect.top = viewport->y;
        bar_rect.right = client_rect->right;
        bar_rect.bottom = viewport->y + viewport->height;
        FillRect(dc, &bar_rect, black_brush);
    }
}

static void yaat_toggle_fullscreen(HWND window)
{
    if (!g_fullscreen) {
        g_windowed_style = (DWORD)GetWindowLongA(window, GWL_STYLE);
        GetWindowRect(window, &g_windowed_rect);
        SetWindowLongA(window, GWL_STYLE, (LONG)WS_POPUP);
        SetWindowPos(window, HWND_TOP, 0, 0,
                     GetSystemMetrics(SM_CXSCREEN),
                     GetSystemMetrics(SM_CYSCREEN),
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = 1;
    } else {
        SetWindowLongA(window, GWL_STYLE, (LONG)g_windowed_style);
        SetWindowPos(window, HWND_NOTOPMOST,
                     g_windowed_rect.left, g_windowed_rect.top,
                     g_windowed_rect.right - g_windowed_rect.left,
                     g_windowed_rect.bottom - g_windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = 0;
    }
    InvalidateRect(window, 0, FALSE);
}

static int yaat_client_to_backbuffer(HWND window, int client_x, int client_y,
                                     int *backbuffer_x, int *backbuffer_y)
{
    RECT client_rect;
    int client_width;
    int client_height;
    YaatViewport viewport;

    if (GetClientRect(window, &client_rect) == 0) return 0;
    client_width = client_rect.right - client_rect.left;
    client_height = client_rect.bottom - client_rect.top;
    yaat_calculate_viewport(client_width, client_height, &viewport);
    if (viewport.width <= 0 || viewport.height <= 0) return 0;
    if (client_x < viewport.x || client_y < viewport.y ||
        client_x >= viewport.x + viewport.width ||
        client_y >= viewport.y + viewport.height) {
        return 0;
    }
    *backbuffer_x = yaat_clamp_int(((client_x - viewport.x) * YAAT_BACKBUFFER_WIDTH) / viewport.width,
                                   0, YAAT_BACKBUFFER_WIDTH - 1);
    *backbuffer_y = yaat_clamp_int(((client_y - viewport.y) * YAAT_BACKBUFFER_HEIGHT) / viewport.height,
                                   0, YAAT_BACKBUFFER_HEIGHT - 1);
    return 1;
}

static void yaat_set_target_from_client(HWND window, int client_x, int client_y,
                                        int immediate_room_change)
{
    int backbuffer_x;
    int backbuffer_y;
    int walk_target_x;
    int walk_target_y;
    int i;

    int verb_index;
    int inventory_index;
    YaatRuntimeHotspot *hotspot;

    if (!yaat_client_to_backbuffer(window, client_x, client_y,
                                   &backbuffer_x, &backbuffer_y)) return;
    g_cursor_x = backbuffer_x;
    g_cursor_y = backbuffer_y;
    verb_index = g_player_visible ? yaat_verb_button_at(backbuffer_x, backbuffer_y) : -1;
    if (verb_index >= 0) {
        yaat_pending_interaction_clear();
        yaat_pending_room_change_clear();
        if (strcmp(g_selected_verb, g_verbs[verb_index]) == 0) {
            yaat_deselect_action();
            yaat_update_command_feedback();
        } else {
            yaat_copy(g_selected_verb, sizeof(g_selected_verb), g_verbs[verb_index], strlen(g_verbs[verb_index]));
            if (strcmp(g_selected_verb, "use") != 0) g_selected_inventory[0] = '\0';
            yaat_update_command_feedback();
        }
        return;
    }
    inventory_index = g_player_visible ? yaat_inventory_slot_at(backbuffer_x, backbuffer_y) : -1;
    if (inventory_index >= 0) {
        yaat_pending_room_change_clear();
        yaat_click_inventory_item(g_inventory[inventory_index]);
        yaat_update_command_feedback();
        return;
    }
    hotspot = g_runtime_load.ok ? yaat_runtime_hotspot_at(backbuffer_x, backbuffer_y) : 0;
    if (immediate_room_change && yaat_runtime_hotspot_change_room_enabled(hotspot)) {
        yaat_click_game(backbuffer_x, backbuffer_y, 1);
        return;
    }
    if (yaat_runtime_hotspot_change_room_enabled(hotspot)) {
        walk_target_x = backbuffer_x;
        walk_target_y = yaat_clamp_int(backbuffer_y, YAAT_PLAYER_HEIGHT,
                                       YAAT_PLAYFIELD_HEIGHT - 1);
        if (yaat_find_walk_target_for_hotspot(hotspot, &walk_target_x,
                                              &walk_target_y)) {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_set(hotspot);
            yaat_set_player_target(walk_target_x, walk_target_y);
            yaat_pending_room_change_maybe_complete();
        } else {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
        }
        return;
    }

    if (g_player_visible && strcmp(yaat_active_verb(), "walk") == 0) {
        if (g_runtime_load.ok) {
            for (i = g_runtime_load.room.object_count - 1; i >= 0; --i) {
                YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
                if (object->visible && backbuffer_x >= object->x &&
                    backbuffer_y >= object->y &&
                    backbuffer_x < object->x + object->width &&
                    backbuffer_y < object->y + object->height) {
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_object(object,
                                                         &walk_target_x,
                                                         &walk_target_y)) {
                        yaat_pending_interaction_clear();
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        return;
                    }
                    break;
                }
            }
        } else if (g_current_room >= 0 && g_current_room < g_room_count) {
            YaatRoom *room = &g_rooms[g_current_room];
            for (i = room->entity_count - 1; i >= 0; --i) {
                YaatEntity *entity = &room->entities[i];
                if (entity->visible && backbuffer_x >= entity->x &&
                    backbuffer_y >= entity->y &&
                    backbuffer_x < entity->x + entity->w &&
                    backbuffer_y < entity->y + entity->h) {
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_rect(entity->x, entity->y,
                                                       entity->w, entity->h,
                                                       &walk_target_x,
                                                       &walk_target_y)) {
                        yaat_pending_interaction_clear();
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        return;
                    }
                    break;
                }
            }
        }
    }

    if (g_selected_verb[0] != '\0' && strcmp(g_selected_verb, "use") != 0) {
        if (g_runtime_load.ok) {
            int script_room_index = yaat_runtime_room_script_index();
            YaatRoom *script_room = (script_room_index >= 0 &&
                                     script_room_index < g_room_count) ?
                                    &g_rooms[script_room_index] : 0;
            for (i = g_runtime_load.room.object_count - 1; i >= 0; --i) {
                YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
                if (object->visible && backbuffer_x >= object->x &&
                    backbuffer_y >= object->y &&
                    backbuffer_x < object->x + object->width &&
                    backbuffer_y < object->y + object->height) {
                    YaatEntity *entity = script_room != 0 ?
                                         yaat_entity_by_id(script_room, object->id) : 0;
                    if (!yaat_should_walk_before_entity_event(entity)) return;
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_object(object,
                                                         &walk_target_x,
                                                         &walk_target_y)) {
                        yaat_pending_interaction_set(backbuffer_x,
                                                     backbuffer_y);
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        yaat_pending_interaction_maybe_complete();
                        return;
                    }
                    break;
                }
            }
            if (hotspot != 0) {
                YaatEntity *entity = script_room != 0 ?
                                     yaat_entity_by_id(script_room, hotspot->id) : 0;
                if (!yaat_should_walk_before_entity_event(entity)) return;
                walk_target_x = backbuffer_x;
                walk_target_y = yaat_clamp_int(backbuffer_y,
                                               YAAT_PLAYER_HEIGHT,
                                               YAAT_PLAYFIELD_HEIGHT - 1);
                if (yaat_find_walk_target_for_hotspot(hotspot, &walk_target_x,
                                                      &walk_target_y)) {
                    yaat_pending_interaction_set(backbuffer_x, backbuffer_y);
                    yaat_pending_room_change_clear();
                    yaat_set_player_target(walk_target_x, walk_target_y);
                    yaat_pending_interaction_maybe_complete();
                    return;
                }
            }
        } else if (g_current_room >= 0 && g_current_room < g_room_count) {
            YaatRoom *room = &g_rooms[g_current_room];
            for (i = room->entity_count - 1; i >= 0; --i) {
                YaatEntity *entity = &room->entities[i];
                if (entity->visible && backbuffer_x >= entity->x &&
                    backbuffer_y >= entity->y &&
                    backbuffer_x < entity->x + entity->w &&
                    backbuffer_y < entity->y + entity->h) {
                    if (!yaat_should_walk_before_entity_event(entity)) return;
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_rect(entity->x, entity->y,
                                                       entity->w, entity->h,
                                                       &walk_target_x,
                                                       &walk_target_y)) {
                        yaat_pending_interaction_set(backbuffer_x,
                                                     backbuffer_y);
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        yaat_pending_interaction_maybe_complete();
                        return;
                    }
                    break;
                }
            }
        }
    }

    if (strcmp(g_selected_verb, "use") == 0) {
        if (g_runtime_load.ok) {
            int script_room_index = yaat_runtime_room_script_index();
            YaatRoom *script_room = (script_room_index >= 0 &&
                                     script_room_index < g_room_count) ?
                                    &g_rooms[script_room_index] : 0;
            for (i = g_runtime_load.room.object_count - 1; i >= 0; --i) {
                YaatRuntimeObject *object = &g_runtime_load.room.objects[i];
                if (object->visible && backbuffer_x >= object->x &&
                    backbuffer_y >= object->y &&
                    backbuffer_x < object->x + object->width &&
                    backbuffer_y < object->y + object->height) {
                    YaatEntity *entity = script_room != 0 ?
                                         yaat_entity_by_id(script_room, object->id) : 0;
                    if (!yaat_should_walk_before_entity_event(entity)) return;
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_object(object,
                                                         &walk_target_x,
                                                         &walk_target_y)) {
                        yaat_pending_interaction_set(backbuffer_x,
                                                     backbuffer_y);
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        yaat_pending_interaction_maybe_complete();
                        return;
                    }
                    break;
                }
            }
            if (hotspot != 0) {
                YaatEntity *entity = script_room != 0 ?
                                     yaat_entity_by_id(script_room, hotspot->id) : 0;
                if (!yaat_should_walk_before_entity_event(entity)) return;
                walk_target_x = backbuffer_x;
                walk_target_y = yaat_clamp_int(backbuffer_y,
                                               YAAT_PLAYER_HEIGHT,
                                               YAAT_PLAYFIELD_HEIGHT - 1);
                if (yaat_find_walk_target_for_hotspot(hotspot, &walk_target_x,
                                                      &walk_target_y)) {
                    yaat_pending_interaction_set(backbuffer_x, backbuffer_y);
                    yaat_pending_room_change_clear();
                    yaat_set_player_target(walk_target_x, walk_target_y);
                    yaat_pending_interaction_maybe_complete();
                    return;
                }
            }
        } else if (g_current_room >= 0 && g_current_room < g_room_count) {
            YaatRoom *room = &g_rooms[g_current_room];
            for (i = room->entity_count - 1; i >= 0; --i) {
                YaatEntity *entity = &room->entities[i];
                if (entity->visible && backbuffer_x >= entity->x &&
                    backbuffer_y >= entity->y &&
                    backbuffer_x < entity->x + entity->w &&
                    backbuffer_y < entity->y + entity->h) {
                    if (!yaat_should_walk_before_entity_event(entity)) return;
                    walk_target_x = backbuffer_x;
                    walk_target_y = yaat_clamp_int(backbuffer_y,
                                                   YAAT_PLAYER_HEIGHT,
                                                   YAAT_PLAYFIELD_HEIGHT - 1);
                    if (yaat_find_walk_target_for_rect(entity->x, entity->y,
                                                       entity->w, entity->h,
                                                       &walk_target_x,
                                                       &walk_target_y)) {
                        yaat_pending_interaction_set(backbuffer_x,
                                                     backbuffer_y);
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        yaat_pending_interaction_maybe_complete();
                        return;
                    }
                    break;
                }
            }
        }
    }

    /* A click that falls through to here targeted the room background, not the
       previously hovered object/hotspot/inventory item. Clear the hover target so
       the command feedback updates from e.g. "Walk to brass key" to "Walk"
       before starting the background walk or deselecting the active verb. */
    yaat_set_hover_target(YAAT_HOVER_EMPTY, "", "");
    yaat_pending_interaction_clear();
    yaat_pending_room_change_clear();
    if (g_selected_verb[0] != '\0') {
        yaat_deselect_action();
        return;
    }
    if (!yaat_is_walkable_at(backbuffer_x, yaat_clamp_int(backbuffer_y, YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1))) {
        yaat_default_action_sentence("walk");
        return;
    }
    (void)yaat_player_walk_animation_for_delta(backbuffer_x - g_player_x,
                                               backbuffer_y - g_player_y);
    yaat_set_player_target(backbuffer_x, backbuffer_y);
    yaat_click_game(backbuffer_x, backbuffer_y, 0);
}

static void yaat_update_hover_target_at(int backbuffer_x, int backbuffer_y)
{
    int inventory_index;
    YaatRuntimeObject *object;
    YaatRuntimeHotspot *hotspot;

    inventory_index = g_player_visible ? yaat_inventory_slot_at(backbuffer_x, backbuffer_y) : -1;
    object = (g_runtime_load.ok && inventory_index < 0) ? yaat_runtime_object_at(backbuffer_x, backbuffer_y) : 0;
    hotspot = (g_runtime_load.ok && inventory_index < 0 && object == 0) ? yaat_runtime_hotspot_at(backbuffer_x, backbuffer_y) : 0;
    if (inventory_index >= 0) {
        const char *item_id = g_inventory[inventory_index];
        yaat_set_hover_target(YAAT_HOVER_INVENTORY, item_id,
                              yaat_runtime_inventory_item_display_name(item_id));
    } else if (object != 0) {
        yaat_set_hover_target(YAAT_HOVER_OBJECT, object->id,
                              object->name[0] != '\0' ? object->name : object->id);
    } else if (hotspot != 0) {
        yaat_set_hover_target(YAAT_HOVER_HOTSPOT, hotspot->id,
                              hotspot->name[0] != '\0' ? hotspot->name : hotspot->id);
    } else {
        yaat_set_hover_target(YAAT_HOVER_EMPTY, "", "");
    }
    yaat_copy(g_cursor_state, sizeof(g_cursor_state),
              hotspot != 0 ? hotspot->cursor : "arrow",
              strlen(hotspot != 0 ? hotspot->cursor : "arrow"));
}

static void yaat_update_cursor_from_client(HWND window, int client_x, int client_y)
{
    int backbuffer_x;
    int backbuffer_y;
    LPCSTR win32_cursor;

    if (!yaat_client_to_backbuffer(window, client_x, client_y,
                                   &backbuffer_x, &backbuffer_y)) return;
    g_cursor_x = backbuffer_x;
    g_cursor_y = backbuffer_y;

    yaat_update_hover_target_at(backbuffer_x, backbuffer_y);

    win32_cursor = strcmp(g_cursor_state, "use") == 0 ? IDC_HAND : IDC_ARROW;
    (void)win32_cursor;
    SetCursor(0);
    InvalidateRect(window, 0, FALSE);
}

static LRESULT CALLBACK yaat_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE: {
        HDC dc = GetDC(window);
        int runtime_room_index;
        if (dc == 0) return -1;
        g_renderer_ready = yaat_gdi_renderer_init(&g_renderer, dc, YAAT_BACKBUFFER_WIDTH, YAAT_BACKBUFFER_HEIGHT);
        ReleaseDC(window, dc);
        if (!g_renderer_ready) return -1;
        yaat_load_verbs();
        yaat_load_demo();
        if (g_runtime_load.ok) {
            runtime_room_index = yaat_room_index_by_id(g_runtime_load.room.id);
            if (runtime_room_index >= 0) g_current_room = runtime_room_index;
            if (g_runtime_load.room.music[0] != '\0') {
                yaat_winmm_audio_play_music(&g_audio, g_runtime_load.room.music);
            }
        }
        SetTimer(window, YAAT_FRAME_TIMER_ID, YAAT_FRAME_TIMER_MS, 0);
        return 0;
    }
    case WM_MOUSEMOVE:
        yaat_update_cursor_from_client(window, (int)(short)LOWORD(l_param),
                                       (int)(short)HIWORD(l_param));
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l_param) == HTCLIENT) {
            SetCursor(0);
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return TRUE;
    case WM_SYSKEYDOWN:
        if (w_param == VK_RETURN && (HIWORD(l_param) & KF_ALTDOWN)) {
            if ((l_param & 0x40000000L) != 0) return 0;
            yaat_toggle_fullscreen(window);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (g_save_menu_mode != YAAT_SAVE_MENU_CLOSED) {
            if (w_param == VK_ESCAPE) {
                yaat_close_save_menu();
            } else if (w_param == VK_UP) {
                if (g_save_menu_selected_slot > 0) --g_save_menu_selected_slot;
            } else if (w_param == VK_DOWN) {
                if (g_save_menu_selected_slot < YAAT_SAVE_SLOT_COUNT - 1) ++g_save_menu_selected_slot;
            } else if (w_param == VK_RETURN || w_param == VK_SPACE) {
                yaat_save_menu_accept();
            } else {
                break;
            }
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (w_param == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) {
            if ((l_param & 0x40000000L) != 0) return 0;
            yaat_toggle_fullscreen(window);
            return 0;
        }
#if defined(_DEBUG) || defined(YAAT_DEBUG)
        if ((GetKeyState(VK_CONTROL) & 0x8000) && w_param == 'S') {
            yaat_save_script_state(YAAT_SAVE_PATH, "Slot1");
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && w_param == 'L') {
            yaat_load_script_state(YAAT_SAVE_PATH);
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
#endif
        if (w_param == VK_F5) {
            yaat_open_save_menu(YAAT_SAVE_MENU_SAVE);
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (w_param == VK_F9) {
            yaat_open_save_menu(YAAT_SAVE_MENU_LOAD);
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (w_param == 'M') {
            yaat_winmm_audio_toggle_muted(&g_audio);
            return 0;
        }
        if (w_param == VK_OEM_MINUS || w_param == VK_SUBTRACT) {
            yaat_winmm_audio_set_volume(&g_audio, yaat_winmm_audio_volume(&g_audio) - 10);
            return 0;
        }
        if (w_param == VK_OEM_PLUS || w_param == VK_ADD) {
            yaat_winmm_audio_set_volume(&g_audio, yaat_winmm_audio_volume(&g_audio) + 10);
            return 0;
        }
        if (w_param == VK_LEFT) yaat_nudge_player_target(-16, 0);
        else if (w_param == VK_RIGHT) yaat_nudge_player_target(16, 0);
        else if (w_param == VK_UP) yaat_nudge_player_target(0, -16);
        else if (w_param == VK_DOWN) yaat_nudge_player_target(0, 16);
        else break;
        InvalidateRect(window, 0, FALSE);
        return 0;
    case WM_RBUTTONDOWN:
        if (g_splash_remaining_ms > 0) {
            g_splash_remaining_ms = 0;
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (g_save_menu_mode != YAAT_SAVE_MENU_CLOSED) return 0;
        if (g_dialogue_visible) g_dialogue_visible = 0;
        if (g_dialogue_choice_visible && yaat_handle_dialogue_click(window, (int)(short)LOWORD(l_param), (int)(short)HIWORD(l_param))) { }
        else if (g_dialogue_visible) { g_dialogue_visible = 0; yaat_dialogue_hide_choices(); }
        else {
            int backbuffer_x;
            int backbuffer_y;
            if (yaat_client_to_backbuffer(window, (int)(short)LOWORD(l_param),
                                          (int)(short)HIWORD(l_param),
                                          &backbuffer_x, &backbuffer_y)) {
                int inventory_index;
                g_cursor_x = backbuffer_x;
                g_cursor_y = backbuffer_y;
                inventory_index = g_player_visible ? yaat_inventory_slot_at(backbuffer_x, backbuffer_y) : -1;
                if (inventory_index >= 0) {
                    yaat_longclick_inventory_item(g_inventory[inventory_index]);
                } else {
                    yaat_longclick_game(backbuffer_x, backbuffer_y);
                }
            }
        }
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_LBUTTONDOWN:
        if (g_splash_remaining_ms > 0) {
            g_splash_remaining_ms = 0;
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (g_save_menu_mode != YAAT_SAVE_MENU_CLOSED) {
            int backbuffer_x;
            int backbuffer_y;
            if (yaat_client_to_backbuffer(window, (int)(short)LOWORD(l_param),
                                          (int)(short)HIWORD(l_param),
                                          &backbuffer_x, &backbuffer_y)) {
                int slot = yaat_save_menu_slot_at(backbuffer_x, backbuffer_y);
                if (slot >= 0) {
                    g_save_menu_selected_slot = slot;
                    yaat_save_menu_accept();
                } else {
                    yaat_close_save_menu();
                }
            }
            InvalidateRect(window, 0, FALSE); return 0;
        }
        if (g_dialogue_visible) g_dialogue_visible = 0;
        if (g_dialogue_choice_visible && yaat_handle_dialogue_click(window, (int)(short)LOWORD(l_param), (int)(short)HIWORD(l_param))) { }
        else if (g_dialogue_visible) { g_dialogue_visible = 0; yaat_dialogue_hide_choices(); }
        else yaat_set_target_from_client(window, (int)(short)LOWORD(l_param),
                                         (int)(short)HIWORD(l_param), 0);
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_LBUTTONDBLCLK:
        if (g_splash_remaining_ms > 0) {
            g_splash_remaining_ms = 0;
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        if (g_save_menu_mode != YAAT_SAVE_MENU_CLOSED) return 0;
        if (g_dialogue_visible) g_dialogue_visible = 0;
        if (g_dialogue_choice_visible && yaat_handle_dialogue_click(window, (int)(short)LOWORD(l_param), (int)(short)HIWORD(l_param))) { }
        else if (g_dialogue_visible) { g_dialogue_visible = 0; yaat_dialogue_hide_choices(); }
        else yaat_set_target_from_client(window, (int)(short)LOWORD(l_param),
                                         (int)(short)HIWORD(l_param), 1);
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_TIMER:
        if (w_param == YAAT_FRAME_TIMER_ID) {
            g_animation_clock_ms += YAAT_FRAME_TIMER_MS;
            if (g_splash_remaining_ms > 0) {
                g_splash_remaining_ms -= YAAT_FRAME_TIMER_MS;
                if (g_splash_remaining_ms < 0) g_splash_remaining_ms = 0;
            }
            if (g_splash_remaining_ms <= 0) yaat_update_player();
            yaat_update_shake();
            yaat_update_script_timers();
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc; RECT client_rect; YaatViewport viewport;
        dc = BeginPaint(window, &paint);
        if (g_renderer_ready && GetClientRect(window, &client_rect) != 0) {
            yaat_calculate_viewport(client_rect.right - client_rect.left,
                                    client_rect.bottom - client_rect.top,
                                    &viewport);
            if (viewport.width > 0 && viewport.height > 0) {
                yaat_render_scene();
                yaat_gdi_renderer_present_stretched(&g_renderer, dc,
                                                    viewport.x, viewport.y,
                                                    viewport.width,
                                                    viewport.height);
                yaat_fill_letterbox_bars(dc, &client_rect, &viewport);
            }
        }
        EndPaint(window, &paint); return 0;
    }
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        KillTimer(window, YAAT_FRAME_TIMER_ID); yaat_winmm_audio_shutdown(&g_audio); yaat_unload_bitmap(&g_font_bitmap); yaat_gdi_renderer_shutdown(&g_renderer); g_renderer_ready = 0; PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcA(window, message, w_param, l_param);
}

static void yaat_apply_language_argument(const char *command_line)
{
    const char *lang;
    char value[YAAT_ASSET_MAX_LANGUAGE];
    int i;
    if (command_line == 0) return;
    lang = strstr(command_line, "--lang=");
    if (lang == 0) lang = strstr(command_line, "-lang=");
    if (lang == 0) return;
    lang = strchr(lang, '=');
    if (lang == 0) return;
    ++lang;
    for (i = 0; i < (int)sizeof(value) - 1 && lang[i] != '\0' && !isspace((unsigned char)lang[i]); ++i) {
        value[i] = lang[i];
    }
    value[i] = '\0';
    if (value[0] != '\0') yaat_runtime_state_set_language(&g_runtime_state, value);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    WNDCLASSEXA window_class;
    HWND window;
    MSG message;

    (void)previous_instance;
    yaat_runtime_state_init(&g_runtime_state);
    yaat_apply_language_argument(command_line);
    yaat_asset_store_init_loose(&g_asset_store, "game");
    yaat_asset_store_init_loose(&g_runtime_asset_store, "game");
    yaat_runtime_load_strings_from_store(&g_runtime_asset_store, g_runtime_state.language, &g_runtime_state.strings);
    yaat_winmm_audio_init(&g_audio, &g_runtime_asset_store);
    yaat_runtime_load_start_room_from_store(&g_asset_store, &g_runtime_load);
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class); window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; window_class.lpfnWndProc = yaat_window_proc;
    window_class.hInstance = instance; window_class.hCursor = LoadCursorA(0, IDC_ARROW); window_class.hbrBackground = 0; window_class.lpszClassName = YAAT_WINDOW_CLASS_NAME;
    if (RegisterClassExA(&window_class) == 0) return 1;
    window = CreateWindowExA(0, YAAT_WINDOW_CLASS_NAME, YAAT_WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, 0, 0, instance, 0);
    if (window == 0) return 1;
    ShowWindow(window, show_command); UpdateWindow(window);
    while (GetMessageA(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageA(&message); }
    return (int)message.wParam;
}
