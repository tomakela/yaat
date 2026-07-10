#include "runtime/asset_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAAT_ARCHIVE_MAGIC "YAATDAT1"
#define YAAT_LINE_MAX 256
#define YAAT_MAX_PATCH_NUMBER 9999

static void copy_string(char *dst, int dst_size, const char *src)
{
    int i;
    if (dst == 0 || dst_size <= 0) return;
    if (src == 0) { dst[0] = '\0'; return; }
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void set_error(YaatAssetReadResult *result, const char *message,
                      const char *path)
{
    result->ok = 0;
    result->size = 0;
    result->data[0] = '\0';
    if (path != 0 && path[0] != '\0') {
        sprintf(result->error, "%s: %s", message, path);
    } else {
        copy_string(result->error, sizeof(result->error), message);
    }
}

static void join_path(char *dst, int dst_size, const char *left,
                      const char *right)
{
    int len;
    copy_string(dst, dst_size, left);
    len = (int)strlen(dst);
    if (len > 0 && len < dst_size - 1 && dst[len - 1] != '/' &&
        dst[len - 1] != '\\') {
        dst[len] = '/';
        dst[len + 1] = '\0';
    }
    if ((int)strlen(dst) < dst_size - 1) {
        strncat(dst, right, dst_size - 1 - (int)strlen(dst));
    }
}

void yaat_asset_store_init(YaatAssetStore *store, const char *root)
{
    if (store == 0) return;
    copy_string(store->root, sizeof(store->root), root == 0 ? "." : root);
}

int yaat_asset_normalize_path(const char *input, char *output, int output_size)
{
    char temp[YAAT_ASSET_STORE_MAX_PATH];
    int i;
    int out;
    int seg_start;

    if (input == 0 || output == 0 || output_size <= 0 || input[0] == '\0') return 0;
    output[0] = '\0';
    if (input[0] == '/' || input[0] == '\\') return 0;
    if (isalpha((unsigned char)input[0]) && input[1] == ':') return 0;

    out = 0;
    for (i = 0; input[i] != '\0'; ++i) {
        char ch = input[i] == '\\' ? '/' : input[i];
        if ((unsigned char)ch < 32 || ch == ':') return 0;
        if (out >= (int)sizeof(temp) - 1) return 0;
        temp[out++] = ch;
    }
    temp[out] = '\0';

    out = 0;
    i = 0;
    while (temp[i] != '\0') {
        while (temp[i] == '/') ++i;
        if (temp[i] == '\0') break;
        seg_start = i;
        while (temp[i] != '\0' && temp[i] != '/') ++i;
        if (i - seg_start == 1 && temp[seg_start] == '.') continue;
        if (i - seg_start == 2 && temp[seg_start] == '.' && temp[seg_start + 1] == '.') return 0;
        if (out != 0) {
            if (out >= output_size - 1) return 0;
            output[out++] = '/';
        }
        while (seg_start < i) {
            if (out >= output_size - 1) return 0;
            output[out++] = temp[seg_start++];
        }
    }
    if (out == 0) return 0;
    output[out] = '\0';
    return 1;
}

static int read_loose(const char *path, YaatAssetReadResult *result)
{
    FILE *fp = fopen(path, "rb");
    long size;
    if (fp == 0) return 0;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0 || size > YAAT_ASSET_STORE_MAX_DATA) {
        fclose(fp);
        set_error(result, "Asset is too large", path);
        return 1;
    }
    result->size = (int)fread(result->data, 1, (size_t)size, fp);
    if (result->size != size || ferror(fp)) {
        fclose(fp);
        set_error(result, "Could not read asset", path);
        return 1;
    }
    fclose(fp);
    result->data[result->size] = '\0';
    result->ok = 1;
    result->error[0] = '\0';
    copy_string(result->source, sizeof(result->source), path);
    return 1;
}

static int read_archive(const char *archive_path, const char *logical,
                        YaatAssetReadResult *result)
{
    FILE *fp = fopen(archive_path, "rb");
    char line[YAAT_LINE_MAX];
    int found = 0;
    if (fp == 0) return 0;
    if (fgets(line, sizeof(line), fp) == 0 || strncmp(line, YAAT_ARCHIVE_MAGIC, 8) != 0) {
        fclose(fp);
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != 0) {
        char entry_path[YAAT_ASSET_STORE_MAX_PATH];
        char normalized[YAAT_ASSET_STORE_MAX_PATH];
        char *colon;
        int size;
        colon = strrchr(line, ':');
        if (colon == 0) break;
        *colon = '\0';
        size = atoi(colon + 1);
        if (size < 0 || size > YAAT_ASSET_STORE_MAX_DATA) break;
        copy_string(entry_path, sizeof(entry_path), line);
        if (yaat_asset_normalize_path(entry_path, normalized, sizeof(normalized)) &&
            strcmp(normalized, logical) == 0) {
            result->size = (int)fread(result->data, 1, (size_t)size, fp);
            if (result->size != size || ferror(fp)) {
                set_error(result, "Could not read archived asset", archive_path);
                fclose(fp);
                return 1;
            }
            result->data[result->size] = '\0';
            result->ok = 1;
            result->error[0] = '\0';
            copy_string(result->source, sizeof(result->source), archive_path);
            found = 1;
        } else {
            fseek(fp, size, SEEK_CUR);
        }
        fgetc(fp);
    }
    fclose(fp);
    return found;
}

void yaat_asset_store_read(YaatAssetStore *store, const char *asset_path,
                           YaatAssetReadResult *result)
{
    char logical[YAAT_ASSET_STORE_MAX_PATH];
    char full[YAAT_ASSET_STORE_MAX_ROOT];
    char archive[YAAT_ASSET_STORE_MAX_ROOT];
    int patch;

    if (result == 0) return;
    memset(result, 0, sizeof(*result));
    if (store == 0 || !yaat_asset_normalize_path(asset_path, logical, sizeof(logical))) {
        set_error(result, "Unsafe asset path", asset_path);
        return;
    }

    join_path(full, sizeof(full), store->root, "game");
    join_path(full, sizeof(full), full, logical);
    if (read_loose(full, result)) return;

    for (patch = YAAT_MAX_PATCH_NUMBER; patch >= 0; --patch) {
        char name[32];
        sprintf(name, "patch%04d.dat", patch);
        join_path(archive, sizeof(archive), store->root, name);
        if (read_archive(archive, logical, result)) return;
    }
    join_path(archive, sizeof(archive), store->root, "game.dat");
    if (read_archive(archive, logical, result)) return;

    set_error(result, "Missing asset", logical);
}
