#include "gdi_renderer.h"

#include <string.h>

static void yaat_gdi_renderer_reset(YaatGdiRenderer *renderer)
{
    if (renderer != 0) {
        memset(renderer, 0, sizeof(*renderer));
    }
}

int yaat_gdi_renderer_init(YaatGdiRenderer *renderer, HDC reference_dc,
                           int width, int height)
{
    HDC source_dc;

    if (renderer == 0 || width <= 0 || height <= 0) {
        return 0;
    }

    yaat_gdi_renderer_reset(renderer);

    renderer->width = width;
    renderer->height = height;
    renderer->pitch = width * 4;

    renderer->bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    renderer->bitmap_info.bmiHeader.biWidth = width;
    renderer->bitmap_info.bmiHeader.biHeight = -height;
    renderer->bitmap_info.bmiHeader.biPlanes = 1;
    renderer->bitmap_info.bmiHeader.biBitCount = 32;
    renderer->bitmap_info.bmiHeader.biCompression = BI_RGB;
    renderer->bitmap_info.bmiHeader.biSizeImage = (DWORD)(renderer->pitch * height);

    source_dc = reference_dc;
    renderer->memory_dc = CreateCompatibleDC(source_dc);
    if (renderer->memory_dc == 0) {
        yaat_gdi_renderer_shutdown(renderer);
        return 0;
    }

    renderer->dib_bitmap = CreateDIBSection(source_dc,
                                            &renderer->bitmap_info,
                                            DIB_RGB_COLORS,
                                            &renderer->pixels,
                                            0,
                                            0);
    if (renderer->dib_bitmap == 0 || renderer->pixels == 0) {
        yaat_gdi_renderer_shutdown(renderer);
        return 0;
    }

    renderer->previous_bitmap = (HBITMAP)SelectObject(renderer->memory_dc,
                                                     renderer->dib_bitmap);
    if (renderer->previous_bitmap == 0 || renderer->previous_bitmap == HGDI_ERROR) {
        yaat_gdi_renderer_shutdown(renderer);
        return 0;
    }

    return 1;
}

void yaat_gdi_renderer_shutdown(YaatGdiRenderer *renderer)
{
    if (renderer == 0) {
        return;
    }

    if (renderer->memory_dc != 0 && renderer->previous_bitmap != 0 &&
        renderer->previous_bitmap != HGDI_ERROR) {
        SelectObject(renderer->memory_dc, renderer->previous_bitmap);
    }

    if (renderer->dib_bitmap != 0) {
        DeleteObject(renderer->dib_bitmap);
    }

    if (renderer->memory_dc != 0) {
        DeleteDC(renderer->memory_dc);
    }

    yaat_gdi_renderer_reset(renderer);
}

void yaat_gdi_renderer_clear(YaatGdiRenderer *renderer, unsigned long bgrx_color)
{
    unsigned long *row;
    int x;
    int y;

    if (renderer == 0 || renderer->pixels == 0) {
        return;
    }

    for (y = 0; y < renderer->height; ++y) {
        row = (unsigned long *)((unsigned char *)renderer->pixels +
                               (y * renderer->pitch));
        for (x = 0; x < renderer->width; ++x) {
            row[x] = bgrx_color;
        }
    }
}

int yaat_gdi_renderer_present(YaatGdiRenderer *renderer, HDC target_dc,
                              int dst_x, int dst_y)
{
    if (renderer == 0 || renderer->memory_dc == 0 || target_dc == 0) {
        return 0;
    }

    return BitBlt(target_dc, dst_x, dst_y, renderer->width, renderer->height,
                  renderer->memory_dc, 0, 0, SRCCOPY) != 0;
}

int yaat_gdi_renderer_present_stretched(YaatGdiRenderer *renderer, HDC target_dc,
                                        int dst_x, int dst_y,
                                        int dst_width, int dst_height)
{
    if (renderer == 0 || renderer->memory_dc == 0 || target_dc == 0 ||
        dst_width <= 0 || dst_height <= 0) {
        return 0;
    }

    return StretchBlt(target_dc, dst_x, dst_y, dst_width, dst_height,
                      renderer->memory_dc, 0, 0,
                      renderer->width, renderer->height, SRCCOPY) != 0;
}

/*
 * Palette and low-color display handling is intentionally not implemented in
 * this first skeleton. A complete Win95 path should detect indexed display
 * modes, realize a suitable logical palette, and dither or quantize the 32-bit
 * BGRX backbuffer before presentation when the system cannot display true color.
 */
