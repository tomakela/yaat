#include "bitmap_assets.h"

#ifdef YAAT_EMBEDDED_MODULE
static void yaat_unload_bitmap(YaatBitmap *bitmap)
{
    if (bitmap->pixels != 0) {
        free(bitmap->pixels);
    }
    bitmap->pixels = 0;
    bitmap->width = 0;
    bitmap->height = 0;
    bitmap->has_alpha = 0;
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
    int has_alpha;

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

    has_alpha = 0;
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
                unsigned long a = bits_per_pixel == 32 ? src[3] : 0xffUL;
                if (bits_per_pixel == 32 && a != 0) has_alpha = 1;
                pixels[(dst_y * (int)bmp_width) + x] = b | (g << 8) | (r << 16) | (a << 24);
            }
        }
    }

    free(data);
    yaat_unload_bitmap(bitmap);
    bitmap->pixels = pixels;
    bitmap->width = (int)bmp_width;
    bitmap->height = (int)bmp_height;
    bitmap->has_alpha = has_alpha;
    yaat_copy(bitmap->path, sizeof(bitmap->path), path, strlen(path));
    return 1;
}

static void yaat_blend_pixel(unsigned long *dst, unsigned long src)
{
    unsigned long alpha;
    unsigned long inv;
    unsigned long rb;
    unsigned long g;

    alpha = (src >> 24) & 0xffUL;
    if (alpha == 0) return;
    if (alpha == 0xffUL) {
        *dst = src & 0x00ffffffUL;
        return;
    }
    inv = 255UL - alpha;
    rb = (((src & 0x00ff00ffUL) * alpha + (*dst & 0x00ff00ffUL) * inv) >> 8) & 0x00ff00ffUL;
    g = (((src & 0x0000ff00UL) * alpha + (*dst & 0x0000ff00UL) * inv) >> 8) & 0x0000ff00UL;
    *dst = rb | g;
}

static void yaat_draw_bitmap_keyed(YaatBitmap *bitmap, int dst_x, int dst_y,
                                   int transparent_color_enabled,
                                   unsigned long transparent_color)
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
    if (dst_x < 0) { src_x0 = -dst_x; copy_width -= src_x0; dst_x = 0; }
    if (dst_y < 0) { src_y0 = -dst_y; copy_height -= src_y0; dst_y = 0; }
    if (dst_x + copy_width > g_renderer.width) copy_width = g_renderer.width - dst_x;
    if (dst_y + copy_height > g_renderer.height) copy_height = g_renderer.height - dst_y;
    if (copy_width <= 0 || copy_height <= 0) {
        return;
    }

    transparent_color &= 0x00ffffffUL;
    for (y = 0; y < copy_height; ++y) {
        unsigned long *dst_row = (unsigned long *)
            ((unsigned char *)g_renderer.pixels + ((dst_y + y) * g_renderer.pitch)) + dst_x;
        unsigned long *src_row =
            bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0;
        int x;

        if (!transparent_color_enabled) {
            memcpy(dst_row, src_row, (size_t)copy_width * sizeof(unsigned long));
            continue;
        }
        for (x = 0; x < copy_width; ++x) {
            if ((src_row[x] & 0x00ffffffUL) != transparent_color) {
                dst_row[x] = src_row[x];
            }
        }
    }
}

static void yaat_draw_bitmap_transparent(YaatBitmap *bitmap, int dst_x, int dst_y,
                                         const YaatTransparency *transparency,
                                         const char *mask_base_path)
{
    YaatBitmap mask_bitmap;
    const YaatBitmap *mask;
    char mask_path[YAAT_ASSET_MAX_PATH * 2];
    int src_x0;
    int src_y0;
    int copy_width;
    int copy_height;
    int x;
    int y;
    YaatTransparencyMode mode;
    unsigned long color_key;

    if (bitmap == 0 || bitmap->pixels == 0) return;

    mode = transparency != 0 ? transparency->mode : YAAT_TRANSPARENCY_ALPHA;
    color_key = transparency != 0 ? transparency->color_key : 0x00ff00ffUL;
    memset(&mask_bitmap, 0, sizeof(mask_bitmap));
    mask = 0;
    if (transparency != 0 && transparency->mask[0] != '\0') {
        if (mask_base_path != 0 && mask_base_path[0] != '\0') {
            yaat_runtime_join_path(mask_path, sizeof(mask_path), mask_base_path,
                                   transparency->mask);
        } else {
            yaat_copy(mask_path, sizeof(mask_path), transparency->mask,
                      strlen(transparency->mask));
        }
        if (yaat_load_bmp(&mask_bitmap, mask_path)) {
            mask = &mask_bitmap;
            mode = YAAT_TRANSPARENCY_MASK;
        }
    }

    src_x0 = 0;
    src_y0 = 0;
    copy_width = bitmap->width;
    copy_height = bitmap->height;
    if (dst_x < 0) { src_x0 = -dst_x; copy_width -= src_x0; dst_x = 0; }
    if (dst_y < 0) { src_y0 = -dst_y; copy_height -= src_y0; dst_y = 0; }
    if (dst_x + copy_width > g_renderer.width) copy_width = g_renderer.width - dst_x;
    if (dst_y + copy_height > g_renderer.height) copy_height = g_renderer.height - dst_y;
    if (copy_width <= 0 || copy_height <= 0) {
        yaat_unload_bitmap(&mask_bitmap);
        return;
    }

    if (mode == YAAT_TRANSPARENCY_NONE) {
        for (y = 0; y < copy_height; ++y) {
            memcpy((unsigned char *)g_renderer.pixels + ((dst_y + y) * g_renderer.pitch) +
                       ((size_t)dst_x * sizeof(unsigned long)),
                   bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0,
                   (size_t)copy_width * sizeof(unsigned long));
        }
        yaat_unload_bitmap(&mask_bitmap);
        return;
    }

    for (y = 0; y < copy_height; ++y) {
        unsigned long *dst = (unsigned long *)((unsigned char *)g_renderer.pixels +
                             ((dst_y + y) * g_renderer.pitch)) + dst_x;
        unsigned long *src = bitmap->pixels + ((src_y0 + y) * bitmap->width) + src_x0;

        for (x = 0; x < copy_width; ++x) {
            unsigned long pixel = src[x];
            int draw = 1;

            if (mode == YAAT_TRANSPARENCY_COLOR_KEY &&
                (pixel & 0x00ffffffUL) == color_key) {
                draw = 0;
            }
            if (mode == YAAT_TRANSPARENCY_MASK) {
                if (mask == 0 || src_x0 + x >= mask->width || src_y0 + y >= mask->height ||
                    ((mask->pixels[((src_y0 + y) * mask->width) + src_x0 + x] & 0x00ffffffUL) == 0)) {
                    draw = 0;
                }
            }
            if (draw) {
                if (mode == YAAT_TRANSPARENCY_ALPHA && bitmap->has_alpha) {
                    yaat_blend_pixel(&dst[x], pixel);
                } else {
                    dst[x] = pixel & 0x00ffffffUL;
                }
            }
        }
    }
    yaat_unload_bitmap(&mask_bitmap);
}


static void yaat_draw_bitmap_transparent_scaled(YaatBitmap *bitmap, int dst_x, int dst_y,
                                                int dst_width, int dst_height,
                                                const YaatTransparency *transparency,
                                                const char *mask_base_path)
{
    YaatBitmap mask_bitmap;
    const YaatBitmap *mask;
    char mask_path[YAAT_ASSET_MAX_PATH * 2];
    int x;
    int y;
    YaatTransparencyMode mode;
    unsigned long color_key;

    if (bitmap == 0 || bitmap->pixels == 0 || dst_width <= 0 || dst_height <= 0) return;
    mode = transparency != 0 ? transparency->mode : YAAT_TRANSPARENCY_ALPHA;
    color_key = transparency != 0 ? transparency->color_key : 0x00ff00ffUL;
    memset(&mask_bitmap, 0, sizeof(mask_bitmap));
    mask = 0;
    if (transparency != 0 && transparency->mask[0] != '\0') {
        if (mask_base_path != 0 && mask_base_path[0] != '\0') {
            yaat_runtime_join_path(mask_path, sizeof(mask_path), mask_base_path,
                                   transparency->mask);
        } else {
            yaat_copy(mask_path, sizeof(mask_path), transparency->mask,
                      strlen(transparency->mask));
        }
        if (yaat_load_bmp(&mask_bitmap, mask_path)) {
            mask = &mask_bitmap;
            mode = YAAT_TRANSPARENCY_MASK;
        }
    }

    for (y = 0; y < dst_height; ++y) {
        int screen_y = dst_y + y;
        int src_y = (y * bitmap->height) / dst_height;
        if (screen_y < 0 || screen_y >= g_renderer.height) continue;
        for (x = 0; x < dst_width; ++x) {
            int screen_x = dst_x + x;
            int src_x = (x * bitmap->width) / dst_width;
            unsigned long pixel;
            int draw;
            unsigned long *dst;

            if (screen_x < 0 || screen_x >= g_renderer.width) continue;
            pixel = bitmap->pixels[(src_y * bitmap->width) + src_x];
            draw = 1;
            if (mode == YAAT_TRANSPARENCY_COLOR_KEY &&
                (pixel & 0x00ffffffUL) == color_key) {
                draw = 0;
            }
            if (mode == YAAT_TRANSPARENCY_MASK) {
                if (mask == 0 || src_x >= mask->width || src_y >= mask->height ||
                    ((mask->pixels[(src_y * mask->width) + src_x] & 0x00ffffffUL) == 0)) {
                    draw = 0;
                }
            }
            if (!draw) continue;
            dst = (unsigned long *)((unsigned char *)g_renderer.pixels +
                  (screen_y * g_renderer.pitch)) + screen_x;
            if (mode == YAAT_TRANSPARENCY_ALPHA && bitmap->has_alpha) {
                yaat_blend_pixel(dst, pixel);
            } else {
                *dst = pixel & 0x00ffffffUL;
            }
        }
    }
    yaat_unload_bitmap(&mask_bitmap);
}

static void yaat_draw_bitmap(YaatBitmap *bitmap, int dst_x, int dst_y)
{
    yaat_draw_bitmap_keyed(bitmap, dst_x, dst_y, 0, 0);
}

static void yaat_draw_bitmap_region(YaatBitmap *bitmap, int dst_x, int dst_y,
                                    int src_x, int src_y, int width,
                                    int height)
{
    int copy_width;
    int copy_height;
    int y;

    if (bitmap == 0 || bitmap->pixels == 0) {
        return;
    }
    if (width <= 0 || height <= 0) {
        src_x = 0;
        src_y = 0;
        width = bitmap->width;
        height = bitmap->height;
    }
    if (src_x < 0) {
        dst_x -= src_x;
        width += src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        dst_y -= src_y;
        height += src_y;
        src_y = 0;
    }
    if (src_x + width > bitmap->width) width = bitmap->width - src_x;
    if (src_y + height > bitmap->height) height = bitmap->height - src_y;
    if (dst_x < 0) {
        src_x -= dst_x;
        width += dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y -= dst_y;
        height += dst_y;
        dst_y = 0;
    }
    copy_width = width;
    copy_height = height;
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
               bitmap->pixels + ((src_y + y) * bitmap->width) + src_x,
               (size_t)copy_width * sizeof(unsigned long));
    }
}


#endif
