#include <windows.h>

#include "platform/win32/gdi_renderer.h"
#include "platform/win32/audio_winmm.h"
#include "script_parser.h"
#include "script_bytecode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime/asset_loader.h"

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
#define YAAT_MAX_VERBS 8
#define YAAT_VERB_BUTTON_WIDTH 50
#define YAAT_VERB_BUTTON_HEIGHT 13
#define YAAT_INVENTORY_SLOT_SIZE 20
#define YAAT_MAX_RUNTIME_OBJECT_MUTATIONS 64
#define YAAT_NAV_CELL_SIZE 4
#define YAAT_NAV_GRID_WIDTH ((YAAT_BACKBUFFER_WIDTH + YAAT_NAV_CELL_SIZE - 1) / YAAT_NAV_CELL_SIZE)
#define YAAT_NAV_GRID_HEIGHT ((YAAT_PLAYFIELD_HEIGHT + YAAT_NAV_CELL_SIZE - 1) / YAAT_NAV_CELL_SIZE)
#define YAAT_NAV_GRID_CELLS (YAAT_NAV_GRID_WIDTH * YAAT_NAV_GRID_HEIGHT)
#define YAAT_MAX_PATH_WAYPOINTS YAAT_NAV_GRID_CELLS

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
static int g_path_waypoint_x[YAAT_MAX_PATH_WAYPOINTS];
static int g_path_waypoint_y[YAAT_MAX_PATH_WAYPOINTS];
static int g_path_waypoint_count;
static int g_path_waypoint_index;
static int g_player_facing_right = 1;
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
static char g_dialogue_speaker[32];
static char g_dialogue_text[YAAT_TEXT_MAX];
static int g_dialogue_visible;
static int g_dialogue_choice_visible;
static YaatRuntimeDialog g_active_dialog;
static char g_active_dialog_node[YAAT_ASSET_MAX_NAME];
static YaatRuntimeLoadResult g_runtime_load;
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

#define YAAT_SAVE_STATE_VERSION 1
#define YAAT_SAVE_PATH "yaat_save_state.txt"
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

static YaatSavedRuntimeObjectState g_saved_runtime_objects[YAAT_MAX_SAVED_RUNTIME_OBJECTS];
static int g_saved_runtime_object_count;
static int g_suppress_runtime_state_capture;

typedef struct YaatViewport { int x; int y; int width; int height; } YaatViewport;
typedef struct YaatBitmap { unsigned long *pixels; int width; int height; int has_alpha; char path[YAAT_ASSET_MAX_PATH * 2]; } YaatBitmap;
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
static char *yaat_trim_text(char *text);
static int yaat_load_bmp(YaatBitmap *bitmap, const char *path);
static int yaat_player_motion_complete(void);
static void yaat_unload_bitmap(YaatBitmap *bitmap);
static void yaat_click_game(int x, int y, int immediate_room_change);
static void yaat_pending_room_change_maybe_complete(void);
static void yaat_pending_interaction_maybe_complete(void);
static void yaat_update_script_timers(void);
static YaatEntity *yaat_entity_by_id(YaatRoom *room, const char *id);
static YaatRuntimeObject *yaat_runtime_object_by_id(const char *id);
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

static void yaat_unload_bitmap(YaatBitmap *bitmap)
{
    if (bitmap->pixels != 0) {
        free(bitmap->pixels);
    }
    bitmap->pixels = 0;
    bitmap->width = 0;
    bitmap->height = 0;
    bitmap->has_alpha = 0;
    bitmap->path[0] = '\0';
}

static int yaat_load_bmp(YaatBitmap *bitmap, const char *path)
{
    unsigned char *data;
    size_t data_size;
    unsigned char file_header[14];
    unsigned char info_header[40];
    unsigned long pixel_offset;
    long bmp_width;
    long bmp_height;
    int top_down;
    unsigned short planes;
    unsigned short bits_per_pixel;
    unsigned long compression;
    unsigned long row_stride;
    unsigned long palette[256];
    unsigned long palette_entries;
    unsigned long *pixels;
    int x;
    int y;
    int has_alpha;

    if (path == 0 || path[0] == '\0') return 0;
    if (strcmp(bitmap->path, path) == 0 && bitmap->pixels != 0) return 1;

    if (!yaat_asset_read_all(&g_asset_store, path, &data, &data_size)) {
        yaat_unload_bitmap(bitmap);
        yaat_copy(bitmap->path, sizeof(bitmap->path), path, strlen(path));
        return 0;
    }
    if (data_size < sizeof(file_header) + sizeof(info_header)) {
        free(data);
        yaat_unload_bitmap(bitmap);
        return 0;
    }
    memcpy(file_header, data, sizeof(file_header));
    memcpy(info_header, data + sizeof(file_header), sizeof(info_header));

    if (file_header[0] != 'B' || file_header[1] != 'M' ||
        yaat_read_le32(info_header) < 40) {
        free(data);
        yaat_unload_bitmap(bitmap);
        return 0;
    }

    pixel_offset = yaat_read_le32(file_header + 10);
    bmp_width = yaat_read_le32_signed(info_header + 4);
    bmp_height = yaat_read_le32_signed(info_header + 8);
    planes = yaat_read_le16(info_header + 12);
    bits_per_pixel = yaat_read_le16(info_header + 14);
    compression = yaat_read_le32(info_header + 16);

    top_down = 0;
    if (bmp_height < 0) {
        top_down = 1;
        bmp_height = -bmp_height;
    }

    if (bmp_width <= 0 || bmp_height <= 0 || planes != 1 ||
        compression != BI_RGB ||
        (bits_per_pixel != 8 && bits_per_pixel != 24 && bits_per_pixel != 32)) {
        free(data);
        yaat_unload_bitmap(bitmap);
        return 0;
    }

    row_stride = ((((unsigned long)bmp_width * bits_per_pixel) + 31) / 32) * 4;
    if ((size_t)pixel_offset + ((size_t)row_stride * (size_t)bmp_height) > data_size) {
        free(data);
        yaat_unload_bitmap(bitmap);
        return 0;
    }

    if (bits_per_pixel == 8) {
        unsigned long i;
        if (pixel_offset < 54) {
            free(data);
            yaat_unload_bitmap(bitmap);
            return 0;
        }
        palette_entries = (pixel_offset - 54) / 4;
        if (palette_entries > 256) palette_entries = 256;
        if (palette_entries == 0 || 54 + (palette_entries * 4) > data_size) {
            free(data);
            yaat_unload_bitmap(bitmap);
            return 0;
        }
        for (i = 0; i < palette_entries; ++i) {
            const unsigned char *palette_color = data + 54 + (i * 4);
            palette[i] = ((unsigned long)palette_color[0]) |
                         ((unsigned long)palette_color[1] << 8) |
                         ((unsigned long)palette_color[2] << 16);
        }
    } else {
        palette_entries = 0;
    }

    pixels = (unsigned long *)malloc((size_t)bmp_width * (size_t)bmp_height *
                                     sizeof(unsigned long));
    if (pixels == 0) {
        free(data);
        yaat_unload_bitmap(bitmap);
        return 0;
    }

    has_alpha = 0;
    for (y = 0; y < bmp_height; ++y) {
        int dst_y = top_down ? y : (int)bmp_height - 1 - y;
        const unsigned char *row = data + pixel_offset + ((size_t)y * row_stride);
        for (x = 0; x < bmp_width; ++x) {
            const unsigned char *src = row + ((bits_per_pixel / 8) * x);
            if (bits_per_pixel == 8) {
                pixels[(dst_y * (int)bmp_width) + x] =
                    src[0] < palette_entries ? palette[src[0]] : 0;
            } else {
                unsigned long b = src[0];
                unsigned long g = src[1];
                unsigned long r = src[2];
                unsigned long a = bits_per_pixel == 32 ? src[3] : 0xffUL;
                if (bits_per_pixel == 32 && a != 0) has_alpha = 1;
                pixels[(dst_y * (int)bmp_width) + x] = b | (g << 8) | (r << 16) | (a << 24);
            }
        }
    }

    free(data);
    yaat_unload_bitmap(bitmap);
    bitmap->pixels = pixels;
    bitmap->width = (int)bmp_width;
    bitmap->height = (int)bmp_height;
    bitmap->has_alpha = has_alpha;
    yaat_copy(bitmap->path, sizeof(bitmap->path), path, strlen(path));
    return 1;
}

static void yaat_blend_pixel(unsigned long *dst, unsigned long src)
{
    unsigned long alpha;
    unsigned long inv;
    unsigned long rb;
    unsigned long g;

    alpha = (src >> 24) & 0xffUL;
    if (alpha == 0) return;
    if (alpha == 0xffUL) {
        *dst = src & 0x00ffffffUL;
        return;
    }
    inv = 255UL - alpha;
    rb = (((src & 0x00ff00ffUL) * alpha + (*dst & 0x00ff00ffUL) * inv) >> 8) & 0x00ff00ffUL;
    g = (((src & 0x0000ff00UL) * alpha + (*dst & 0x0000ff00UL) * inv) >> 8) & 0x0000ff00UL;
    *dst = rb | g;
}

static void yaat_draw_bitmap_keyed(YaatBitmap *bitmap, int dst_x, int dst_y,
                                   int transparent_color_enabled,
                                   unsigned long transparent_color)
{
    int src_x0;
    int src_y0;
    int copy_width;
    int copy_height;
    int y;

    if (bitmap == 0 || bitmap->pixels == 0) {
        return;
    }

    src_x0 = 0;
    src_y0 = 0;
    copy_width = bitmap->width;
    copy_height = bitmap->height;
    if (dst_x < 0) { src_x0 = -dst_x; copy_width -= src_x0; dst_x = 0; }
    if (dst_y < 0) { src_y0 = -dst_y; copy_height -= src_y0; dst_y = 0; }
    if (dst_x + copy_width > g_renderer.width) copy_width = g_renderer.width - dst_x;
    if (dst_y + copy_height > g_renderer.height) copy_height = g_renderer.height - dst_y;
    if (copy_width <= 0 || copy_height <= 0) {
        return;
    }

    transparent_color &= 0x00ffffffUL;
    for (y = 0; y < copy_height; ++y) {
        unsigned long *dst_row = (unsigned long *)
            ((unsigned char *)g_renderer.pixels + ((dst_y + y) * g_renderer.pitch)) + dst_x;
        unsigned long *src_row =
            bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0;
        int x;

        if (!transparent_color_enabled) {
            memcpy(dst_row, src_row, (size_t)copy_width * sizeof(unsigned long));
            continue;
        }
        for (x = 0; x < copy_width; ++x) {
            if ((src_row[x] & 0x00ffffffUL) != transparent_color) {
                dst_row[x] = src_row[x];
            }
        }
    }
}

static void yaat_draw_bitmap_transparent(YaatBitmap *bitmap, int dst_x, int dst_y,
                                         const YaatTransparency *transparency,
                                         const char *mask_base_path)
{
    YaatBitmap mask_bitmap;
    const YaatBitmap *mask;
    char mask_path[YAAT_ASSET_MAX_PATH * 2];
    int src_x0;
    int src_y0;
    int copy_width;
    int copy_height;
    int x;
    int y;
    YaatTransparencyMode mode;
    unsigned long color_key;

    if (bitmap == 0 || bitmap->pixels == 0) return;

    mode = transparency != 0 ? transparency->mode : YAAT_TRANSPARENCY_ALPHA;
    color_key = transparency != 0 ? transparency->color_key : 0x00ff00ffUL;
    memset(&mask_bitmap, 0, sizeof(mask_bitmap));
    mask = 0;
    if (transparency != 0 && transparency->mask[0] != '\0') {
        if (mask_base_path != 0 && mask_base_path[0] != '\0') {
            yaat_runtime_join_path(mask_path, sizeof(mask_path), mask_base_path,
                                   transparency->mask);
        } else {
            yaat_copy(mask_path, sizeof(mask_path), transparency->mask,
                      strlen(transparency->mask));
        }
        if (yaat_load_bmp(&mask_bitmap, mask_path)) {
            mask = &mask_bitmap;
            mode = YAAT_TRANSPARENCY_MASK;
        }
    }

    src_x0 = 0;
    src_y0 = 0;
    copy_width = bitmap->width;
    copy_height = bitmap->height;
    if (dst_x < 0) { src_x0 = -dst_x; copy_width -= src_x0; dst_x = 0; }
    if (dst_y < 0) { src_y0 = -dst_y; copy_height -= src_y0; dst_y = 0; }
    if (dst_x + copy_width > g_renderer.width) copy_width = g_renderer.width - dst_x;
    if (dst_y + copy_height > g_renderer.height) copy_height = g_renderer.height - dst_y;
    if (copy_width <= 0 || copy_height <= 0) {
        yaat_unload_bitmap(&mask_bitmap);
        return;
    }

    if (mode == YAAT_TRANSPARENCY_NONE) {
        for (y = 0; y < copy_height; ++y) {
            memcpy((unsigned char *)g_renderer.pixels + ((dst_y + y) * g_renderer.pitch) +
                       ((size_t)dst_x * sizeof(unsigned long)),
                   bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0,
                   (size_t)copy_width * sizeof(unsigned long));
        }
        yaat_unload_bitmap(&mask_bitmap);
        return;
    }

    for (y = 0; y < copy_height; ++y) {
        unsigned long *dst = (unsigned long *)((unsigned char *)g_renderer.pixels +
                             ((dst_y + y) * g_renderer.pitch)) + dst_x;
        unsigned long *src = bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0;

        for (x = 0; x < copy_width; ++x) {
            unsigned long pixel = src[x];
            int draw = 1;

            if (mode == YAAT_TRANSPARENCY_COLOR_KEY &&
                (pixel & 0x00ffffffUL) == color_key) {
                draw = 0;
            }
            if (mode == YAAT_TRANSPARENCY_MASK) {
                if (mask == 0 || src_x0 + x >= mask->width || src_y0 + y >= mask->height ||
                    ((mask->pixels[((src_y0 + y) * mask->width) + src_x0 + x] & 0x00ffffffUL) == 0)) {
                    draw = 0;
                }
            }
            if (draw) {
                if (mode == YAAT_TRANSPARENCY_ALPHA && bitmap->has_alpha) {
                    yaat_blend_pixel(&dst[x], pixel);
                } else {
                    dst[x] = pixel & 0x00ffffffUL;
                }
            }
        }
    }
    yaat_unload_bitmap(&mask_bitmap);
}

static void yaat_draw_bitmap(YaatBitmap *bitmap, int dst_x, int dst_y)
{
    yaat_draw_bitmap_keyed(bitmap, dst_x, dst_y, 0, 0);
}

static void yaat_draw_bitmap_region(YaatBitmap *bitmap, int dst_x, int dst_y,
                                    int src_x, int src_y, int width,
                                    int height)
{
    int copy_width;
    int copy_height;
    int y;

    if (bitmap == 0 || bitmap->pixels == 0) {
        return;
    }
    if (width <= 0 || height <= 0) {
        src_x = 0;
        src_y = 0;
        width = bitmap->width;
        height = bitmap->height;
    }
    if (src_x < 0) {
        dst_x -= src_x;
        width += src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        dst_y -= src_y;
        height += src_y;
        src_y = 0;
    }
    if (src_x + width > bitmap->width) width = bitmap->width - src_x;
    if (src_y + height > bitmap->height) height = bitmap->height - src_y;
    if (dst_x < 0) {
        src_x -= dst_x;
        width += dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y -= dst_y;
        height += dst_y;
        dst_y = 0;
    }
    copy_width = width;
    copy_height = height;
    if (dst_x + copy_width > g_renderer.width) {
        copy_width = g_renderer.width - dst_x;
    }
    if (dst_y + copy_height > g_renderer.height) {
        copy_height = g_renderer.height - dst_y;
    }
    if (copy_width <= 0 || copy_height <= 0) {
        return;
    }

    for (y = 0; y < copy_height; ++y) {
        memcpy((unsigned char *)g_renderer.pixels + ((dst_y + y) * g_renderer.pitch) +
                   ((size_t)dst_x * sizeof(unsigned long)),
               bitmap->pixels + ((src_y + y) * bitmap->width) + src_x,
               (size_t)copy_width * sizeof(unsigned long));
    }
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

static void yaat_clear_player_path(void)
{
    g_path_waypoint_count = 0;
    g_path_waypoint_index = 0;
}

static int yaat_room_has_walkmask(void)
{
    return g_runtime_load.ok && g_runtime_load.room.walkmask[0] != '\0' &&
           yaat_load_runtime_walkmask();
}

static int yaat_nav_cell_x_to_world(int cell_x)
{
    return yaat_clamp_int((cell_x * YAAT_NAV_CELL_SIZE) + (YAAT_NAV_CELL_SIZE / 2),
                          YAAT_PLAYER_WIDTH / 2,
                          YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
}

static int yaat_nav_cell_y_to_world(int cell_y)
{
    return yaat_clamp_int((cell_y * YAAT_NAV_CELL_SIZE) + (YAAT_NAV_CELL_SIZE / 2),
                          YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1);
}

static int yaat_nav_world_to_cell_x(int x)
{
    return yaat_clamp_int(x / YAAT_NAV_CELL_SIZE, 0, YAAT_NAV_GRID_WIDTH - 1);
}

static int yaat_nav_world_to_cell_y(int y)
{
    return yaat_clamp_int(y / YAAT_NAV_CELL_SIZE, 0, YAAT_NAV_GRID_HEIGHT - 1);
}

static int yaat_nav_cell_walkable(int cell_x, int cell_y)
{
    int x;
    int y;
    if (cell_x < 0 || cell_y < 0 ||
        cell_x >= YAAT_NAV_GRID_WIDTH || cell_y >= YAAT_NAV_GRID_HEIGHT) {
        return 0;
    }
    x = yaat_nav_cell_x_to_world(cell_x);
    y = yaat_nav_cell_y_to_world(cell_y);
    return yaat_is_walkable_at(x, y);
}

static int yaat_path_append_waypoint(int x, int y)
{
    if (g_path_waypoint_count >= YAAT_MAX_PATH_WAYPOINTS) return 0;
    g_path_waypoint_x[g_path_waypoint_count] = x;
    g_path_waypoint_y[g_path_waypoint_count] = y;
    ++g_path_waypoint_count;
    return 1;
}

static int yaat_build_player_route(int target_x, int target_y)
{
    static short came_from[YAAT_NAV_GRID_CELLS];
    static short queue[YAAT_NAV_GRID_CELLS];
    static short route[YAAT_NAV_GRID_CELLS];
    int start_x;
    int start_y;
    int goal_x;
    int goal_y;
    int start;
    int goal;
    int head;
    int tail;
    int found;
    int i;
    int route_count;

    yaat_clear_player_path();
    if (!yaat_room_has_walkmask()) return 0;

    start_x = yaat_nav_world_to_cell_x(g_player_x);
    start_y = yaat_nav_world_to_cell_y(g_player_y);
    goal_x = yaat_nav_world_to_cell_x(target_x);
    goal_y = yaat_nav_world_to_cell_y(target_y);
    if (!yaat_nav_cell_walkable(goal_x, goal_y)) return 0;

    start = (start_y * YAAT_NAV_GRID_WIDTH) + start_x;
    goal = (goal_y * YAAT_NAV_GRID_WIDTH) + goal_x;
    for (i = 0; i < YAAT_NAV_GRID_CELLS; ++i) came_from[i] = -1;
    head = 0;
    tail = 0;
    queue[tail++] = (short)start;
    came_from[start] = (short)start;
    found = start == goal;

    while (head < tail && !found) {
        static const int offsets[8][2] = {
            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
            { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 }
        };
        int current;
        int cx;
        int cy;
        current = queue[head++];
        cx = current % YAAT_NAV_GRID_WIDTH;
        cy = current / YAAT_NAV_GRID_WIDTH;
        for (i = 0; i < 8; ++i) {
            int nx = cx + offsets[i][0];
            int ny = cy + offsets[i][1];
            int next;
            if (!yaat_nav_cell_walkable(nx, ny)) continue;
            if (offsets[i][0] != 0 && offsets[i][1] != 0 &&
                (!yaat_nav_cell_walkable(cx + offsets[i][0], cy) ||
                 !yaat_nav_cell_walkable(cx, cy + offsets[i][1]))) {
                continue;
            }
            next = (ny * YAAT_NAV_GRID_WIDTH) + nx;
            if (came_from[next] != -1) continue;
            came_from[next] = (short)current;
            if (next == goal) {
                found = 1;
                break;
            }
            queue[tail++] = (short)next;
        }
    }
    if (!found) return 0;

    route_count = 0;
    i = goal;
    while (i != start && route_count < YAAT_NAV_GRID_CELLS) {
        route[route_count++] = (short)i;
        i = came_from[i];
    }

    for (i = route_count - 1; i >= 0; --i) {
        int cell = route[i];
        int cx = cell % YAAT_NAV_GRID_WIDTH;
        int cy = cell / YAAT_NAV_GRID_WIDTH;
        if (!yaat_path_append_waypoint(yaat_nav_cell_x_to_world(cx),
                                       yaat_nav_cell_y_to_world(cy))) {
            break;
        }
    }
    if (g_path_waypoint_count == 0 ||
        g_path_waypoint_x[g_path_waypoint_count - 1] != target_x ||
        g_path_waypoint_y[g_path_waypoint_count - 1] != target_y) {
        yaat_path_append_waypoint(target_x, target_y);
    }
    return g_path_waypoint_count > 0;
}

static void yaat_set_player_target(int x, int y)
{
    int has_walkmask;

    x = yaat_clamp_int(x, YAAT_PLAYER_WIDTH / 2,
                       YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    y = yaat_clamp_int(y, YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1);
    if (!yaat_is_walkable_at(x, y)) {
        return;
    }
    if (x < g_player_x) {
        g_player_facing_right = 0;
    } else if (x > g_player_x) {
        g_player_facing_right = 1;
    }

    has_walkmask = yaat_room_has_walkmask();
    if (has_walkmask) {
        if (!yaat_build_player_route(x, y)) {
            return;
        }
        g_target_x = g_path_waypoint_x[g_path_waypoint_index];
        g_target_y = g_path_waypoint_y[g_path_waypoint_index];
    } else {
        g_target_x = x;
        g_target_y = y;
        yaat_clear_player_path();
    }
}

static int yaat_player_motion_complete(void)
{
    return g_player_x == g_target_x && g_player_y == g_target_y &&
           g_path_waypoint_index >= g_path_waypoint_count;
}

static void yaat_advance_player_path(void)
{
    while (g_path_waypoint_index < g_path_waypoint_count &&
           g_player_x == g_target_x && g_player_y == g_target_y) {
        ++g_path_waypoint_index;
        if (g_path_waypoint_index < g_path_waypoint_count) {
            g_target_x = g_path_waypoint_x[g_path_waypoint_index];
            g_target_y = g_path_waypoint_y[g_path_waypoint_index];
        } else {
            yaat_clear_player_path();
        }
    }
}

static int yaat_find_walk_target_for_rect(int rect_x, int rect_y, int rect_width,
                                          int rect_height, int *target_x,
                                          int *target_y)
{
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int center_x;
    int y;
    int offset;

    if (target_x == 0 || target_y == 0 || rect_width <= 0 || rect_height <= 0) {
        return 0;
    }

    min_x = yaat_clamp_int(rect_x, YAAT_PLAYER_WIDTH / 2,
                           YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    max_x = yaat_clamp_int(rect_x + rect_width - 1,
                           YAAT_PLAYER_WIDTH / 2,
                           YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    min_y = yaat_clamp_int(rect_y, YAAT_PLAYER_HEIGHT,
                           YAAT_PLAYFIELD_HEIGHT - 1);
    max_y = yaat_clamp_int(rect_y + rect_height - 1,
                           YAAT_PLAYER_HEIGHT,
                           YAAT_PLAYFIELD_HEIGHT - 1);
    if (min_x > max_x || min_y > max_y) {
        return 0;
    }

    center_x = rect_x + (rect_width / 2);
    center_x = yaat_clamp_int(center_x, min_x, max_x);
    for (y = max_y; y >= min_y; --y) {
        if (yaat_is_walkable_at(center_x, y)) {
            *target_x = center_x;
            *target_y = y;
            return 1;
        }
        for (offset = 1; center_x - offset >= min_x || center_x + offset <= max_x;
             ++offset) {
            if (center_x - offset >= min_x &&
                yaat_is_walkable_at(center_x - offset, y)) {
                *target_x = center_x - offset;
                *target_y = y;
                return 1;
            }
            if (center_x + offset <= max_x &&
                yaat_is_walkable_at(center_x + offset, y)) {
                *target_x = center_x + offset;
                *target_y = y;
                return 1;
            }
        }
    }
    return 0;
}

static int yaat_find_walk_target_for_hotspot(const YaatRuntimeHotspot *hotspot,
                                             int *target_x, int *target_y)
{
    int anchor_x;
    int anchor_y;

    if (hotspot == 0) {
        return 0;
    }
    if (hotspot->has_walk_x && hotspot->has_walk_y) {
        anchor_x = yaat_clamp_int(hotspot->walk_x, YAAT_PLAYER_WIDTH / 2,
                                  YAAT_BACKBUFFER_WIDTH -
                                      (YAAT_PLAYER_WIDTH / 2));
        anchor_y = yaat_clamp_int(hotspot->walk_y, YAAT_PLAYER_HEIGHT,
                                  YAAT_PLAYFIELD_HEIGHT - 1);
        if (yaat_is_walkable_at(anchor_x, anchor_y)) {
            *target_x = anchor_x;
            *target_y = anchor_y;
            return 1;
        }
    }
    return yaat_find_walk_target_for_rect(hotspot->x, hotspot->y,
                                          hotspot->width, hotspot->height,
                                          target_x, target_y);
}

static int yaat_find_walk_target_for_object(const YaatRuntimeObject *object,
                                            int *target_x, int *target_y)
{
    int anchor_x;
    int anchor_y;

    if (object == 0) {
        return 0;
    }
    if (object->has_walk_x && object->has_walk_y) {
        anchor_x = yaat_clamp_int(object->walk_x, YAAT_PLAYER_WIDTH / 2,
                                  YAAT_BACKBUFFER_WIDTH -
                                      (YAAT_PLAYER_WIDTH / 2));
        anchor_y = yaat_clamp_int(object->walk_y, YAAT_PLAYER_HEIGHT,
                                  YAAT_PLAYFIELD_HEIGHT - 1);
        if (yaat_is_walkable_at(anchor_x, anchor_y)) {
            *target_x = anchor_x;
            *target_y = anchor_y;
            return 1;
        }
    }
    return yaat_find_walk_target_for_rect(object->x, object->y,
                                          object->width, object->height,
                                          target_x, target_y);
}

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

    if (g_player_x != g_target_x || g_player_y != g_target_y) {
        if (g_target_x < g_player_x) {
            animation_id = "walk_left";
        } else if (g_target_x > g_player_x) {
            animation_id = "walk_right";
        } else {
            animation_id = g_player_facing_right ? "walk_right" : "walk_left";
        }
    } else {
        animation_id = g_player_facing_right ? "idle_right" : "idle_left";
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
    draw_x = g_player_x - (frame_width / 2) + g_shake_offset_x;
    draw_y = g_player_y - frame_height + g_shake_offset_y;

    transparency.mode = YAAT_TRANSPARENCY_ALPHA;
    transparency.color_key = 0x00ff00ffUL;
    transparency.mask[0] = '\0';
    yaat_draw_bitmap_transparent(&g_player_bitmap, draw_x, draw_y,
                                 &transparency, "graphics/sprites");
    if (g_player_transparent_color_enabled) {
        yaat_draw_bitmap_keyed(&g_player_bitmap, draw_x, draw_y,
                               g_player_transparent_color_enabled,
                               g_player_transparent_color);
    }
    if (frame->x != 0 || frame->y != 0 ||
        frame_width != g_player_bitmap.width ||
        frame_height != g_player_bitmap.height) {
        yaat_draw_bitmap_region(&g_player_bitmap, draw_x, draw_y,
                                frame->x, frame->y, frame_width, frame_height);
    }
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

static void yaat_draw_runtime_room(void)
{
    int i;
    int floor_y;
    int background_drawn;
    unsigned long background_color;
    int dialogue_x;
    int dialogue_y;

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

    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object;
        unsigned long object_color;
        YaatBitmap object_bitmap;
        char object_path[YAAT_ASSET_MAX_PATH * 2];
        const char *object_sprite;

        object = &g_runtime_load.room.objects[i];
        if (!object->visible || object->width <= 0 || object->height <= 0) {
            continue;
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
            continue;
        }
        object_color = yaat_hash_color(object_sprite, 0x002f5f9eUL);
        yaat_draw_rect(&g_renderer, object->x + g_shake_offset_x,
                       object->y + g_shake_offset_y,
                       object->width, object->height, 0x00202020UL);
        yaat_draw_rect(&g_renderer, object->x + 1 + g_shake_offset_x,
                       object->y + 1 + g_shake_offset_y,
                       object->width - 2, object->height - 2, object_color);
    }

    if (g_player_visible) yaat_draw_player();
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
    int start_x = 164;
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

static int yaat_save_script_state(const char *path)
{
    FILE *file;
    int i;
    int j;

    yaat_capture_runtime_object_state();
    file = fopen(path, "w");
    if (file == 0) return 0;

    fprintf(file, "YAAT_SCRIPT_STATE %d\n", YAAT_SAVE_STATE_VERSION);
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
        if (strcmp(key, "player") == 0) {
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

static int yaat_dialogue_position_for_speaker(int *dialogue_x, int *dialogue_y)
{
    int speaker_x;
    int speaker_y;

    if (!g_dialogue_visible || dialogue_x == 0 || dialogue_y == 0) {
        return 0;
    }

    if (strcmp(g_dialogue_speaker, "player") == 0) {
        if (!g_player_visible) {
            return 0;
        }
        speaker_x = g_player_x;
        speaker_y = g_player_y - YAAT_PLAYER_HEIGHT;
    } else {
        YaatRuntimeObject *object;
        YaatEntity *entity;

        object = yaat_runtime_object_by_id(g_dialogue_speaker);
        if (object != 0 && object->visible) {
            speaker_x = object->x + (object->width / 2);
            speaker_y = object->y;
        } else if (!g_runtime_load.ok && g_current_room >= 0 && g_current_room < g_room_count) {
            entity = yaat_entity_by_id(&g_rooms[g_current_room], g_dialogue_speaker);
            if (entity == 0 || !entity->visible) {
                return 0;
            }
            speaker_x = entity->x + (entity->w / 2);
            speaker_y = entity->y;
        } else {
            return 0;
        }
    }

    *dialogue_x = yaat_clamp_int(speaker_x - 60, 0, YAAT_BACKBUFFER_WIDTH - 120);
    *dialogue_y = yaat_clamp_int(speaker_y - 16, 0, YAAT_PLAYFIELD_HEIGHT - 16);
    return 1;
}

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
}

static const char *yaat_active_verb(void)
{
    return g_selected_verb[0] != '\0' ? g_selected_verb : "walk";
}

static void yaat_player_say(const char *text)
{
    yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), "player", strlen("player"));
    yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), text, strlen(text));
    g_dialogue_visible = 1;
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


static void yaat_dialogue_hide_choices(void)
{
    g_dialogue_choice_visible = 0;
    g_active_dialog_node[0] = '\0';
}

static void yaat_dialogue_show_node(const char *node_id)
{
    YaatRuntimeDialogNode *node;
    if (node_id == 0 || strcmp(node_id, "end") == 0) {
        yaat_dialogue_hide_choices();
        return;
    }
    node = yaat_runtime_dialog_find_node(&g_active_dialog, node_id);
    if (node == 0) {
        yaat_dialogue_hide_choices();
        return;
    }
    yaat_copy(g_active_dialog_node, sizeof(g_active_dialog_node), node->id, strlen(node->id));
    if (node->speaker[0] != '\0') yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), node->speaker, strlen(node->speaker));
    else yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), "player", strlen("player"));
    yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), node->text, strlen(node->text));
    g_dialogue_visible = 1;
    g_dialogue_choice_visible = node->choice_count > 0;
    if (!g_dialogue_choice_visible && node->next[0] != '\0') yaat_dialogue_show_node(node->next);
}

static void yaat_start_dialogue(const char *dialog_id)
{
    if (yaat_runtime_load_dialog_from_store(&g_asset_store, dialog_id, &g_active_dialog) ||
        yaat_runtime_load_dialog_from_store(&g_runtime_asset_store, dialog_id, &g_active_dialog)) {
        yaat_dialogue_show_node("start");
    }
}

static void yaat_select_dialogue_choice(const char *choice_id)
{
    YaatRuntimeDialogNode *choice;
    YaatEvent *event;
    choice = yaat_runtime_dialog_find_choice(&g_active_dialog, choice_id);
    if (choice == 0) return;
    if (choice->reply[0] != '\0') {
        YaatRuntimeDialogNode *node;
        node = yaat_runtime_dialog_find_node(&g_active_dialog, g_active_dialog_node);
        if (node != 0 && node->speaker[0] != '\0') yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), node->speaker, strlen(node->speaker));
        else yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), "player", strlen("player"));
        yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), choice->reply, strlen(choice->reply));
        g_dialogue_visible = 1;
    }
    event = yaat_find_event(g_global_events, g_global_event_count,
                            choice->event[0] != '\0' ? choice->event : "dialog_choice",
                            choice->id);
    if (event != 0) yaat_execute_event(event);
    if (choice->next[0] != '\0') yaat_dialogue_show_node(choice->next);
    else yaat_dialogue_hide_choices();
}

static int yaat_dialogue_choice_at(int x, int y)
{
    YaatRuntimeDialogNode *node;
    int i;
    if (!g_dialogue_choice_visible) return -1;
    node = yaat_runtime_dialog_find_node(&g_active_dialog, g_active_dialog_node);
    if (node == 0) return -1;
    for (i = 0; i < node->choice_count; ++i) {
        int top;
        top = YAAT_PLAYFIELD_HEIGHT + 22 + (i * 12);
        if (x >= 8 && x < YAAT_BACKBUFFER_WIDTH - 8 && y >= top && y < top + 10) return i;
    }
    return -1;
}

static int yaat_handle_dialogue_click(HWND window, int client_x, int client_y)
{
    int x, y, choice_index;
    YaatRuntimeDialogNode *node;
    if (!yaat_client_to_backbuffer(window, client_x, client_y, &x, &y)) return 0;
    choice_index = yaat_dialogue_choice_at(x, y);
    if (choice_index < 0) return 0;
    node = yaat_runtime_dialog_find_node(&g_active_dialog, g_active_dialog_node);
    if (node == 0) return 0;
    yaat_select_dialogue_choice(node->choice_ids[choice_index]);
    return 1;
}

static void yaat_draw_dialogue_choices(void)
{
    YaatRuntimeDialogNode *node;
    int i;
    if (!g_dialogue_choice_visible) return;
    node = yaat_runtime_dialog_find_node(&g_active_dialog, g_active_dialog_node);
    if (node == 0) return;
    for (i = 0; i < node->choice_count; ++i) {
        YaatRuntimeDialogNode *choice;
        const char *text;
        choice = yaat_runtime_dialog_find_choice(&g_active_dialog, node->choice_ids[i]);
        text = choice != 0 && choice->text[0] != '\0' ? choice->text : node->choice_ids[i];
        yaat_draw_text_block(12, YAAT_PLAYFIELD_HEIGHT + 22 + (i * 12), text, 0x00a0d8ffUL);
    }
}

static void yaat_execute_commands(int first, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        YaatCommand *cmd = &g_commands[first + i];
        if (cmd->kind == YAAT_CMD_SAY) {
            yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), cmd->a, strlen(cmd->a));
            yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), cmd->b, strlen(cmd->b));
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
        } else if (cmd->kind == YAAT_CMD_DROP || cmd->kind == YAAT_CMD_REMOVE_INVENTORY || cmd->kind == YAAT_CMD_CONSUME) {
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
            yaat_copy(g_cutscene_overlay_text, sizeof(g_cutscene_overlay_text), cmd->a, strlen(cmd->a));
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
    g_player_x = YAAT_BACKBUFFER_WIDTH / 2;
    g_player_y = YAAT_PLAYFIELD_HEIGHT - 20;
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


static void yaat_render_scene(void)
{
    if (g_runtime_load.ok) {
        yaat_draw_runtime_room();
    } else if (g_room_count > 0) {
        yaat_draw_script_scene();
    } else {
        yaat_draw_error_scene();
    }
    yaat_draw_verb_ui();
    if (g_dialogue_visible) {
        yaat_draw_text_block(8, YAAT_PLAYFIELD_HEIGHT + 25, g_dialogue_speaker, 0x00ffd060UL);
        yaat_draw_text_block(70, YAAT_PLAYFIELD_HEIGHT + 25, g_dialogue_text, 0x00f0f0f0UL);
    }
    yaat_draw_dialogue_choices();
    if (g_cutscene_overlay_visible) {
        yaat_gdi_renderer_clear(&g_renderer, 0x00000000UL);
        yaat_draw_text_block(64, (YAAT_BACKBUFFER_HEIGHT / 2) - 8, g_cutscene_overlay_text, 0x00a0c8ffUL);
    }
    yaat_draw_cursor_placeholder();
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
    if (dx > YAAT_PLAYER_SPEED_PIXELS) dx = YAAT_PLAYER_SPEED_PIXELS; else if (dx < -YAAT_PLAYER_SPEED_PIXELS) dx = -YAAT_PLAYER_SPEED_PIXELS;
    if (dy > YAAT_PLAYER_SPEED_PIXELS) dy = YAAT_PLAYER_SPEED_PIXELS; else if (dy < -YAAT_PLAYER_SPEED_PIXELS) dy = -YAAT_PLAYER_SPEED_PIXELS;
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

    moving = moved;
    if (moving) {
        if (dx < 0) {
            yaat_set_player_animation("walk_left");
        } else if (dx > 0) {
            yaat_set_player_animation("walk_right");
        } else {
            yaat_set_player_animation(g_player_facing_right ?
                                      "walk_right" : "walk_left");
        }
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
        yaat_set_player_animation(g_player_facing_right ?
                                  "idle_right" : "idle_left");
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

static void yaat_runtime_execute_entity_event(const char *entity_id, const char *script_event)
{
    char event_name[32];
    YaatRoom *room;
    YaatEntity *entity;
    YaatEvent *event;

    if (g_room_count <= 0) return;
    room = &g_rooms[yaat_runtime_room_script_index()];
    entity = yaat_entity_by_id(room, entity_id);
    if (entity == 0) return;

    (void)script_event;
    yaat_copy(event_name, sizeof(event_name), yaat_active_verb(), strlen(yaat_active_verb()));
    if (strcmp(event_name, "use") == 0 && g_selected_inventory[0] != '\0') {
        event = yaat_find_event(entity->events, entity->event_count, event_name, g_selected_inventory);
    } else {
        event = yaat_find_event(entity->events, entity->event_count, event_name, 0);
    }
    if (event == 0 && strcmp(event_name, "walk") == 0) {
        event = yaat_find_event(entity->events, entity->event_count, "click", 0);
    }
    if (event != 0) {
        yaat_execute_event(event);
    } else {
        yaat_default_action_sentence(event_name);
        yaat_deselect_action();
    }
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
    if (g_player_x >= hotspot->x && g_player_y >= hotspot->y &&
        g_player_x < hotspot->x + hotspot->width &&
        g_player_y < hotspot->y + hotspot->height) {
        yaat_runtime_change_room(hotspot);
    } else {
        yaat_pending_room_change_clear();
    }
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

static void yaat_click_inventory_item(const char *item)
{
    YaatEntity *entity;
    YaatEvent *event;
    if (item == 0 || item[0] == '\0') return;
    yaat_pending_interaction_clear();
    if (strcmp(g_selected_verb, "use") == 0) {
        yaat_copy(g_selected_inventory, sizeof(g_selected_inventory), item, strlen(item));
        return;
    }
    entity = yaat_entity_by_id_any_room(item);
    if (entity == 0) return;
    event = yaat_find_event(entity->events, entity->event_count, g_selected_verb, 0);
    if (event == 0 && strcmp(g_selected_verb, "click") != 0) {
        event = yaat_find_event(entity->events, entity->event_count, "click", 0);
    }
    if (event != 0) {
        yaat_execute_event(event);
    } else {
        yaat_default_action_sentence(g_selected_verb);
        yaat_deselect_action();
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
            if (!event && strcmp(yaat_active_verb(), "walk") == 0) event = yaat_find_event(e->events, e->event_count, "click", 0);
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
    verb_index = yaat_verb_button_at(backbuffer_x, backbuffer_y);
    if (verb_index >= 0) {
        yaat_pending_interaction_clear();
        yaat_pending_room_change_clear();
        if (strcmp(g_selected_verb, g_verbs[verb_index]) == 0) {
            yaat_deselect_action();
        } else {
            yaat_copy(g_selected_verb, sizeof(g_selected_verb), g_verbs[verb_index], strlen(g_verbs[verb_index]));
            if (strcmp(g_selected_verb, "use") != 0) g_selected_inventory[0] = '\0';
        }
        return;
    }
    inventory_index = yaat_inventory_slot_at(backbuffer_x, backbuffer_y);
    if (inventory_index >= 0) {
        yaat_pending_room_change_clear();
        yaat_click_inventory_item(g_inventory[inventory_index]);
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
        } else {
            yaat_pending_interaction_clear();
            yaat_pending_room_change_clear();
        }
        return;
    }

    if (strcmp(g_selected_verb, "use") == 0) {
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
                        yaat_pending_interaction_set(backbuffer_x,
                                                     backbuffer_y);
                        yaat_pending_room_change_clear();
                        yaat_set_player_target(walk_target_x, walk_target_y);
                        return;
                    }
                    break;
                }
            }
            if (hotspot != 0) {
                walk_target_x = backbuffer_x;
                walk_target_y = yaat_clamp_int(backbuffer_y,
                                               YAAT_PLAYER_HEIGHT,
                                               YAAT_PLAYFIELD_HEIGHT - 1);
                if (yaat_find_walk_target_for_hotspot(hotspot, &walk_target_x,
                                                      &walk_target_y)) {
                    yaat_pending_interaction_set(backbuffer_x, backbuffer_y);
                    yaat_pending_room_change_clear();
                    yaat_set_player_target(walk_target_x, walk_target_y);
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
                        return;
                    }
                    break;
                }
            }
        }
    }

    yaat_pending_interaction_clear();
    yaat_pending_room_change_clear();
    if (g_selected_verb[0] != '\0') {
        yaat_default_action_sentence(g_selected_verb);
        yaat_deselect_action();
        return;
    }
    if (!yaat_is_walkable_at(backbuffer_x, yaat_clamp_int(backbuffer_y, YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1))) {
        yaat_default_action_sentence("walk");
        return;
    }
    if (backbuffer_x < g_player_x) {
        g_player_facing_right = 0;
    } else if (backbuffer_x > g_player_x) {
        g_player_facing_right = 1;
    }
    yaat_set_player_target(backbuffer_x, backbuffer_y);
    yaat_click_game(backbuffer_x, backbuffer_y, 0);
}

static void yaat_update_cursor_from_client(HWND window, int client_x, int client_y)
{
    int backbuffer_x;
    int backbuffer_y;
    YaatRuntimeHotspot *hotspot;
    const char *cursor_state;
    LPCSTR win32_cursor;

    if (!yaat_client_to_backbuffer(window, client_x, client_y,
                                   &backbuffer_x, &backbuffer_y)) return;
    g_cursor_x = backbuffer_x;
    g_cursor_y = backbuffer_y;

    hotspot = g_runtime_load.ok ? yaat_runtime_hotspot_at(backbuffer_x, backbuffer_y) : 0;
    cursor_state = hotspot != 0 ? hotspot->cursor : "arrow";
    yaat_copy(g_cursor_state, sizeof(g_cursor_state), cursor_state,
              strlen(cursor_state));

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
        if (w_param == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) {
            if ((l_param & 0x40000000L) != 0) return 0;
            yaat_toggle_fullscreen(window);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && w_param == 'S') {
            yaat_save_script_state(YAAT_SAVE_PATH);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && w_param == 'L') {
            yaat_load_script_state(YAAT_SAVE_PATH);
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
    case WM_LBUTTONDOWN:
        if (g_dialogue_choice_visible && yaat_handle_dialogue_click(window, (int)(short)LOWORD(l_param), (int)(short)HIWORD(l_param))) { }
        else if (g_dialogue_visible) { g_dialogue_visible = 0; yaat_dialogue_hide_choices(); }
        else yaat_set_target_from_client(window, (int)(short)LOWORD(l_param),
                                         (int)(short)HIWORD(l_param), 0);
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_LBUTTONDBLCLK:
        if (g_dialogue_choice_visible && yaat_handle_dialogue_click(window, (int)(short)LOWORD(l_param), (int)(short)HIWORD(l_param))) { }
        else if (g_dialogue_visible) { g_dialogue_visible = 0; yaat_dialogue_hide_choices(); }
        else yaat_set_target_from_client(window, (int)(short)LOWORD(l_param),
                                         (int)(short)HIWORD(l_param), 1);
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_TIMER:
        if (w_param == YAAT_FRAME_TIMER_ID) {
            g_animation_clock_ms += YAAT_FRAME_TIMER_MS;
            yaat_update_player();
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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    WNDCLASSEXA window_class;
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    yaat_asset_store_init_loose(&g_asset_store, "game");
    yaat_asset_store_init_loose(&g_runtime_asset_store, "game");
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
