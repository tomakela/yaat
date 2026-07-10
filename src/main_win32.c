#include <windows.h>

#include "platform/win32/gdi_renderer.h"
#include "script_tokenizer.h"

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
#define YAAT_MAX_ROOMS 8
#define YAAT_MAX_ENTITIES 32
#define YAAT_MAX_EVENTS 8
#define YAAT_MAX_COMMANDS 64
#define YAAT_MAX_VARS 64
#define YAAT_MAX_INVENTORY 16
#define YAAT_TEXT_MAX 160
#define YAAT_MAX_RUNTIME_HOTSPOTS 32

typedef struct YaatViewport {
    int x;
    int y;
    int width;
    int height;
} YaatViewport;

typedef enum YaatEntityKind { YAAT_ENTITY_HOTSPOT, YAAT_ENTITY_OBJECT } YaatEntityKind;
typedef enum YaatCommandKind { YAAT_CMD_SAY, YAAT_CMD_SET, YAAT_CMD_GOTO, YAAT_CMD_PLAY_SOUND, YAAT_CMD_TAKE, YAAT_CMD_HIDE, YAAT_CMD_IF } YaatCommandKind;

typedef struct YaatCommand YaatCommand;

typedef struct YaatEvent {
    char name[32];
    char item[32];
    int first_command;
    int command_count;
} YaatEvent;

typedef struct YaatEntity {
    YaatEntityKind kind;
    char id[32];
    char name[64];
    int x;
    int y;
    int w;
    int h;
    int visible;
    YaatEvent events[YAAT_MAX_EVENTS];
    int event_count;
} YaatEntity;

typedef struct YaatRoom {
    char id[32];
    char label[64];
    unsigned long color;
    YaatEntity entities[YAAT_MAX_ENTITIES];
    int entity_count;
    YaatEvent events[YAAT_MAX_EVENTS];
    int event_count;
} YaatRoom;

struct YaatCommand {
    YaatCommandKind kind;
    char a[96];
    char b[96];
    int bool_value;
    int first_child;
    int child_count;
    int first_else_child;
    int else_child_count;
};

typedef struct YaatVar {
    char name[32];
    int bool_value;
} YaatVar;

typedef struct YaatScriptCursor {
    ScriptToken *tokens;
    size_t count;
    size_t index;
} YaatScriptCursor;

static YaatGdiRenderer g_renderer;
static int g_renderer_ready;
static int g_player_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_player_y = YAAT_PLAYFIELD_HEIGHT / 2;
static int g_target_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_target_y = YAAT_PLAYFIELD_HEIGHT / 2;
static int g_player_facing_right = 1;
static int g_cursor_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_cursor_y = YAAT_PLAYFIELD_HEIGHT / 2;
static YaatRoom g_rooms[YAAT_MAX_ROOMS];
static int g_room_count;
static int g_current_room;
static YaatCommand g_commands[YAAT_MAX_COMMANDS];
static int g_command_count;
static YaatVar g_vars[YAAT_MAX_VARS];
static int g_var_count;
static char g_inventory[YAAT_MAX_INVENTORY][32];
static int g_inventory_count;
static char g_dialogue_speaker[32];
static char g_dialogue_text[YAAT_TEXT_MAX];
static int g_dialogue_visible;
static YaatRuntimeLoadResult g_runtime_load;
static YaatAssetStore g_asset_store;
static YaatRuntimeHotspot g_runtime_hotspots[YAAT_MAX_RUNTIME_HOTSPOTS];
static int g_runtime_hotspot_count;
static char g_cursor_state[32] = "arrow";
static int g_fullscreen;
static RECT g_windowed_rect;
static DWORD g_windowed_style;

typedef struct YaatBitmap {
    unsigned long *pixels;
    int width;
    int height;
    char path[YAAT_ASSET_MAX_PATH * 2];
} YaatBitmap;

static YaatBitmap g_background_bitmap;
static YaatBitmap g_player_bitmap;

static void yaat_runtime_join_path(char *dst, size_t dst_size,
                                   const char *left, const char *right);
static const char *yaat_runtime_logical_path(const char *path);

static int yaat_clamp_int(int value, int minimum, int maximum)
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

static int yaat_token_is(ScriptToken *token, const char *text)
{
    return token->length == strlen(text) && memcmp(token->lexeme, text, token->length) == 0;
}

static ScriptToken *yaat_peek(YaatScriptCursor *cursor)
{
    return &cursor->tokens[cursor->index];
}

static ScriptToken *yaat_advance_token(YaatScriptCursor *cursor)
{
    if (cursor->index + 1 < cursor->count) cursor->index++;
    return &cursor->tokens[cursor->index - 1];
}

static int yaat_match_token(YaatScriptCursor *cursor, ScriptTokenType type)
{
    if (yaat_peek(cursor)->type != type) return 0;
    yaat_advance_token(cursor);
    return 1;
}

static void yaat_skip_block(YaatScriptCursor *cursor)
{
    int depth = 1;
    while (depth > 0 && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        if (yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) depth++;
        else if (yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE)) depth--;
        else yaat_advance_token(cursor);
    }
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

    if (text == 0) return;
    for (i = 0; text[i] != '\0' && cy < YAAT_BACKBUFFER_HEIGHT - 7; ++i) {
        if (text[i] == '\n' || cx > YAAT_BACKBUFFER_WIDTH - 8) {
            cx = x;
            cy += 8;
            if (text[i] == '\n') continue;
        }
        if (text[i] != ' ') yaat_draw_rect(&g_renderer, cx, cy, 5, 7, color);
        cx += 6;
    }
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
                pixels[(dst_y * (int)bmp_width) + x] = b | (g << 8) | (r << 16);
            }
        }
    }

    free(data);
    yaat_unload_bitmap(bitmap);
    bitmap->pixels = pixels;
    bitmap->width = (int)bmp_width;
    bitmap->height = (int)bmp_height;
    yaat_copy(bitmap->path, sizeof(bitmap->path), path, strlen(path));
    return 1;
}

static void yaat_draw_bitmap(YaatBitmap *bitmap, int dst_x, int dst_y)
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
    if (dst_x < 0) {
        src_x0 = -dst_x;
        copy_width -= src_x0;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y0 = -dst_y;
        copy_height -= src_y0;
        dst_y = 0;
    }
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
               bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0,
               (size_t)copy_width * sizeof(unsigned long));
    }
}

static int yaat_draw_runtime_background(void)
{
    char path[YAAT_ASSET_MAX_PATH * 2];
    int copy_width;
    int copy_height;
    int y;

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

    copy_width = g_background_bitmap.width;
    copy_height = g_background_bitmap.height;
    if (copy_width > g_renderer.width) {
        copy_width = g_renderer.width;
    }
    if (copy_height > g_renderer.height) {
        copy_height = g_renderer.height;
    }

    for (y = 0; y < copy_height; ++y) {
        memcpy((unsigned char *)g_renderer.pixels + (y * g_renderer.pitch),
               g_background_bitmap.pixels + (y * g_background_bitmap.width),
               (size_t)copy_width * sizeof(unsigned long));
    }

    return 1;
}

static void yaat_draw_player_placeholder(void)
{
    int shadow_x;
    int shadow_y;
    int body_x;
    int body_y;

    shadow_x = g_player_x - (YAAT_PLAYER_WIDTH / 2) - 2;
    shadow_y = g_player_y + 7;
    body_x = g_player_x - (YAAT_PLAYER_WIDTH / 2);
    body_y = g_player_y - YAAT_PLAYER_HEIGHT;

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

static void yaat_draw_player(void)
{
    char path[YAAT_ASSET_MAX_PATH * 2];
    const char *sprite_name;
    int draw_x;
    int draw_y;

    if (g_player_x != g_target_x || g_player_y != g_target_y) {
        if (g_target_x < g_player_x) {
            sprite_name = "player_walk_left.bmp";
        } else if (g_target_x > g_player_x) {
            sprite_name = "player_walk_right.bmp";
        } else {
            sprite_name = g_player_facing_right ?
                          "player_walk_right.bmp" : "player_walk_left.bmp";
        }
    } else {
        sprite_name = "player_idle.bmp";
    }
    yaat_runtime_join_path(path, sizeof(path), "graphics/sprites",
                           sprite_name);
    if (!yaat_load_bmp(&g_player_bitmap, path)) {
        yaat_draw_player_placeholder();
        return;
    }

    draw_x = g_player_x - (g_player_bitmap.width / 2);
    draw_y = g_player_y - g_player_bitmap.height;
    yaat_draw_bitmap(&g_player_bitmap, draw_x, draw_y);
}

static void yaat_draw_runtime_room(void)
{
    int i;
    int floor_y;
    int background_drawn;
    unsigned long background_color;

    background_color = yaat_hash_color(g_runtime_load.room.background,
                                        0x00d8c7a3UL);
    yaat_gdi_renderer_clear(&g_renderer, background_color);
    background_drawn = yaat_draw_runtime_background();

    if (!background_drawn) {
        floor_y = YAAT_BACKBUFFER_HEIGHT - 44;
        if (g_runtime_load.room.height > 0) {
            floor_y = (YAAT_BACKBUFFER_HEIGHT * 3) / 4;
        }
        yaat_draw_rect(&g_renderer, 0, floor_y, YAAT_BACKBUFFER_WIDTH,
                       YAAT_BACKBUFFER_HEIGHT - floor_y, 0x005f6f4aUL);

        yaat_draw_rect(&g_renderer, 12, 12, 128, 22, 0x00282828UL);
        yaat_draw_rect(&g_renderer, 14, 14, 124, 18, 0x00d8d0b8UL);
    }

    for (i = 0; i < g_runtime_load.room.hotspot_count; ++i) {
        YaatRuntimeHotspot *hotspot;
        unsigned long hotspot_color;

        hotspot = &g_runtime_load.room.hotspots[i];
        if (hotspot->width <= 0 || hotspot->height <= 0) {
            continue;
        }
        hotspot_color = yaat_hash_color(hotspot->cursor, 0x00c08020UL);
        yaat_draw_rect(&g_renderer, hotspot->x, hotspot->y,
                       hotspot->width, hotspot->height, 0x00f0d020UL);
        yaat_draw_rect(&g_renderer, hotspot->x + 1, hotspot->y + 1,
                       hotspot->width - 2, hotspot->height - 2,
                       hotspot_color);
    }

    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object;
        unsigned long object_color;
        YaatBitmap object_bitmap;
        char object_path[YAAT_ASSET_MAX_PATH * 2];

        object = &g_runtime_load.room.objects[i];
        if (!object->visible || object->width <= 0 || object->height <= 0) {
            continue;
        }
        memset(&object_bitmap, 0, sizeof(object_bitmap));
        yaat_runtime_join_path(object_path, sizeof(object_path),
                               yaat_runtime_logical_path(g_runtime_load.room.room_path),
                               object->sprite);
        if (yaat_load_bmp(&object_bitmap, object_path)) {
            yaat_draw_bitmap(&object_bitmap, object->x, object->y);
            yaat_unload_bitmap(&object_bitmap);
            continue;
        }
        object_color = yaat_hash_color(object->sprite, 0x002f5f9eUL);
        yaat_draw_rect(&g_renderer, object->x, object->y,
                       object->width, object->height, 0x00202020UL);
        yaat_draw_rect(&g_renderer, object->x + 1, object->y + 1,
                       object->width - 2, object->height - 2, object_color);
    }

    yaat_draw_player();
}


static void yaat_draw_cursor_placeholder(void)
{
    unsigned long outline_color;
    unsigned long fill_color;

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

    for (i = g_runtime_hotspot_count - 1; i >= 0; --i) {
        YaatRuntimeHotspot *hotspot = &g_runtime_hotspots[i];
        if (hotspot->width > 0 && hotspot->height > 0 && x >= hotspot->x &&
            y >= hotspot->y && x < hotspot->x + hotspot->width &&
            y < hotspot->y + hotspot->height) {
            return hotspot;
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

static int yaat_get_var(const char *name)
{
    int idx = yaat_find_var(name);
    if (idx < 0) return 0;
    return g_vars[idx].bool_value;
}
static void yaat_set_var(const char *name, int value)
{
    int idx = yaat_find_var(name);
    if (idx < 0 && g_var_count < YAAT_MAX_VARS) {
        idx = g_var_count++;
        yaat_copy(g_vars[idx].name, sizeof(g_vars[idx].name), name, strlen(name));
    }
    if (idx >= 0) g_vars[idx].bool_value = value;
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

static int yaat_room_index_by_id(const char *id)
{
    int i;
    for (i = 0; i < g_room_count; ++i) if (strcmp(g_rooms[i].id, id) == 0) return i;
    return -1;
}

static YaatEntity *yaat_entity_by_id(YaatRoom *room, const char *id)
{
    int i;
    for (i = 0; room && i < room->entity_count; ++i) if (strcmp(room->entities[i].id, id) == 0) return &room->entities[i];
    return 0;
}

static YaatEvent *yaat_find_event(YaatEvent *events, int count, const char *name, const char *item)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (strcmp(events[i].name, name) == 0 && (item == 0 || events[i].item[0] == '\0' || strcmp(events[i].item, item) == 0)) return &events[i];
    }
    return 0;
}

static void yaat_enter_room(int room_index);

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
            yaat_set_var(cmd->a, cmd->bool_value);
        } else if (cmd->kind == YAAT_CMD_GOTO) {
            int idx = yaat_room_index_by_id(cmd->a);
            if (idx >= 0) yaat_enter_room(idx);
        } else if (cmd->kind == YAAT_CMD_PLAY_SOUND) {
            MessageBeep(MB_OK);
        } else if (cmd->kind == YAAT_CMD_TAKE) {
            yaat_take_inventory(cmd->a);
        } else if (cmd->kind == YAAT_CMD_HIDE) {
            YaatEntity *entity = yaat_entity_by_id(&g_rooms[g_current_room], cmd->a);
            if (entity) entity->visible = 0;
        } else if (cmd->kind == YAAT_CMD_IF) {
            if (yaat_get_var(cmd->a)) yaat_execute_commands(cmd->first_child, cmd->child_count);
            else yaat_execute_commands(cmd->first_else_child, cmd->else_child_count);
        }
    }
}

static void yaat_execute_event(YaatEvent *event)
{
    if (event) yaat_execute_commands(event->first_command, event->command_count);
}

static void yaat_enter_room(int room_index)
{
    YaatEvent *enter_event;
    g_current_room = room_index;
    g_player_x = YAAT_BACKBUFFER_WIDTH / 2;
    g_player_y = YAAT_PLAYFIELD_HEIGHT - 20;
    g_target_x = g_player_x;
    g_target_y = g_player_y;
    enter_event = yaat_find_event(g_rooms[g_current_room].events, g_rooms[g_current_room].event_count, "enter", 0);
    yaat_execute_event(enter_event);
}

static int yaat_parse_commands(YaatScriptCursor *cursor);

static void yaat_parse_event(YaatScriptCursor *cursor, YaatEvent *events, int *event_count)
{
    YaatEvent *event;
    ScriptToken *token;
    if (*event_count >= YAAT_MAX_EVENTS) return;
    event = &events[(*event_count)++];
    memset(event, 0, sizeof(*event));
    token = yaat_advance_token(cursor);
    yaat_copy(event->name, sizeof(event->name), token->lexeme, token->length);
    if (yaat_peek(cursor)->type == SCRIPT_TOKEN_IDENTIFIER && !yaat_token_is(yaat_peek(cursor), "on")) {
        token = yaat_advance_token(cursor);
        yaat_copy(event->item, sizeof(event->item), token->lexeme, token->length);
    }
    if (yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) {
        event->first_command = g_command_count;
        event->command_count = yaat_parse_commands(cursor);
    }
}

static int yaat_parse_commands(YaatScriptCursor *cursor)
{
    int first = g_command_count;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        ScriptToken *token = yaat_advance_token(cursor);
        YaatCommand *cmd;
        if (g_command_count >= YAAT_MAX_COMMANDS) break;
        cmd = &g_commands[g_command_count++];
        memset(cmd, 0, sizeof(*cmd));
        if (token->type == SCRIPT_TOKEN_KEYWORD_IF) {
            ScriptToken *cond = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_IF;
            yaat_copy(cmd->a, sizeof(cmd->a), cond->lexeme, cond->length);
            if (yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) { cmd->first_child = g_command_count; cmd->child_count = yaat_parse_commands(cursor); }
            if (yaat_match_token(cursor, SCRIPT_TOKEN_KEYWORD_ELSE) && yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) { cmd->first_else_child = g_command_count; cmd->else_child_count = yaat_parse_commands(cursor); }
        } else if (yaat_token_is(token, "say")) {
            ScriptToken *speaker = yaat_advance_token(cursor);
            ScriptToken *text = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SAY;
            yaat_copy(cmd->a, sizeof(cmd->a), speaker->lexeme, speaker->length);
            yaat_copy(cmd->b, sizeof(cmd->b), text->lexeme, text->length);
        } else if (yaat_token_is(token, "set")) {
            ScriptToken *name = yaat_advance_token(cursor);
            yaat_match_token(cursor, SCRIPT_TOKEN_EQUAL);
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SET;
            yaat_copy(cmd->a, sizeof(cmd->a), name->lexeme, name->length);
            cmd->bool_value = token->type == SCRIPT_TOKEN_KEYWORD_TRUE;
        } else if (yaat_token_is(token, "goto")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_GOTO;
            yaat_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "play_sound")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_PLAY_SOUND;
            yaat_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "take")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_TAKE;
            yaat_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "hide")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_HIDE;
            yaat_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else {
            g_command_count--;
            if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        }
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
    return g_command_count - first;
}

static void yaat_parse_entity(YaatScriptCursor *cursor, YaatRoom *room, YaatEntityKind kind)
{
    YaatEntity *entity;
    ScriptToken *token;
    if (room->entity_count >= YAAT_MAX_ENTITIES) return;
    entity = &room->entities[room->entity_count++];
    memset(entity, 0, sizeof(*entity));
    entity->kind = kind;
    entity->visible = 1;
    token = yaat_advance_token(cursor);
    yaat_copy(entity->id, sizeof(entity->id), token->lexeme, token->length);
    yaat_copy(entity->name, sizeof(entity->name), entity->id, strlen(entity->id));
    if (!yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) return;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        token = yaat_advance_token(cursor);
        if (token->type == SCRIPT_TOKEN_KEYWORD_ON) yaat_parse_event(cursor, entity->events, &entity->event_count);
        else if (yaat_token_is(token, "name")) { token = yaat_advance_token(cursor); yaat_copy(entity->name, sizeof(entity->name), token->lexeme, token->length); }
        else if (yaat_token_is(token, "at")) { entity->x = atoi(yaat_advance_token(cursor)->lexeme); yaat_match_token(cursor, SCRIPT_TOKEN_COMMA); entity->y = atoi(yaat_advance_token(cursor)->lexeme); }
        else if (yaat_token_is(token, "size")) { entity->w = atoi(yaat_advance_token(cursor)->lexeme); yaat_match_token(cursor, SCRIPT_TOKEN_COMMA); entity->h = atoi(yaat_advance_token(cursor)->lexeme); }
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_STRING || yaat_peek(cursor)->type == SCRIPT_TOKEN_IDENTIFIER || yaat_peek(cursor)->type == SCRIPT_TOKEN_INTEGER) yaat_advance_token(cursor);
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
}

static void yaat_parse_room(YaatScriptCursor *cursor)
{
    YaatRoom *room;
    ScriptToken *token;
    if (g_room_count >= YAAT_MAX_ROOMS) return;
    room = &g_rooms[g_room_count++];
    memset(room, 0, sizeof(*room));
    room->color = 0x00d8c7a3UL + (unsigned long)(g_room_count * 0x00101010UL);
    token = yaat_advance_token(cursor);
    yaat_copy(room->id, sizeof(room->id), token->lexeme, token->length);
    yaat_copy(room->label, sizeof(room->label), room->id, strlen(room->id));
    if (!yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) return;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        token = yaat_advance_token(cursor);
        if (token->type == SCRIPT_TOKEN_KEYWORD_ON) yaat_parse_event(cursor, room->events, &room->event_count);
        else if (token->type == SCRIPT_TOKEN_KEYWORD_OBJECT) yaat_parse_entity(cursor, room, YAAT_ENTITY_OBJECT);
        else if (token->type == SCRIPT_TOKEN_KEYWORD_HOTSPOT) yaat_parse_entity(cursor, room, YAAT_ENTITY_HOTSPOT);
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        else if (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE) yaat_advance_token(cursor);
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
}

static void yaat_parse_script_text(const char *source)
{
    ScriptTokenizerResult result = script_tokenize(source);
    YaatScriptCursor cursor;
    cursor.tokens = result.tokens.items;
    cursor.count = result.tokens.count;
    cursor.index = 0;
    while (yaat_peek(&cursor)->type != SCRIPT_TOKEN_EOF) {
        ScriptToken *token = yaat_advance_token(&cursor);
        if (token->type == SCRIPT_TOKEN_KEYWORD_VAR) {
            ScriptToken *name = yaat_advance_token(&cursor);
            char var_name[32];
            yaat_match_token(&cursor, SCRIPT_TOKEN_EQUAL);
            token = yaat_advance_token(&cursor);
            yaat_copy(var_name, sizeof(var_name), name->lexeme, name->length);
            yaat_set_var(var_name, token->type == SCRIPT_TOKEN_KEYWORD_TRUE);
        } else if (token->type == SCRIPT_TOKEN_KEYWORD_ROOM) yaat_parse_room(&cursor);
        else if (yaat_peek(&cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(&cursor); yaat_skip_block(&cursor); }
    }
    script_tokenizer_result_free(&result);
}

static void yaat_load_script_file(const char *path)
{
    unsigned char *buffer;
    size_t size;

    if (!yaat_asset_read_all(&g_asset_store, path, &buffer, &size)) return;
    buffer[size] = '\0';
    yaat_parse_script_text((const char *)buffer);
    free(buffer);
}

static void yaat_load_demo(void)
{
    yaat_load_script_file("scripts/startup.yaat");
    yaat_load_script_file("rooms/room000_start/script.yaat");
    yaat_load_script_file("rooms/room001_intro/script.yaat");
    yaat_load_script_file("rooms/room002_exit/script.yaat");
    yaat_enter_room(0);
}

static void yaat_draw_script_scene(void)
{
    int i;
    YaatRoom *room = &g_rooms[g_current_room];
    yaat_gdi_renderer_clear(&g_renderer, room->color);
    yaat_draw_rect(&g_renderer, 0, YAAT_PLAYFIELD_HEIGHT - 44, YAAT_BACKBUFFER_WIDTH, 44, 0x008a6f48UL);
    for (i = 0; i < room->entity_count; ++i) {
        YaatEntity *e = &room->entities[i];
        if (!e->visible) continue;
        yaat_draw_rect(&g_renderer, e->x, e->y, e->w, e->h, e->kind == YAAT_ENTITY_OBJECT ? 0x00d4b24cUL : 0x004e8bc4UL);
        yaat_draw_rect(&g_renderer, e->x + 2, e->y + 2, e->w - 4, e->h - 4, e->kind == YAAT_ENTITY_OBJECT ? 0x00ffe090UL : 0x008ec5ffUL);
    }
    yaat_draw_rect(&g_renderer, g_target_x - 5, g_target_y - 1, 11, 3, 0x000f3c70UL);
    yaat_draw_rect(&g_renderer, g_target_x - 1, g_target_y - 5, 3, 11, 0x000f3c70UL);
    yaat_draw_player();
    yaat_draw_rect(&g_renderer, 0, YAAT_PLAYFIELD_HEIGHT, YAAT_BACKBUFFER_WIDTH, 40, 0x00101018UL);
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
    yaat_draw_cursor_placeholder();
}

static void yaat_update_player(void)
{
    int dx;
    int dy;

    g_target_x = yaat_clamp_int(g_target_x, YAAT_PLAYER_WIDTH / 2,
                                YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    g_target_y = yaat_clamp_int(g_target_y, YAAT_PLAYER_HEIGHT,
                                YAAT_PLAYFIELD_HEIGHT - 1);
    dx = g_target_x - g_player_x;
    dy = g_target_y - g_player_y;
    if (dx > YAAT_PLAYER_SPEED_PIXELS) dx = YAAT_PLAYER_SPEED_PIXELS; else if (dx < -YAAT_PLAYER_SPEED_PIXELS) dx = -YAAT_PLAYER_SPEED_PIXELS;
    if (dy > YAAT_PLAYER_SPEED_PIXELS) dy = YAAT_PLAYER_SPEED_PIXELS; else if (dy < -YAAT_PLAYER_SPEED_PIXELS) dy = -YAAT_PLAYER_SPEED_PIXELS;
    g_player_x += dx; g_player_y += dy;
}

static void yaat_nudge_player_target(int dx, int dy)
{
    if (dx < 0) {
        g_player_facing_right = 0;
    } else if (dx > 0) {
        g_player_facing_right = 1;
    }
    g_target_x = yaat_clamp_int(g_target_x + dx, YAAT_PLAYER_WIDTH / 2,
                                YAAT_BACKBUFFER_WIDTH - (YAAT_PLAYER_WIDTH / 2));
    g_target_y = yaat_clamp_int(g_target_y + dy, YAAT_PLAYER_HEIGHT,
                                YAAT_PLAYFIELD_HEIGHT - 1);
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

static void yaat_runtime_event_name(char *dst, size_t dst_size, const char *script_event)
{
    const char *name = script_event;
    if (script_event != 0 && strncmp(script_event, "on_", 3) == 0) {
        name = script_event + 3;
    }
    yaat_copy(dst, dst_size, name != 0 ? name : "click", strlen(name != 0 ? name : "click"));
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

    yaat_runtime_event_name(event_name, sizeof(event_name), script_event);
    event = yaat_find_event(entity->events, entity->event_count, event_name, 0);
    if (event == 0 && strcmp(event_name, "click") != 0) {
        event = yaat_find_event(entity->events, entity->event_count, "click", 0);
    }
    if (event == 0) event = yaat_find_event(entity->events, entity->event_count, "look", 0);
    yaat_execute_event(event);
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

static int yaat_runtime_click_game(int x, int y)
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
            yaat_runtime_join_path(path, sizeof(path),
                                   yaat_runtime_logical_path(room->room_path),
                                   "objects.ini");
            if (!yaat_runtime_ini_hit(path, x, y, id, sizeof(id), event_name, sizeof(event_name))) {
                yaat_copy(id, sizeof(id), object->id, strlen(object->id));
                yaat_copy(event_name, sizeof(event_name), "on_click", strlen("on_click"));
            }
            yaat_runtime_execute_entity_event(id, event_name);
            return 1;
        }
    }

    yaat_runtime_join_path(path, sizeof(path),
                           yaat_runtime_logical_path(room->room_path),
                           "hotspots.ini");
    if (yaat_runtime_ini_hit(path, x, y, id, sizeof(id), event_name, sizeof(event_name))) {
        yaat_runtime_execute_entity_event(id, event_name);
        return 1;
    }
    return 0;
}

static void yaat_click_game(int x, int y)
{
    int i;
    YaatRoom *room;
    if (g_runtime_load.ok) {
        yaat_runtime_click_game(x, y);
        return;
    }
    room = &g_rooms[g_current_room];
    for (i = room->entity_count - 1; i >= 0; --i) {
        YaatEntity *e = &room->entities[i];
        if (e->visible && x >= e->x && y >= e->y && x < e->x + e->w && y < e->y + e->h) {
            YaatEvent *event = 0;
            if (strcmp(e->id, "locked_door") == 0 && yaat_get_var("door_locked") && yaat_has_inventory("brass_key")) event = yaat_find_event(e->events, e->event_count, "use", "brass_key");
            if (!event) event = yaat_find_event(e->events, e->event_count, "click", 0);
            if (!event) event = yaat_find_event(e->events, e->event_count, "look", 0);
            yaat_execute_event(event);
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

static void yaat_set_target_from_client(HWND window, int client_x, int client_y)
{
    int backbuffer_x;
    int backbuffer_y;

    if (!yaat_client_to_backbuffer(window, client_x, client_y,
                                   &backbuffer_x, &backbuffer_y)) return;
    g_cursor_x = backbuffer_x;
    g_cursor_y = backbuffer_y;
    if (backbuffer_x < g_player_x) {
        g_player_facing_right = 0;
    } else if (backbuffer_x > g_player_x) {
        g_player_facing_right = 1;
    }
    g_target_x = backbuffer_x;
    g_target_y = yaat_clamp_int(backbuffer_y, YAAT_PLAYER_HEIGHT, YAAT_PLAYFIELD_HEIGHT - 1);
    yaat_click_game(backbuffer_x, backbuffer_y);
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
        yaat_load_demo();
        if (g_runtime_load.ok) {
            runtime_room_index = yaat_room_index_by_id(g_runtime_load.room.id);
            if (runtime_room_index >= 0) g_current_room = runtime_room_index;
        }
        yaat_load_runtime_hotspots();
        SetTimer(window, YAAT_FRAME_TIMER_ID, YAAT_FRAME_TIMER_MS, 0);
        return 0;
    }
    case WM_MOUSEMOVE:
        yaat_update_cursor_from_client(window, (int)(short)LOWORD(l_param),
                                       (int)(short)HIWORD(l_param));
        return 0;
    case WM_SETCURSOR:
        SetCursor(0);
        return TRUE;
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
        if (w_param == VK_LEFT) yaat_nudge_player_target(-16, 0);
        else if (w_param == VK_RIGHT) yaat_nudge_player_target(16, 0);
        else if (w_param == VK_UP) yaat_nudge_player_target(0, -16);
        else if (w_param == VK_DOWN) yaat_nudge_player_target(0, 16);
        else break;
        InvalidateRect(window, 0, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        if (g_dialogue_visible) g_dialogue_visible = 0;
        else yaat_set_target_from_client(window, (int)(short)LOWORD(l_param),
                                         (int)(short)HIWORD(l_param));
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_TIMER:
        if (w_param == YAAT_FRAME_TIMER_ID) { yaat_update_player(); InvalidateRect(window, 0, FALSE); return 0; }
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
        KillTimer(window, YAAT_FRAME_TIMER_ID); yaat_gdi_renderer_shutdown(&g_renderer); g_renderer_ready = 0; PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcA(window, message, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    WNDCLASSEXA window_class;
    HWND window;
    MSG message;
    YaatAssetStore asset_store;

    (void)previous_instance;
    (void)command_line;

    yaat_asset_store_init(&g_asset_store, "game");
    yaat_runtime_load_start_room("game/game.ini", &g_runtime_load);
    yaat_asset_store_init_loose(&asset_store, "game");
    yaat_runtime_load_start_room_from_store(&asset_store, &g_runtime_load);

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class); window_class.style = CS_HREDRAW | CS_VREDRAW; window_class.lpfnWndProc = yaat_window_proc;
    window_class.hInstance = instance; window_class.hCursor = LoadCursorA(0, IDC_ARROW); window_class.hbrBackground = 0; window_class.lpszClassName = YAAT_WINDOW_CLASS_NAME;
    if (RegisterClassExA(&window_class) == 0) return 1;
    window = CreateWindowExA(0, YAAT_WINDOW_CLASS_NAME, YAAT_WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, 0, 0, instance, 0);
    if (window == 0) return 1;
    ShowWindow(window, show_command); UpdateWindow(window);
    while (GetMessageA(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageA(&message); }
    return (int)message.wParam;
}
