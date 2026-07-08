#include <windows.h>

#include "platform/win32/gdi_renderer.h"

#define YAAT_WINDOW_CLASS_NAME "YAATWindowClass"
#define YAAT_WINDOW_TITLE "YAAT"
#define YAAT_BACKBUFFER_WIDTH 320
#define YAAT_BACKBUFFER_HEIGHT 240
#define YAAT_PLAYER_WIDTH 18
#define YAAT_PLAYER_HEIGHT 34
#define YAAT_PLAYER_SPEED_PIXELS 4
#define YAAT_FRAME_TIMER_ID 1
#define YAAT_FRAME_TIMER_MS 16

static YaatGdiRenderer g_renderer;
static int g_renderer_ready;
static int g_player_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_player_y = YAAT_BACKBUFFER_HEIGHT / 2;
static int g_target_x = YAAT_BACKBUFFER_WIDTH / 2;
static int g_target_y = YAAT_BACKBUFFER_HEIGHT / 2;

static int yaat_clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
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

    if (renderer == 0 || renderer->pixels == 0 || width <= 0 || height <= 0) {
        return;
    }

    clipped_x0 = yaat_clamp_int(x, 0, renderer->width);
    clipped_y0 = yaat_clamp_int(y, 0, renderer->height);
    clipped_x1 = yaat_clamp_int(x + width, 0, renderer->width);
    clipped_y1 = yaat_clamp_int(y + height, 0, renderer->height);

    for (draw_y = clipped_y0; draw_y < clipped_y1; ++draw_y) {
        row = (unsigned long *)((unsigned char *)renderer->pixels +
                               (draw_y * renderer->pitch));
        for (draw_x = clipped_x0; draw_x < clipped_x1; ++draw_x) {
            row[draw_x] = color;
        }
    }
}

static void yaat_render_scene(void)
{
    int shadow_x;
    int shadow_y;
    int body_x;
    int body_y;

    yaat_gdi_renderer_clear(&g_renderer, 0x00d8c7a3UL);

    yaat_draw_rect(&g_renderer, 0, YAAT_BACKBUFFER_HEIGHT - 44,
                   YAAT_BACKBUFFER_WIDTH, 44, 0x008a6f48UL);
    yaat_draw_rect(&g_renderer, 18, 18, 92, 28, 0x00bca77fUL);
    yaat_draw_rect(&g_renderer, 22, 22, 84, 20, 0x00ebddb8UL);

    yaat_draw_rect(&g_renderer, g_target_x - 5, g_target_y - 1, 11, 3,
                   0x000f3c70UL);
    yaat_draw_rect(&g_renderer, g_target_x - 1, g_target_y - 5, 3, 11,
                   0x000f3c70UL);

    shadow_x = g_player_x - (YAAT_PLAYER_WIDTH / 2) - 2;
    shadow_y = g_player_y + 7;
    body_x = g_player_x - (YAAT_PLAYER_WIDTH / 2);
    body_y = g_player_y - YAAT_PLAYER_HEIGHT;

    yaat_draw_rect(&g_renderer, shadow_x, shadow_y,
                   YAAT_PLAYER_WIDTH + 4, 5, 0x00664f38UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y, 10, 10, 0x005a3a24UL);
    yaat_draw_rect(&g_renderer, body_x + 3, body_y + 9, 12, 15, 0x002f5f9eUL);
    yaat_draw_rect(&g_renderer, body_x, body_y + 12, 4, 12, 0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 14, body_y + 12, 4, 12, 0x00274774UL);
    yaat_draw_rect(&g_renderer, body_x + 4, body_y + 24, 4, 10, 0x001f2430UL);
    yaat_draw_rect(&g_renderer, body_x + 10, body_y + 24, 4, 10, 0x001f2430UL);
}

static void yaat_update_player(void)
{
    int dx;
    int dy;

    dx = g_target_x - g_player_x;
    dy = g_target_y - g_player_y;

    if (dx > YAAT_PLAYER_SPEED_PIXELS) {
        dx = YAAT_PLAYER_SPEED_PIXELS;
    } else if (dx < -YAAT_PLAYER_SPEED_PIXELS) {
        dx = -YAAT_PLAYER_SPEED_PIXELS;
    }

    if (dy > YAAT_PLAYER_SPEED_PIXELS) {
        dy = YAAT_PLAYER_SPEED_PIXELS;
    } else if (dy < -YAAT_PLAYER_SPEED_PIXELS) {
        dy = -YAAT_PLAYER_SPEED_PIXELS;
    }

    g_player_x += dx;
    g_player_y += dy;
}

static void yaat_set_target_from_client(HWND window, int client_x, int client_y)
{
    RECT client_rect;
    int client_width;
    int client_height;

    if (GetClientRect(window, &client_rect) == 0) {
        return;
    }

    client_width = client_rect.right - client_rect.left;
    client_height = client_rect.bottom - client_rect.top;
    if (client_width <= 0 || client_height <= 0) {
        return;
    }

    g_target_x = (client_x * YAAT_BACKBUFFER_WIDTH) / client_width;
    g_target_y = (client_y * YAAT_BACKBUFFER_HEIGHT) / client_height;
    g_target_x = yaat_clamp_int(g_target_x, 0, YAAT_BACKBUFFER_WIDTH - 1);
    g_target_y = yaat_clamp_int(g_target_y, 0, YAAT_BACKBUFFER_HEIGHT - 1);
}

static LRESULT CALLBACK yaat_window_proc(HWND window, UINT message,
                                         WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE: {
        HDC dc;

        dc = GetDC(window);
        if (dc == 0) {
            return -1;
        }
        g_renderer_ready = yaat_gdi_renderer_init(&g_renderer, dc,
                                                  YAAT_BACKBUFFER_WIDTH,
                                                  YAAT_BACKBUFFER_HEIGHT);
        ReleaseDC(window, dc);
        if (!g_renderer_ready) {
            return -1;
        }
        SetTimer(window, YAAT_FRAME_TIMER_ID, YAAT_FRAME_TIMER_MS, 0);
        return 0;
    }
    case WM_LBUTTONDOWN:
        yaat_set_target_from_client(window, LOWORD(l_param), HIWORD(l_param));
        InvalidateRect(window, 0, FALSE);
        return 0;
    case WM_TIMER:
        if (w_param == YAAT_FRAME_TIMER_ID) {
            yaat_update_player();
            InvalidateRect(window, 0, FALSE);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc;
        RECT client_rect;

        dc = BeginPaint(window, &paint);
        if (g_renderer_ready && GetClientRect(window, &client_rect) != 0) {
            yaat_render_scene();
            yaat_gdi_renderer_present_stretched(&g_renderer, dc, 0, 0,
                                                client_rect.right - client_rect.left,
                                                client_rect.bottom - client_rect.top);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        KillTimer(window, YAAT_FRAME_TIMER_ID);
        yaat_gdi_renderer_shutdown(&g_renderer);
        g_renderer_ready = 0;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcA(window, message, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command)
{
    WNDCLASSEXA window_class;
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = yaat_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = YAAT_WINDOW_CLASS_NAME;

    if (RegisterClassExA(&window_class) == 0) {
        return 1;
    }

    window = CreateWindowExA(0,
                             YAAT_WINDOW_CLASS_NAME,
                             YAAT_WINDOW_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             640,
                             480,
                             0,
                             0,
                             instance,
                             0);
    if (window == 0) {
        return 1;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    while (GetMessageA(&message, 0, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return (int)message.wParam;
}
