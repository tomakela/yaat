/*
 * Offline YAAT asset packer.
 *
 * Produces a ZIP-format .dat archive from a loose asset directory. This is a
 * modern host-only development tool and has no runtime engine dependencies.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_PATH_BUF 1024
#define READ_BUF_SIZE 32768

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

typedef struct PackedFile {
    char *logical_path;
    char *disk_path;
    uint32_t crc32;
    uint32_t size;
    uint32_t local_header_offset;
} PackedFile;

typedef struct FileList {
    PackedFile *items;
    size_t count;
    size_t capacity;
} FileList;

static uint32_t crc_table[256];

static char *xstrdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

static void join_path(char *out, size_t out_size, const char *a, const char *b) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '/' || a[len - 1] == '\\')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s%c%s", a, PATH_SEP, b);
    }
}

static int is_supported_component(const char *name) {
    const unsigned char *p;
    if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    for (p = (const unsigned char *)name; *p != '\0'; p++) {
        if (*p > 127) {
            return 0;
        }
        if (!(islower(*p) || isdigit(*p) || *p == '_' || *p == '-' || *p == '.')) {
            return 0;
        }
    }
    return 1;
}

static void crc32_init(void) {
    uint32_t i, j;
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (j = 0; j < 8; j++) {
            c = (c & 1U) ? (0xedb88320U ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
}

static uint32_t crc32_update(uint32_t crc, const unsigned char *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0xffU] ^ (crc >> 8);
    }
    return crc;
}

static int add_file(FileList *list, const char *logical_path, const char *disk_path) {
    PackedFile *new_items;
    size_t i;
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].logical_path, logical_path) == 0) {
            fprintf(stderr, "error: duplicate logical path: %s\n", logical_path);
            return 0;
        }
    }
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 32 : list->capacity * 2;
        new_items = (PackedFile *)realloc(list->items, new_capacity * sizeof(PackedFile));
        if (new_items == NULL) {
            fprintf(stderr, "error: out of memory\n");
            return 0;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count].logical_path = xstrdup(logical_path);
    list->items[list->count].disk_path = xstrdup(disk_path);
    if (list->items[list->count].logical_path == NULL || list->items[list->count].disk_path == NULL) {
        fprintf(stderr, "error: out of memory\n");
        return 0;
    }
    list->items[list->count].crc32 = 0;
    list->items[list->count].size = 0;
    list->items[list->count].local_header_offset = 0;
    list->count++;
    return 1;
}

static int walk_tree(const char *root, const char *relative, FileList *list) {
    char current[MAX_PATH_BUF];
    DIR *dir;
    struct dirent *entry;
    if (relative[0] == '\0') {
        snprintf(current, sizeof(current), "%s", root);
    } else {
        join_path(current, sizeof(current), root, relative);
    }
    dir = opendir(current);
    if (dir == NULL) {
        fprintf(stderr, "error: cannot open directory %s: %s\n", current, strerror(errno));
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char disk_path[MAX_PATH_BUF];
        char logical_path[MAX_PATH_BUF];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!is_supported_component(entry->d_name)) {
            fprintf(stderr, "error: unsupported asset name: %s%s%s\n", relative, relative[0] ? "/" : "", entry->d_name);
            closedir(dir);
            return 0;
        }
        join_path(disk_path, sizeof(disk_path), current, entry->d_name);
        if (relative[0] == '\0') {
            snprintf(logical_path, sizeof(logical_path), "%s", entry->d_name);
        } else {
            snprintf(logical_path, sizeof(logical_path), "%s/%s", relative, entry->d_name);
        }
        if (stat(disk_path, &st) != 0) {
            fprintf(stderr, "error: cannot stat %s: %s\n", disk_path, strerror(errno));
            closedir(dir);
            return 0;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!walk_tree(root, logical_path, list)) {
                closedir(dir);
                return 0;
            }
        } else if (S_ISREG(st.st_mode)) {
            if ((uint64_t)st.st_size > 0xffffffffUL) {
                fprintf(stderr, "error: file is too large for ZIP32: %s\n", logical_path);
                closedir(dir);
                return 0;
            }
            if (!add_file(list, logical_path, disk_path)) {
                closedir(dir);
                return 0;
            }
        } else {
            fprintf(stderr, "error: unsupported non-regular asset: %s\n", logical_path);
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return 1;
}

static int compare_files(const void *a, const void *b) {
    const PackedFile *fa = (const PackedFile *)a;
    const PackedFile *fb = (const PackedFile *)b;
    return strcmp(fa->logical_path, fb->logical_path);
}

static void put_u16(FILE *f, uint16_t v) {
    fputc((int)(v & 0xffU), f);
    fputc((int)((v >> 8) & 0xffU), f);
}

static void put_u32(FILE *f, uint32_t v) {
    put_u16(f, (uint16_t)(v & 0xffffU));
    put_u16(f, (uint16_t)((v >> 16) & 0xffffU));
}

static int scan_file(PackedFile *file) {
    unsigned char buf[READ_BUF_SIZE];
    FILE *in = fopen(file->disk_path, "rb");
    size_t n;
    uint32_t crc = 0xffffffffU;
    uint32_t size = 0;
    if (in == NULL) {
        fprintf(stderr, "error: cannot read %s: %s\n", file->disk_path, strerror(errno));
        return 0;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (size > 0xffffffffU - (uint32_t)n) {
            fprintf(stderr, "error: file grew too large: %s\n", file->logical_path);
            fclose(in);
            return 0;
        }
        crc = crc32_update(crc, buf, n);
        size += (uint32_t)n;
    }
    if (ferror(in)) {
        fprintf(stderr, "error: failed reading %s\n", file->disk_path);
        fclose(in);
        return 0;
    }
    fclose(in);
    file->crc32 = crc ^ 0xffffffffU;
    file->size = size;
    return 1;
}

static int copy_file_data(FILE *out, const PackedFile *file) {
    unsigned char buf[READ_BUF_SIZE];
    FILE *in = fopen(file->disk_path, "rb");
    size_t n;
    if (in == NULL) {
        fprintf(stderr, "error: cannot read %s: %s\n", file->disk_path, strerror(errno));
        return 0;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "error: failed writing archive data\n");
            fclose(in);
            return 0;
        }
    }
    fclose(in);
    return 1;
}

static int write_archive(const char *output_path, FileList *list) {
    FILE *out = fopen(output_path, "wb");
    size_t i;
    uint32_t central_offset;
    uint32_t central_size;
    long pos;
    if (out == NULL) {
        fprintf(stderr, "error: cannot create %s: %s\n", output_path, strerror(errno));
        return 0;
    }
    for (i = 0; i < list->count; i++) {
        uint16_t name_len = (uint16_t)strlen(list->items[i].logical_path);
        if (!scan_file(&list->items[i])) {
            fclose(out);
            return 0;
        }
        pos = ftell(out);
        if (pos < 0 || (uint64_t)pos > 0xffffffffUL) {
            fprintf(stderr, "error: archive is too large for ZIP32\n");
            fclose(out);
            return 0;
        }
        list->items[i].local_header_offset = (uint32_t)pos;
        put_u32(out, 0x04034b50UL); put_u16(out, 20); put_u16(out, 0); put_u16(out, 0);
        put_u16(out, 0); put_u16(out, 0); put_u32(out, list->items[i].crc32);
        put_u32(out, list->items[i].size); put_u32(out, list->items[i].size);
        put_u16(out, name_len); put_u16(out, 0);
        fwrite(list->items[i].logical_path, 1, name_len, out);
        if (!copy_file_data(out, &list->items[i])) { fclose(out); return 0; }
    }
    pos = ftell(out);
    if (pos < 0 || (uint64_t)pos > 0xffffffffUL) { fclose(out); return 0; }
    central_offset = (uint32_t)pos;
    for (i = 0; i < list->count; i++) {
        uint16_t name_len = (uint16_t)strlen(list->items[i].logical_path);
        put_u32(out, 0x02014b50UL); put_u16(out, 20); put_u16(out, 20); put_u16(out, 0); put_u16(out, 0);
        put_u16(out, 0); put_u16(out, 0); put_u32(out, list->items[i].crc32);
        put_u32(out, list->items[i].size); put_u32(out, list->items[i].size);
        put_u16(out, name_len); put_u16(out, 0); put_u16(out, 0); put_u16(out, 0); put_u16(out, 0);
        put_u32(out, 0); put_u32(out, list->items[i].local_header_offset);
        fwrite(list->items[i].logical_path, 1, name_len, out);
    }
    pos = ftell(out);
    if (pos < 0 || (uint64_t)pos > 0xffffffffUL) { fclose(out); return 0; }
    central_size = (uint32_t)pos - central_offset;
    put_u32(out, 0x06054b50UL); put_u16(out, 0); put_u16(out, 0);
    put_u16(out, (uint16_t)list->count); put_u16(out, (uint16_t)list->count);
    put_u32(out, central_size); put_u32(out, central_offset); put_u16(out, 0);
    if (fclose(out) != 0) {
        fprintf(stderr, "error: failed closing %s\n", output_path);
        return 0;
    }
    return 1;
}

static int write_manifest(const char *output_path, const FileList *list) {
    char manifest_path[MAX_PATH_BUF];
    FILE *m;
    size_t i;
    snprintf(manifest_path, sizeof(manifest_path), "%s.manifest.txt", output_path);
    m = fopen(manifest_path, "w");
    if (m == NULL) {
        fprintf(stderr, "error: cannot create manifest %s: %s\n", manifest_path, strerror(errno));
        return 0;
    }
    fprintf(m, "archive: %s\nfiles: %lu\n\n", output_path, (unsigned long)list->count);
    for (i = 0; i < list->count; i++) {
        fprintf(m, "%10lu  %08lx  %s\n", (unsigned long)list->items[i].size,
                (unsigned long)list->items[i].crc32, list->items[i].logical_path);
    }
    fclose(m);
    printf("wrote %s with %lu files\n", output_path, (unsigned long)list->count);
    printf("wrote manifest %s\n", manifest_path);
    return 1;
}

static void free_list(FileList *list) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->items[i].logical_path);
        free(list->items[i].disk_path);
    }
    free(list->items);
}

int main(int argc, char **argv) {
    FileList list;
    struct stat st;
    if (argc != 3) {
        fprintf(stderr, "usage: asset_pack <asset_folder> <output.dat>\n");
        return 2;
    }
    if (stat(argv[1], &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "error: input must be a directory: %s\n", argv[1]);
        return 1;
    }
    memset(&list, 0, sizeof(list));
    crc32_init();
    if (!walk_tree(argv[1], "", &list)) { free_list(&list); return 1; }
    if (list.count > 0xffffU) {
        fprintf(stderr, "error: too many files for ZIP32\n");
        free_list(&list);
        return 1;
    }
    qsort(list.items, list.count, sizeof(PackedFile), compare_files);
    if (!write_archive(argv[2], &list) || !write_manifest(argv[2], &list)) {
        free_list(&list);
        return 1;
    }
    free_list(&list);
    return 0;
}
