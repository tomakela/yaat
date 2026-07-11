#include "script_parser.h"
#include "script_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct YaatScriptCursor {
    ScriptToken *tokens;
    size_t count;
    size_t index;
} YaatScriptCursor;

static void parser_copy(char *dst, size_t dst_size, const char *src, size_t len)
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

static YaatValue yaat_parse_value_token(ScriptToken *token)
{
    char text[96];
    parser_copy(text, sizeof(text), token->lexeme, token->length);
    if (token->type == SCRIPT_TOKEN_KEYWORD_TRUE) return yaat_value_bool(1);
    if (token->type == SCRIPT_TOKEN_KEYWORD_FALSE) return yaat_value_bool(0);
    if (token->type == SCRIPT_TOKEN_INTEGER) return yaat_value_int(atoi(text));
    return yaat_value_string(text);
}

static YaatConditionOp yaat_parse_condition_op(ScriptTokenType type)
{
    if (type == SCRIPT_TOKEN_EQUAL_EQUAL) return YAAT_COND_EQ;
    if (type == SCRIPT_TOKEN_BANG_EQUAL) return YAAT_COND_NE;
    if (type == SCRIPT_TOKEN_LESS) return YAAT_COND_LT;
    if (type == SCRIPT_TOKEN_LESS_EQUAL) return YAAT_COND_LTE;
    if (type == SCRIPT_TOKEN_GREATER) return YAAT_COND_GT;
    if (type == SCRIPT_TOKEN_GREATER_EQUAL) return YAAT_COND_GTE;
    return YAAT_COND_TRUTHY;
}

static int yaat_is_condition_op(ScriptTokenType type)
{
    return type == SCRIPT_TOKEN_EQUAL_EQUAL || type == SCRIPT_TOKEN_BANG_EQUAL ||
           type == SCRIPT_TOKEN_LESS || type == SCRIPT_TOKEN_LESS_EQUAL ||
           type == SCRIPT_TOKEN_GREATER || type == SCRIPT_TOKEN_GREATER_EQUAL;
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


static int yaat_parse_commands(YaatScriptPackage *package, YaatScriptCursor *cursor);

static void yaat_parse_event(YaatScriptPackage *package, YaatScriptCursor *cursor, YaatEvent *events, int *event_count)
{
    YaatEvent *event;
    ScriptToken *token;
    if (*event_count >= YAAT_MAX_EVENTS) return;
    event = &events[(*event_count)++];
    memset(event, 0, sizeof(*event));
    token = yaat_advance_token(cursor);
    parser_copy(event->name, sizeof(event->name), token->lexeme, token->length);
    if (yaat_peek(cursor)->type == SCRIPT_TOKEN_IDENTIFIER && !yaat_token_is(yaat_peek(cursor), "on")) {
        token = yaat_advance_token(cursor);
        parser_copy(event->item, sizeof(event->item), token->lexeme, token->length);
    }
    if (yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) {
        event->first_command = package->command_count;
        event->command_count = yaat_parse_commands(package, cursor);
    }
}

static int yaat_parse_commands(YaatScriptPackage *package, YaatScriptCursor *cursor)
{
    int first = package->command_count;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        ScriptToken *token = yaat_advance_token(cursor);
        YaatCommand *cmd;
        if (package->command_count >= YAAT_MAX_COMMANDS) break;
        cmd = &package->commands[package->command_count++];
        memset(cmd, 0, sizeof(*cmd));
        if (token->type == SCRIPT_TOKEN_KEYWORD_IF) {
            ScriptToken *cond = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_IF;
            cmd->condition_op = YAAT_COND_TRUTHY;
            parser_copy(cmd->a, sizeof(cmd->a), cond->lexeme, cond->length);
            if ((yaat_token_is(cond, "has") || yaat_token_is(cond, "inventory")) && yaat_peek(cursor)->type != SCRIPT_TOKEN_LEFT_BRACE) {
                token = yaat_advance_token(cursor);
                parser_copy(cmd->b, sizeof(cmd->b), token->lexeme, token->length);
            }
            if (yaat_is_condition_op(yaat_peek(cursor)->type)) {
                ScriptToken *op = yaat_advance_token(cursor);
                ScriptToken *rhs = yaat_advance_token(cursor);
                cmd->condition_op = yaat_parse_condition_op(op->type);
                cmd->value = yaat_parse_value_token(rhs);
            }
            if (yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) { cmd->first_child = package->command_count; cmd->child_count = yaat_parse_commands(package, cursor); }
            if (yaat_match_token(cursor, SCRIPT_TOKEN_KEYWORD_ELSE) && yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) { cmd->first_else_child = package->command_count; cmd->else_child_count = yaat_parse_commands(package, cursor); }
        } else if (yaat_token_is(token, "say")) {
            ScriptToken *speaker = yaat_advance_token(cursor);
            ScriptToken *text = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SAY;
            parser_copy(cmd->a, sizeof(cmd->a), speaker->lexeme, speaker->length);
            parser_copy(cmd->b, sizeof(cmd->b), text->lexeme, text->length);
        } else if (yaat_token_is(token, "set")) {
            ScriptToken *name = yaat_advance_token(cursor);
            yaat_match_token(cursor, SCRIPT_TOKEN_EQUAL);
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SET;
            parser_copy(cmd->a, sizeof(cmd->a), name->lexeme, name->length);
            cmd->value = yaat_parse_value_token(token);
            cmd->bool_value = cmd->value.bool_value;
            cmd->int_value = cmd->value.int_value;
        } else if (yaat_token_is(token, "goto")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_GOTO;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "play_sound")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_PLAY_SOUND;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "take")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_TAKE;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "pickup")) {
            ScriptToken *object_id = yaat_advance_token(cursor);
            ScriptToken *item_id = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_PICKUP;
            parser_copy(cmd->a, sizeof(cmd->a), object_id->lexeme, object_id->length);
            parser_copy(cmd->b, sizeof(cmd->b), item_id->lexeme, item_id->length);
        } else if (yaat_token_is(token, "drop")) {
            ScriptToken *item_id = yaat_advance_token(cursor);
            ScriptToken *object_id = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_DROP;
            parser_copy(cmd->a, sizeof(cmd->a), item_id->lexeme, item_id->length);
            parser_copy(cmd->b, sizeof(cmd->b), object_id->lexeme, object_id->length);
        } else if (yaat_token_is(token, "remove_inventory")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_REMOVE_INVENTORY;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "consume")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_CONSUME;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "hide")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_HIDE;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "show")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SHOW;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "move")) {
            ScriptToken *x;
            ScriptToken *y;
            token = yaat_advance_token(cursor);
            x = yaat_advance_token(cursor);
            yaat_match_token(cursor, SCRIPT_TOKEN_COMMA);
            y = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_MOVE_OBJECT;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
            cmd->bool_value = atoi(x->lexeme);
            cmd->int_value = atoi(y->lexeme);
        } else if (yaat_token_is(token, "set_sprite")) {
            ScriptToken *sprite;
            token = yaat_advance_token(cursor);
            sprite = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SET_OBJECT_SPRITE;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
            parser_copy(cmd->b, sizeof(cmd->b), sprite->lexeme, sprite->length);
        } else if (yaat_token_is(token, "title_card") || yaat_token_is(token, "cutscene_text")) {
            ScriptToken *text = yaat_advance_token(cursor);
            ScriptToken *duration = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_TITLE_CARD;
            parser_copy(cmd->a, sizeof(cmd->a), text->lexeme, text->length);
            cmd->int_value = atoi(duration->lexeme);
        } else if (yaat_token_is(token, "wait")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_WAIT;
            cmd->int_value = atoi(token->lexeme);
        } else if (yaat_token_is(token, "move_player")) {
            ScriptToken *x = yaat_advance_token(cursor);
            ScriptToken *y;
            yaat_match_token(cursor, SCRIPT_TOKEN_COMMA);
            y = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_MOVE_PLAYER;
            cmd->bool_value = atoi(x->lexeme);
            cmd->int_value = atoi(y->lexeme);
        } else if (yaat_token_is(token, "dialog")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_DIALOG;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "choice")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_CHOICE;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else if (yaat_token_is(token, "show_player")) {
            cmd->kind = YAAT_CMD_SET_PLAYER_VISIBLE;
            cmd->bool_value = 1;
        } else if (yaat_token_is(token, "hide_player")) {
            cmd->kind = YAAT_CMD_SET_PLAYER_VISIBLE;
            cmd->bool_value = 0;
        } else if (yaat_token_is(token, "shake")) {
            ScriptToken *duration = yaat_advance_token(cursor);
            ScriptToken *magnitude = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_SHAKE;
            cmd->bool_value = atoi(duration->lexeme);
            cmd->int_value = atoi(magnitude->lexeme);
        } else if (yaat_token_is(token, "call")) {
            token = yaat_advance_token(cursor);
            cmd->kind = YAAT_CMD_CALL;
            parser_copy(cmd->a, sizeof(cmd->a), token->lexeme, token->length);
        } else {
            package->command_count--;
            if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        }
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
    return package->command_count - first;
}

static void yaat_parse_entity(YaatScriptPackage *package, YaatScriptCursor *cursor, YaatRoom *room, YaatEntityKind kind)
{
    YaatEntity *entity;
    ScriptToken *token;
    if (room->entity_count >= YAAT_MAX_ENTITIES) return;
    entity = &room->entities[room->entity_count++];
    memset(entity, 0, sizeof(*entity));
    entity->kind = kind;
    entity->visible = 1;
    token = yaat_advance_token(cursor);
    parser_copy(entity->id, sizeof(entity->id), token->lexeme, token->length);
    parser_copy(entity->name, sizeof(entity->name), entity->id, strlen(entity->id));
    if (!yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) return;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        token = yaat_advance_token(cursor);
        if (token->type == SCRIPT_TOKEN_KEYWORD_ON) yaat_parse_event(package, cursor, entity->events, &entity->event_count);
        else if (yaat_token_is(token, "name")) { token = yaat_advance_token(cursor); parser_copy(entity->name, sizeof(entity->name), token->lexeme, token->length); }
        else if (yaat_token_is(token, "at")) { entity->x = atoi(yaat_advance_token(cursor)->lexeme); yaat_match_token(cursor, SCRIPT_TOKEN_COMMA); entity->y = atoi(yaat_advance_token(cursor)->lexeme); }
        else if (yaat_token_is(token, "size")) { entity->w = atoi(yaat_advance_token(cursor)->lexeme); yaat_match_token(cursor, SCRIPT_TOKEN_COMMA); entity->h = atoi(yaat_advance_token(cursor)->lexeme); }
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_STRING || yaat_peek(cursor)->type == SCRIPT_TOKEN_IDENTIFIER || yaat_peek(cursor)->type == SCRIPT_TOKEN_INTEGER) yaat_advance_token(cursor);
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
}

static void yaat_parse_room(YaatScriptPackage *package, YaatScriptCursor *cursor)
{
    YaatRoom *room;
    ScriptToken *token;
    if (package->room_count >= YAAT_MAX_ROOMS) return;
    room = &package->rooms[package->room_count++];
    memset(room, 0, sizeof(*room));
    room->color = 0x00d8c7a3UL + (unsigned long)(package->room_count * 0x00101010UL);
    token = yaat_advance_token(cursor);
    parser_copy(room->id, sizeof(room->id), token->lexeme, token->length);
    parser_copy(room->label, sizeof(room->label), room->id, strlen(room->id));
    if (!yaat_match_token(cursor, SCRIPT_TOKEN_LEFT_BRACE)) return;
    while (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE && yaat_peek(cursor)->type != SCRIPT_TOKEN_EOF) {
        token = yaat_advance_token(cursor);
        if (token->type == SCRIPT_TOKEN_KEYWORD_ON) yaat_parse_event(package, cursor, room->events, &room->event_count);
        else if (token->type == SCRIPT_TOKEN_KEYWORD_OBJECT) yaat_parse_entity(package, cursor, room, YAAT_ENTITY_OBJECT);
        else if (token->type == SCRIPT_TOKEN_KEYWORD_HOTSPOT) yaat_parse_entity(package, cursor, room, YAAT_ENTITY_HOTSPOT);
        else if (yaat_peek(cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(cursor); yaat_skip_block(cursor); }
        else if (yaat_peek(cursor)->type != SCRIPT_TOKEN_RIGHT_BRACE) yaat_advance_token(cursor);
    }
    yaat_match_token(cursor, SCRIPT_TOKEN_RIGHT_BRACE);
}

int yaat_parse_script_text_into(YaatScriptPackage *package, const char *source)
{
    ScriptTokenizerResult result;
    YaatScriptCursor cursor;
    int ok;
    if (!package || !source) return 0;
    result = script_tokenize(source);
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
            parser_copy(var_name, sizeof(var_name), name->lexeme, name->length);
            {
                YaatValue value = yaat_parse_value_token(token);
                yaat_script_package_set_var_value(package, var_name, &value);
            }
        } else if (token->type == SCRIPT_TOKEN_KEYWORD_ROOM) yaat_parse_room(package, &cursor);
        else if (token->type == SCRIPT_TOKEN_KEYWORD_EVENT || yaat_token_is(token, "proc")) yaat_parse_event(package, &cursor, package->global_events, &package->global_event_count);
        else if (yaat_peek(&cursor)->type == SCRIPT_TOKEN_LEFT_BRACE) { yaat_advance_token(&cursor); yaat_skip_block(&cursor); }
    }
    ok = result.diagnostics.count == 0;
    script_tokenizer_result_free(&result);
    return ok;
}

int yaat_parse_script_file_into(YaatScriptPackage *package, const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *buffer;
    int ok = 0;
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 0; }
    buffer = (char *)malloc((size_t)size + 1);
    if (buffer) {
        if (fread(buffer, 1, (size_t)size, file) == (size_t)size) {
            buffer[size] = '\0';
            ok = yaat_parse_script_text_into(package, buffer);
        }
        free(buffer);
    }
    fclose(file);
    return ok;
}

