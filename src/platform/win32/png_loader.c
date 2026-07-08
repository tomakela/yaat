#include "png_loader.h"

/*
 * This module is intentionally dependency-isolated:
 * - No file I/O is performed here; callers pass a complete PNG memory buffer.
 * - No Win32 APIs are needed, so there is no Unicode/ANSI API surface here.
 * - Decoder-library symbols stay private to this translation unit when enabled.
 *
 * Future YAAT_USE_SPNG integration should keep external decoder references such
 * as spng_ctx, spng_decode_image, and mz_uncompress in this file only.  Do not
 * expose spng or miniz headers, types, or constants through png_loader.h.
 */

static yaat_png_error yaat_png_fail(yaat_png_error error_code,
                                    unsigned long *width_out,
                                    unsigned long *height_out)
{
    if (width_out != 0) {
        *width_out = 0;
    }
    if (height_out != 0) {
        *height_out = 0;
    }
    return error_code;
}

static yaat_png_error yaat_validate_png_arguments(const unsigned char *png_data,
                                                  unsigned long png_size,
                                                  unsigned char *rgba_out,
                                                  unsigned long rgba_out_size,
                                                  unsigned long *width_out,
                                                  unsigned long *height_out)
{
    (void)rgba_out_size;

    if (width_out != 0) {
        *width_out = 0;
    }
    if (height_out != 0) {
        *height_out = 0;
    }

    if (png_data == 0 || png_size == 0 || rgba_out == 0 ||
        width_out == 0 || height_out == 0) {
        return YAAT_PNG_ERROR_INVALID_ARGUMENT;
    }

    return YAAT_PNG_OK;
}

#if defined(YAAT_USE_RUNTIME_PNG) && defined(YAAT_USE_SPNG)

/*
 * Runtime PNG decoding is enabled and the selected backend is spng.
 *
 * The real implementation will include private spng/miniz headers here and map
 * their errors into yaat_png_error values.  Keep references to decoder symbols
 * including spng_ctx, spng_decode_image, and mz_uncompress confined to this
 * translation unit.
 */
static yaat_png_error yaat_load_png_rgba8_spng(const unsigned char *png_data,
                                               unsigned long png_size,
                                               unsigned char *rgba_out,
                                               unsigned long rgba_out_size,
                                               unsigned long *width_out,
                                               unsigned long *height_out)
{
    (void)png_data;
    (void)png_size;
    (void)rgba_out;
    (void)rgba_out_size;
    return yaat_png_fail(YAAT_PNG_ERROR_UNSUPPORTED_FORMAT,
                         width_out,
                         height_out);
}

#elif defined(YAAT_USE_RUNTIME_PNG)

/* Runtime PNG was requested, but no concrete backend has been selected. */
static yaat_png_error yaat_load_png_rgba8_runtime_unconfigured(
    const unsigned char *png_data,
    unsigned long png_size,
    unsigned char *rgba_out,
    unsigned long rgba_out_size,
    unsigned long *width_out,
    unsigned long *height_out)
{
    (void)png_data;
    (void)png_size;
    (void)rgba_out;
    (void)rgba_out_size;
    return yaat_png_fail(YAAT_PNG_ERROR_UNSUPPORTED_FORMAT,
                         width_out,
                         height_out);
}

#else

/* Default build: runtime PNG decoding is deliberately disabled. */
static yaat_png_error yaat_load_png_rgba8_disabled(const unsigned char *png_data,
                                                   unsigned long png_size,
                                                   unsigned char *rgba_out,
                                                   unsigned long rgba_out_size,
                                                   unsigned long *width_out,
                                                   unsigned long *height_out)
{
    (void)png_data;
    (void)png_size;
    (void)rgba_out;
    (void)rgba_out_size;
    return yaat_png_fail(YAAT_PNG_ERROR_UNSUPPORTED_FORMAT,
                         width_out,
                         height_out);
}

#endif

yaat_png_error yaat_load_png_rgba8(const unsigned char *png_data,
                                  unsigned long png_size,
                                  unsigned char *rgba_out,
                                  unsigned long rgba_out_size,
                                  unsigned long *width_out,
                                  unsigned long *height_out)
{
    yaat_png_error argument_error;

    argument_error = yaat_validate_png_arguments(png_data,
                                                 png_size,
                                                 rgba_out,
                                                 rgba_out_size,
                                                 width_out,
                                                 height_out);
    if (argument_error != YAAT_PNG_OK) {
        return argument_error;
    }

#if defined(YAAT_USE_RUNTIME_PNG) && defined(YAAT_USE_SPNG)
    return yaat_load_png_rgba8_spng(png_data,
                                    png_size,
                                    rgba_out,
                                    rgba_out_size,
                                    width_out,
                                    height_out);
#elif defined(YAAT_USE_RUNTIME_PNG)
    return yaat_load_png_rgba8_runtime_unconfigured(png_data,
                                                    png_size,
                                                    rgba_out,
                                                    rgba_out_size,
                                                    width_out,
                                                    height_out);
#else
    return yaat_load_png_rgba8_disabled(png_data,
                                        png_size,
                                        rgba_out,
                                        rgba_out_size,
                                        width_out,
                                        height_out);
#endif
}

const char *yaat_png_error_string(yaat_png_error error_code)
{
    switch (error_code) {
    case YAAT_PNG_OK:
        return "ok";
    case YAAT_PNG_ERROR_INVALID_ARGUMENT:
        return "invalid argument";
    case YAAT_PNG_ERROR_UNSUPPORTED_FORMAT:
        return "unsupported PNG runtime decoder";
    case YAAT_PNG_ERROR_DECODE_FAILED:
        return "PNG decode failed";
    case YAAT_PNG_ERROR_OUT_OF_MEMORY:
        return "out of memory";
    case YAAT_PNG_ERROR_OUTPUT_TOO_SMALL:
        return "output buffer too small";
    default:
        return "unknown PNG error";
    }
}
