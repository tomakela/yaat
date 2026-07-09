#include <windows.h>

#include "platform/win32/gdi_renderer.h"
#include "script_tokenizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime/asset_loader.h"

#define YAAT_WINDOW_CLASS_NAME "YAATWindowClass"
#define YAAT_WINDOW_TITLE "YAAT"
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

static void yaat_draw_runtime_room(void)
{
    int i;
    int floor_y;
    unsigned long background_color;

    background_color = yaat_hash_color(g_runtime_load.room.background,
                                        0x00d8c7a3UL);
    yaat_gdi_renderer_clear(&g_renderer, background_color);

    floor_y = YAAT_BACKBUFFER_HEIGHT - 44;
    if (g_runtime_load.room.height > 0) {
        floor_y = (YAAT_BACKBUFFER_HEIGHT * 3) / 4;
    }
    yaat_draw_rect(&g_renderer, 0, floor_y, YAAT_BACKBUFFER_WIDTH,
                   YAAT_BACKBUFFER_HEIGHT - floor_y, 0x005f6f4aUL);

    yaat_draw_rect(&g_renderer, 12, 12, 128, 22, 0x00282828UL);
    yaat_draw_rect(&g_renderer, 14, 14, 124, 18, 0x00d8d0b8UL);

    for (i = 0; i < g_runtime_load.room.object_count; ++i) {
        YaatRuntimeObject *object;
        unsigned long object_color;

        object = &g_runtime_load.room.objects[i];
        if (!object->visible || object->width <= 0 || object->height <= 0) {
            continue;
        }
        object_color = yaat_hash_color(object->sprite, 0x002f5f9eUL);
        yaat_draw_rect(&g_renderer, object->x, object->y,
                       object->width, object->height, 0x00202020UL);
        yaat_draw_rect(&g_renderer, object->x + 1, object->y + 1,
                       object->width - 2, object->height - 2, object_color);
    }
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

static YaatRoom *yaat_room_by_id(const char *id)
{
    int i;
    for (i = 0; i < g_room_count; ++i) if (strcmp(g_rooms[i].id, id) == 0) return &g_rooms[i];
    return 0;
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
            yaat_match_token(&cursor, SCRIPT_TOKEN_EQUAL);
            token = yaat_advance_token(&cursor);
            char var_name[32];
            yaat_copy(var_name, sizeof(var_name), name->lexeme, name->length);
            yaat_set_var(var_name, token->type == SCRIPT_TOKEN_KEYWORD_TRUE);
        } else if (token->type == SCRIPT_TOKEN_KEYWORD_ROOM) yaat_parse_room(&cursor);
        else if (yaat_peek(&cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(&cursor); yaat_skip_block(&cursor); }
    }
    script_tokenizer_result_free(&result);
}

static void yaat_load_script_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *buffer;
    if (!file) return;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer = (char *)malloc((size_t)size + 1);
    if (buffer) {
        fread(buffer, 1, (size_t)size, file);
        buffer[size] = '\0';
        yaat_parse_script_text(buffer);
        free(buffer);
    }
    fclose(file);
}

static void yaat_load_demo(void)
{
    yaat_load_script_file("game/scripts/startup.yaat");
    yaat_load_script_file("game/rooms/room000_start/script.yaat");
    yaat_load_script_file("game/rooms/room001_intro/script.yaat");
    yaat_load_script_file("game/rooms/room002_exit/script.yaat");
    yaat_enter_room(0);
}

static void yaat_draw_script_scene(void)
{
    int shadow_x, shadow_y, body_x, body_y, i;
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
    shadow_x = g_player_x - (YAAT_PLAYER_WIDTH / 2) - 2; shadow_y = g_player_y + 7;
    body_x = g_player_x - (YAAT_PLAYER_WIDTH / 2); body_y = g_player_y - YAAT_PLAYER_HEIGHT;
    yaat_draw_rect(&g_renderer, shadow_x, shadow_y, YAAT_PLAYER_WIDTH + 4, 5, 0x00664f38UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y, 10, 10, 0x005a3a24UL);
    yaat_draw_rect(&g_renderer, body_x + 3, body_y + 9, 12, 15, 0x002f5f9eUL);
    yaat_draw_rect(&g_renderer, body_x, body_y + 12, 4, 12, 0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 14, body_y + 12, 4, 12, 0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y + 24, 4, 10, 0x001f2430UL);
    yaat_draw_rect(&g_renderer, body_x + 10, body_y + 24, 4, 10, 0x001f2430UL);
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
}

static void yaat_update_player(void)
{
    int dx = g_target_x - g_player_x;
    int dy = g_target_y - g_player_y;
    if (dx > YAAT_PLAYER_SPEED_PIXELS) dx = YAAT_PLAYER_SPEED_PIXELS; else if (dx < -YAAT_PLAYER_SPEED_PIXELS) dx = -YAAT_PLAYER_SPEED_PIXELS;
    if (dy > YAAT_PLAYER_SPEED_PIXELS) dy = YAAT_PLAYER_SPEED_PIXELS; else if (dy < -YAAT_PLAYER_SPEED_PIXELS) dy = -YAAT_PLAYER_SPEED_PIXELS;
    g_player_x += dx; g_player_y += dy;
}

static void yaat_click_game(int x, int y)
{
    int i;
    YaatRoom *room = &g_rooms[g_current_room];
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

static void yaat_set_target_from_client(HWND window, int client_x, int client_y)
{
    RECT client_rect; int client_width; int client_height;
    if (GetClientRect(window, &client_rect) == 0) return;
    client_width = client_rect.right - client_rect.left; client_height = client_rect.bottom - client_rect.top;
    if (client_width <= 0 || client_height <= 0) return;
    g_target_x = (client_x * YAAT_BACKBUFFER_WIDTH) / client_width;
    g_target_y = (client_y * YAAT_BACKBUFFER_HEIGHT) / client_height;
    g_target_x = yaat_clamp_int(g_target_x, 0, YAAT_BACKBUFFER_WIDTH - 1);
    g_target_y = yaat_clamp_int(g_target_y, 0, YAAT_PLAYFIELD_HEIGHT - 1);
    yaat_click_game(g_target_x, g_target_y);
}

static LRESULT CALLBACK yaat_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE: {
        HDC dc = GetDC(window);
        if (dc == 0) return -1;
        g_renderer_ready = yaat_gdi_renderer_init(&g_renderer, dc, YAAT_BACKBUFFER_WIDTH, YAAT_BACKBUFFER_HEIGHT);
        ReleaseDC(window, dc);
        if (!g_renderer_ready) return -1;
        yaat_load_demo();
        SetTimer(window, YAAT_FRAME_TIMER_ID, YAAT_FRAME_TIMER_MS, 0);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (g_dialogue_visible) g_dialogue_visible = 0;
        else yaat_set_target_from_client(window, LOWORD(l_param), HIWORD(l_param));
        InvalidateRect(window, 0, FALSE); return 0;
    case WM_TIMER:
        if (w_param == YAAT_FRAME_TIMER_ID) { yaat_update_player(); InvalidateRect(window, 0, FALSE); return 0; }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc; RECT client_rect;
        dc = BeginPaint(window, &paint);
        if (g_renderer_ready && GetClientRect(window, &client_rect) != 0) {
            yaat_render_scene();
            yaat_gdi_renderer_present_stretched(&g_renderer, dc, 0, 0, client_rect.right - client_rect.left, client_rect.bottom - client_rect.top);
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

    (void)previous_instance;
    (void)command_line;

    yaat_runtime_load_start_room("game/game.ini", &g_runtime_load);

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class); window_class.style = CS_HREDRAW | CS_VREDRAW; window_class.lpfnWndProc = yaat_window_proc;
    window_class.hInstance = instance; window_class.hCursor = LoadCursorA(0, IDC_ARROW); window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); window_class.lpszClassName = YAAT_WINDOW_CLASS_NAME;
    if (RegisterClassExA(&window_class) == 0) return 1;
    window = CreateWindowExA(0, YAAT_WINDOW_CLASS_NAME, YAAT_WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, 0, 0, instance, 0);
    if (window == 0) return 1;
    ShowWindow(window, show_command); UpdateWindow(window);
    while (GetMessageA(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageA(&message); }
    return (int)message.wParam;
}
