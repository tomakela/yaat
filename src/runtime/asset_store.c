#include "runtime/asset_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void yaat_asset_store_clear(YaatAssetStore *store)
{
    if (store == 0) {
        return;
    }
    store->base_dir[0] = '\0';
    store->initialized = 0;
}

static int yaat_asset_store_copy(char *dst, int dst_size, const char *src)
{
    int i;

    if (dst == 0 || dst_size <= 0 || src == 0) {
        return YAAT_ASSET_STORE_ERROR;
    }
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    if (src[i] != '\0') {
        return YAAT_ASSET_STORE_ERROR;
    }
    return YAAT_ASSET_STORE_OK;
}

static int yaat_asset_is_slash(char ch)
{
    return ch == '/' || ch == '\\';
}

static int yaat_asset_has_drive_letter(const char *path)
{
    if (path == 0) {
        return 0;
    }
    return isalpha((unsigned char)path[0]) && path[1] == ':';
}

static int yaat_asset_normalize_path(const char *logical_path, char *out,
                                     int out_size)
{
    int in_index;
    int out_index;
    int segment_start;
    int segment_len;
    char ch;

    if (logical_path == 0 || out == 0 || out_size <= 0) {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    if (logical_path[0] == '\0' || yaat_asset_is_slash(logical_path[0]) ||
        yaat_asset_has_drive_letter(logical_path)) {
        return YAAT_ASSET_STORE_BAD_PATH;
    }

    in_index = 0;
    out_index = 0;
    segment_start = 0;
    segment_len = 0;
    while (logical_path[in_index] != '\0') {
        ch = logical_path[in_index];
        if (ch == ':') {
            return YAAT_ASSET_STORE_BAD_PATH;
        }
        if (yaat_asset_is_slash(ch)) {
            if (segment_len == 0) {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            if (segment_len == 1 && out[segment_start] == '.') {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            if (segment_len == 2 && out[segment_start] == '.' &&
                out[segment_start + 1] == '.') {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            if (out_index >= out_size - 1) {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            out[out_index++] = '/';
            segment_start = out_index;
            segment_len = 0;
        } else {
            if ((unsigned char)ch < 32) {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            if (out_index >= out_size - 1) {
                return YAAT_ASSET_STORE_BAD_PATH;
            }
            out[out_index++] = (char)tolower((unsigned char)ch);
            ++segment_len;
        }
        ++in_index;
    }

    if (segment_len == 0) {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    if (segment_len == 1 && out[segment_start] == '.') {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    if (segment_len == 2 && out[segment_start] == '.' &&
        out[segment_start + 1] == '.') {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    out[out_index] = '\0';
    return YAAT_ASSET_STORE_OK;
}

static int yaat_asset_join_path(const YaatAssetStore *store,
                                const char *logical_path, char *out,
                                int out_size)
{
    char normalized[YAAT_ASSET_STORE_MAX_PATH];
    int result;
    int len;

    if (store == 0 || !store->initialized) {
        return YAAT_ASSET_STORE_ERROR;
    }
    result = yaat_asset_normalize_path(logical_path, normalized,
                                       sizeof(normalized));
    if (result != YAAT_ASSET_STORE_OK) {
        return result;
    }
    result = yaat_asset_store_copy(out, out_size, store->base_dir);
    if (result != YAAT_ASSET_STORE_OK) {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    len = (int)strlen(out);
    if (len > 0 && !yaat_asset_is_slash(out[len - 1])) {
        if (len >= out_size - 1) {
            return YAAT_ASSET_STORE_BAD_PATH;
        }
        out[len] = '/';
        out[len + 1] = '\0';
    }
    if ((int)strlen(out) + (int)strlen(normalized) >= out_size) {
        return YAAT_ASSET_STORE_BAD_PATH;
    }
    strcat(out, normalized);
    return YAAT_ASSET_STORE_OK;
}

int yaat_asset_store_init(YaatAssetStore *store, const char *base_dir)
{
    int len;

    if (store == 0 || base_dir == 0 || base_dir[0] == '\0') {
        return YAAT_ASSET_STORE_ERROR;
    }
    yaat_asset_store_clear(store);
    if (yaat_asset_store_copy(store->base_dir, sizeof(store->base_dir),
                              base_dir) != YAAT_ASSET_STORE_OK) {
        yaat_asset_store_clear(store);
        return YAAT_ASSET_STORE_ERROR;
    }
    len = (int)strlen(store->base_dir);
    while (len > 1 && yaat_asset_is_slash(store->base_dir[len - 1])) {
        store->base_dir[len - 1] = '\0';
        --len;
    }
    store->initialized = 1;
    return YAAT_ASSET_STORE_OK;
}

void yaat_asset_store_shutdown(YaatAssetStore *store)
{
    yaat_asset_store_clear(store);
}

int yaat_asset_read_all(YaatAssetStore *store, const char *logical_path,
                        unsigned char **bytes, unsigned long *size)
{
    char path[YAAT_ASSET_STORE_MAX_PATH * 2];
    FILE *file;
    long file_size;
    unsigned char *buffer;
    size_t read_count;
    int result;

    if (bytes == 0 || size == 0) {
        return YAAT_ASSET_STORE_ERROR;
    }
    *bytes = 0;
    *size = 0;

    result = yaat_asset_join_path(store, logical_path, path, sizeof(path));
    if (result != YAAT_ASSET_STORE_OK) {
        return result;
    }

    file = fopen(path, "rb");
    if (file == 0) {
        return YAAT_ASSET_STORE_NOT_FOUND;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return YAAT_ASSET_STORE_ERROR;
    }
    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return YAAT_ASSET_STORE_ERROR;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return YAAT_ASSET_STORE_ERROR;
    }

    buffer = (unsigned char *)malloc((size_t)file_size + 1);
    if (buffer == 0) {
        fclose(file);
        return YAAT_ASSET_STORE_NO_MEMORY;
    }
    read_count = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    if (read_count != (size_t)file_size) {
        free(buffer);
        return YAAT_ASSET_STORE_ERROR;
    }
    buffer[file_size] = '\0';
    *bytes = buffer;
    *size = (unsigned long)file_size;
    return YAAT_ASSET_STORE_OK;
}

int yaat_asset_exists(YaatAssetStore *store, const char *logical_path)
{
    char path[YAAT_ASSET_STORE_MAX_PATH * 2];
    FILE *file;
    int result;

    result = yaat_asset_join_path(store, logical_path, path, sizeof(path));
    if (result != YAAT_ASSET_STORE_OK) {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    fclose(file);
    return 1;
}
