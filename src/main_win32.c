#include <windows.h>

#include "platform/win32/gdi_renderer.h"

#define YAAT_WINDOW_CLASS_NAME "YAATWindowClass"
#define YAAT_WINDOW_TITLE "YAAT"

static LRESULT CALLBACK yaat_window_proc(HWND window, UINT message,
                                         WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
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

/*
 * Rendering is not wired into the window procedure yet. The engine loop is
 * expected to initialize YaatGdiRenderer after a window device context is
 * available, draw software pixels into the CreateDIBSection memory, and present
 * with BitBlt or StretchBlt during WM_PAINT or a fixed-timestep pump.
 */
