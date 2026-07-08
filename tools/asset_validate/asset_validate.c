/*
 * Offline YAAT asset tree validator.
 *
 * Rule source: docs/asset-structure.md
 * This tool intentionally has no runtime engine dependencies.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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
#define RUNTIME_PATH_LIMIT 120

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

static int errors = 0;
static int warnings = 0;

static const char *known_exts[] = {
    ".bmp", ".wav", ".mid", ".yaat", ".ini", ".pal", ".cur", ".fnt", NULL
};

static void report_issue(const char *kind, const char *path, const char *message) {
    if (strcmp(kind, "error") == 0) {
        errors++;
    } else {
        warnings++;
    }
    printf("%s: %s: %s\n", kind, path, message);
}

static void join_path(char *out, size_t out_size, const char *a, const char *b) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '/' || a[len - 1] == '\\')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s%c%s", a, PATH_SEP, b);
    }
}

static int stat_path(const char *path, struct stat *st) {
    return stat(path, st) == 0;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat_path(path, &st) && S_ISDIR(st.st_mode);
}

static int is_file(const char *path) {
    struct stat st;
    return stat_path(path, &st) && S_ISREG(st.st_mode);
}

static void require_file(const char *path) {
    if (!is_file(path)) {
        report_issue("error", path, "required file is missing");
    }
}

static int has_known_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    int i;
    if (dot == NULL) {
        return 0;
    }
    for (i = 0; known_exts[i] != NULL; i++) {
        if (strcmp(dot, known_exts[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_lowercase_ascii_runtime_path(const char *path) {
    const unsigned char *p = (const unsigned char *)path;
    while (*p != '\0') {
        if (*p > 127) {
            return 0;
        }
        if (isupper(*p) || isspace(*p)) {
            return 0;
        }
        if (!(islower(*p) || isdigit(*p) || *p == '_' || *p == '-' || *p == '.' || *p == '/' || *p == '\\' || *p == ':')) {
            return 0;
        }
        p++;
    }
    return 1;
}

static void check_path_recommendations(const char *display_path, int expect_file_extension) {
    if (!is_lowercase_ascii_runtime_path(display_path)) {
        report_issue("warning", display_path, "path should use lowercase ASCII names without spaces");
    }
    if (strlen(display_path) >= RUNTIME_PATH_LIMIT) {
        report_issue("warning", display_path, "runtime asset path should be under 120 characters");
    }
    if (expect_file_extension && !has_known_extension(display_path)) {
        report_issue("warning", display_path, "file extension is not one of .bmp, .wav, .mid, .yaat, .ini, .pal, .cur, .fnt");
    }
}

static void normalize_slashes(char *s) {
    while (*s != '\0') {
        if (*s == '\\') {
            *s = '/';
        }
        s++;
    }
}

static int read_ini_value(const char *path, const char *section, const char *key, char *out, size_t out_size) {
    FILE *fp = fopen(path, "r");
    char line[512];
    char current[128] = "";
    size_t key_len = strlen(key);
    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        char *end;
        while (isspace((unsigned char)*p)) p++;
        if (*p == ';' || *p == '#' || *p == '\0') continue;
        end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) *--end = '\0';
        if (*p == '[' && end > p + 2 && end[-1] == ']') {
            end[-1] = '\0';
            snprintf(current, sizeof(current), "%s", p + 1);
            continue;
        }
        if (strcmp(current, section) == 0 && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            char *value = p + key_len + 1;
            while (isspace((unsigned char)*value)) value++;
            snprintf(out, out_size, "%s", value);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void resolve_relative(char *out, size_t out_size, const char *base_dir, const char *ref) {
    if (ref[0] == '/' || ref[0] == '\\' || (isalpha((unsigned char)ref[0]) && ref[1] == ':')) {
        snprintf(out, out_size, "%s", ref);
    } else {
        join_path(out, out_size, base_dir, ref);
    }
}

static void validate_room_reference(const char *room_dir, const char *room_ini, const char *section, const char *key) {
    char ref[MAX_PATH_BUF];
    char full[MAX_PATH_BUF];
    if (!read_ini_value(room_ini, section, key, ref, sizeof(ref))) {
        report_issue("warning", room_ini, "recommended room metadata reference is missing");
        return;
    }
    normalize_slashes(ref);
    check_path_recommendations(ref, 1);
    resolve_relative(full, sizeof(full), room_dir, ref);
    if (!is_file(full)) {
        char msg[MAX_PATH_BUF + 128];
        snprintf(msg, sizeof(msg), "metadata [%s] %s references missing file '%s'", section, key, ref);
        report_issue("error", room_ini, msg);
    }
}

static void validate_room(const char *rooms_dir, const char *name) {
    char room_dir[MAX_PATH_BUF];
    char room_ini[MAX_PATH_BUF];
    join_path(room_dir, sizeof(room_dir), rooms_dir, name);
    if (!is_dir(room_dir)) return;
    check_path_recommendations(name, 0);
    join_path(room_ini, sizeof(room_ini), room_dir, "room.ini");
    require_file(room_ini);
    if (!is_file(room_ini)) return;
    validate_room_reference(room_dir, room_ini, "display", "background");
    validate_room_reference(room_dir, room_ini, "display", "palette");
    validate_room_reference(room_dir, room_ini, "audio", "music");
    validate_room_reference(room_dir, room_ini, "script", "file");
}

static void scan_files(const char *root, const char *relative) {
    char path[MAX_PATH_BUF];
    DIR *dir;
    struct dirent *entry;
    if (relative[0] == '\0') snprintf(path, sizeof(path), "%s", root);
    else join_path(path, sizeof(path), root, relative);
    dir = opendir(path);
    if (dir == NULL) return;
    while ((entry = readdir(dir)) != NULL) {
        char child_rel[MAX_PATH_BUF];
        char child_path[MAX_PATH_BUF];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (relative[0] == '\0') snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);
        else snprintf(child_rel, sizeof(child_rel), "%s/%s", relative, entry->d_name);
        join_path(child_path, sizeof(child_path), root, child_rel);
        if (is_dir(child_path)) {
            check_path_recommendations(child_rel, 0);
            scan_files(root, child_rel);
        } else if (is_file(child_path)) {
            check_path_recommendations(child_rel, 1);
        }
    }
    closedir(dir);
}

static void validate_rooms(const char *game_root) {
    char rooms_dir[MAX_PATH_BUF];
    DIR *dir;
    struct dirent *entry;
    join_path(rooms_dir, sizeof(rooms_dir), game_root, "rooms");
    if (!is_dir(rooms_dir)) {
        report_issue("error", rooms_dir, "required rooms folder is missing");
        return;
    }
    dir = opendir(rooms_dir);
    if (dir == NULL) {
        report_issue("error", rooms_dir, strerror(errno));
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        {
            char child[MAX_PATH_BUF];
            join_path(child, sizeof(child), rooms_dir, entry->d_name);
            if (is_dir(child)) {
                validate_room(rooms_dir, entry->d_name);
            } else {
                report_issue("warning", child, "rooms/ entries should be room folders");
            }
        }
    }
    closedir(dir);
}

int main(int argc, char **argv) {
    const char *game_root = argc > 1 ? argv[1] : "game";
    char game_ini[MAX_PATH_BUF];

    printf("YAAT offline asset validator\n");
    printf("validating: %s\n", game_root);

    if (!is_dir(game_root)) {
        report_issue("error", game_root, "game asset root folder is missing");
        printf("summary: %d error(s), %d warning(s)\n", errors, warnings);
        return 2;
    }

    join_path(game_ini, sizeof(game_ini), game_root, "game.ini");
    require_file(game_ini);
    validate_rooms(game_root);
    scan_files(game_root, "");

    printf("summary: %d error(s), %d warning(s)\n", errors, warnings);
    return errors == 0 ? 0 : 1;
}
