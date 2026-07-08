#include <windows.h>

static const char g_class_name[] = "YAATWin95Smoke";

static LRESULT CALLBACK smoke_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
{
    WNDCLASSEXA wc;
    HWND hwnd;
    MSG msg;

    (void)prev_instance;
    (void)cmd_line;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = smoke_wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = g_class_name;

    if (!RegisterClassExA(&wc)) {
        return 1;
    }

    hwnd = CreateWindowExA(
        0,
        g_class_name,
        "YAAT Win95 Smoke Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        200,
        NULL,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL) {
        return 2;
    }

    ShowWindow(hwnd, show_cmd);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
