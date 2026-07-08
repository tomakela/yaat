#ifndef YAAT_PLATFORM_WIN32_GDI_RENDERER_H
#define YAAT_PLATFORM_WIN32_GDI_RENDERER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YAAT_GDI_PIXEL_FORMAT_BGRX32 1

typedef struct YaatGdiRenderer {
    HDC memory_dc;
    HBITMAP dib_bitmap;
    HBITMAP previous_bitmap;
    BITMAPINFO bitmap_info;
    void *pixels;
    int width;
    int height;
    int pitch;
} YaatGdiRenderer;

int yaat_gdi_renderer_init(YaatGdiRenderer *renderer, HDC reference_dc,
                           int width, int height);
void yaat_gdi_renderer_shutdown(YaatGdiRenderer *renderer);
void yaat_gdi_renderer_clear(YaatGdiRenderer *renderer, unsigned long bgrx_color);
int yaat_gdi_renderer_present(YaatGdiRenderer *renderer, HDC target_dc,
                              int dst_x, int dst_y);
int yaat_gdi_renderer_present_stretched(YaatGdiRenderer *renderer, HDC target_dc,
                                        int dst_x, int dst_y,
                                        int dst_width, int dst_height);

#ifdef __cplusplus
}
#endif

#endif
