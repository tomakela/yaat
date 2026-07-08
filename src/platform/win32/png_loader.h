#ifndef YAAT_PLATFORM_WIN32_PNG_LOADER_H
#define YAAT_PLATFORM_WIN32_PNG_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Engine-local PNG decode result codes.  The loader accepts only memory
 * buffers; file ownership and I/O stay with higher-level asset code.
 */
typedef enum yaat_png_error {
    YAAT_PNG_OK = 0,
    YAAT_PNG_ERROR_INVALID_ARGUMENT = 1,
    YAAT_PNG_ERROR_UNSUPPORTED_FORMAT = 2,
    YAAT_PNG_ERROR_DECODE_FAILED = 3,
    YAAT_PNG_ERROR_OUT_OF_MEMORY = 4,
    YAAT_PNG_ERROR_OUTPUT_TOO_SMALL = 5
} yaat_png_error;

/*
 * Decode a PNG memory buffer into caller-owned RGBA8 storage.
 *
 * Parameters:
 * - png_data/png_size: complete PNG bytes already loaded by the caller.
 * - rgba_out/rgba_out_size: caller-owned destination buffer.
 * - width_out/height_out: receive decoded dimensions on success.
 *
 * The decoder never performs file I/O and never allocates the output image.
 * On failure width_out and height_out are set to zero when provided.
 */
yaat_png_error yaat_load_png_rgba8(const unsigned char *png_data,
                                  unsigned long png_size,
                                  unsigned char *rgba_out,
                                  unsigned long rgba_out_size,
                                  unsigned long *width_out,
                                  unsigned long *height_out);

const char *yaat_png_error_string(yaat_png_error error_code);

#ifdef __cplusplus
}
#endif

#endif /* YAAT_PLATFORM_WIN32_PNG_LOADER_H */
