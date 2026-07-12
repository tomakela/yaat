#include "navigation.h"

#ifdef YAAT_EMBEDDED_MODULE
static int g_path_waypoint_x[YAAT_MAX_PATH_WAYPOINTS];
static int g_path_waypoint_y[YAAT_MAX_PATH_WAYPOINTS];
static int g_path_waypoint_count;
static int g_path_waypoint_index;

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
    if (x != g_player_x || y != g_player_y) {
        (void)yaat_player_walk_animation_for_delta(x - g_player_x, y - g_player_y);
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


#endif
