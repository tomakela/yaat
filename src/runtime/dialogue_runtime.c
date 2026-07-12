#include "dialogue_runtime.h"

#ifdef YAAT_EMBEDDED_MODULE
static char g_dialogue_speaker[32];
static char g_dialogue_text[YAAT_TEXT_MAX];
static int g_dialogue_visible;
static int g_dialogue_choice_visible;
static YaatRuntimeDialog g_active_dialog;
static char g_active_dialog_node[YAAT_ASSET_MAX_NAME];

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

static void yaat_player_say(const char *text)
{
    yaat_copy(g_dialogue_speaker, sizeof(g_dialogue_speaker), "player", strlen("player"));
    yaat_copy(g_dialogue_text, sizeof(g_dialogue_text), text, strlen(text));
    g_dialogue_visible = 1;
}

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

#endif
