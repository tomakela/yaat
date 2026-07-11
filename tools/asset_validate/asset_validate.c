/*
 * Offline YAAT asset tree / archive validator.
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
#define MAX_ASSET_BYTES (64UL * 1024UL * 1024UL)
#define ZIP_EOCD_SIG 0x06054b50UL
#define ZIP_CDH_SIG 0x02014b50UL
#define ZIP_LFH_SIG 0x04034b50UL

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

typedef struct AssetEntry {
    char path[MAX_PATH_BUF];
    char source[MAX_PATH_BUF];
    unsigned long size;
    unsigned long local_header;
    unsigned short method;
    unsigned short flags;
    int is_archive;
    int readable_text;
    int overrides;
} AssetEntry;

typedef struct AssetList {
    AssetEntry *items;
    size_t count;
    size_t cap;
} AssetList;

static int errors = 0;
static int warnings = 0;

static const char *known_exts[] = { ".bmp", ".wav", ".mid", ".yaat", ".ini", ".pal", ".cur", ".fnt", NULL };

static void report_issue(const char *kind, const char *path, const char *message) {
    if (strcmp(kind, "error") == 0) errors++; else warnings++;
    printf("%s: %s: %s\n", kind, path, message);
}

static unsigned short rd16(const unsigned char *p) { return (unsigned short)(p[0] | (p[1] << 8)); }
static unsigned long rd32(const unsigned char *p) { return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }

static void join_path(char *out, size_t out_size, const char *a, const char *b) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '/' || a[len - 1] == '\\')) snprintf(out, out_size, "%s%s", a, b);
    else snprintf(out, out_size, "%s%c%s", a, PATH_SEP, b);
}
static int stat_path(const char *path, struct stat *st) { return stat(path, st) == 0; }
static int is_dir(const char *path) { struct stat st; return stat_path(path, &st) && S_ISDIR(st.st_mode); }
static int is_file(const char *path) { struct stat st; return stat_path(path, &st) && S_ISREG(st.st_mode); }

static void normalize_slashes(char *s) { while (*s) { if (*s == '\\') *s = '/'; s++; } }

static int has_known_extension(const char *path) {
    const char *dot = strrchr(path, '.'); int i;
    if (!dot) return 0;
    for (i = 0; known_exts[i]; i++) if (strcmp(dot, known_exts[i]) == 0) return 1;
    return 0;
}

static int normalized_asset_path(const char *in, char *out, size_t out_size) {
    char tmp[MAX_PATH_BUF]; char *tok; size_t len;
    if (!in || !*in || strlen(in) >= sizeof(tmp)) return 0;
    snprintf(tmp, sizeof(tmp), "%s", in); normalize_slashes(tmp);
    if (tmp[0] == '/' || strstr(tmp, "//") || (isalpha((unsigned char)tmp[0]) && tmp[1] == ':')) return 0;
    out[0] = '\0'; tok = strtok(tmp, "/");
    while (tok) {
        if (strcmp(tok, ".") == 0 || strcmp(tok, "..") == 0 || tok[0] == '\0') return 0;
        len = strlen(out);
        if (len && len + 1 < out_size) strncat(out, "/", out_size - strlen(out) - 1);
        if (strlen(out) + strlen(tok) + 1 >= out_size) return 0;
        strcat(out, tok);
        tok = strtok(NULL, "/");
    }
    return out[0] != '\0';
}

static int is_lowercase_ascii_runtime_path(const char *path) {
    const unsigned char *p = (const unsigned char *)path;
    while (*p) {
        if (*p > 127 || isupper(*p) || isspace(*p)) return 0;
        if (!(islower(*p) || isdigit(*p) || *p == '_' || *p == '-' || *p == '.' || *p == '/')) return 0;
        p++;
    }
    return 1;
}

static void check_path_recommendations(const char *display_path, int expect_file_extension) {
    char norm[MAX_PATH_BUF];
    if (!normalized_asset_path(display_path, norm, sizeof(norm))) {
        report_issue("error", display_path, "asset path is not normalized or safe");
        return;
    }
    if (!is_lowercase_ascii_runtime_path(norm)) report_issue("warning", display_path, "path should use lowercase ASCII names without spaces");
    if (strlen(norm) >= RUNTIME_PATH_LIMIT) report_issue("warning", display_path, "runtime asset path should be under 120 characters");
    if (expect_file_extension && !has_known_extension(norm)) report_issue("warning", display_path, "file extension is not one of .bmp, .wav, .mid, .yaat, .ini, .pal, .cur, .fnt");
}

static void assets_add(AssetList *list, const AssetEntry *entry) {
    size_t i; AssetEntry copy = *entry;
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].path, copy.path) == 0) { copy.overrides = 1; break; }
    }
    if (list->count == list->cap) {
        size_t nc = list->cap ? list->cap * 2 : 128;
        AssetEntry *ni = (AssetEntry *)realloc(list->items, nc * sizeof(*ni));
        if (!ni) { report_issue("error", copy.path, "out of memory"); return; }
        list->items = ni; list->cap = nc;
    }
    list->items[list->count++] = copy;
    if (copy.overrides) {
        char msg[MAX_PATH_BUF + 80]; snprintf(msg, sizeof(msg), "overrides an existing asset intentionally from %s", copy.source);
        report_issue("warning", copy.path, msg);
    }
}

static const AssetEntry *assets_find_effective(const AssetList *list, const char *path) {
    size_t i; char norm[MAX_PATH_BUF];
    if (!normalized_asset_path(path, norm, sizeof(norm))) return NULL;
    for (i = list->count; i > 0; i--) if (strcmp(list->items[i-1].path, norm) == 0) return &list->items[i-1];
    return NULL;
}

static void scan_loose(const char *root, const char *relative, AssetList *list) {
    char path[MAX_PATH_BUF]; DIR *dir; struct dirent *entry;
    if (relative[0] == '\0') snprintf(path, sizeof(path), "%s", root); else join_path(path, sizeof(path), root, relative);
    dir = opendir(path); if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char child_rel[MAX_PATH_BUF], child_path[MAX_PATH_BUF], norm[MAX_PATH_BUF]; struct stat st;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (relative[0] == '\0') snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name); else snprintf(child_rel, sizeof(child_rel), "%s/%s", relative, entry->d_name);
        join_path(child_path, sizeof(child_path), root, child_rel);
        if (is_dir(child_path)) { check_path_recommendations(child_rel, 0); scan_loose(root, child_rel, list); }
        else if (is_file(child_path)) {
            AssetEntry e; memset(&e, 0, sizeof(e)); check_path_recommendations(child_rel, 1);
            if (!normalized_asset_path(child_rel, norm, sizeof(norm))) continue;
            snprintf(e.path, sizeof(e.path), "%s", norm); snprintf(e.source, sizeof(e.source), "%s", child_path);
            if (stat_path(child_path, &st)) e.size = (unsigned long)st.st_size;
            e.readable_text = 1; assets_add(list, &e);
            if (e.size > MAX_ASSET_BYTES) report_issue("error", child_rel, "file is oversized before decompression/load");
        }
    }
    closedir(dir);
}

static int read_file_bytes_at(const char *path, unsigned long off, unsigned char *buf, size_t n) {
    FILE *fp = fopen(path, "rb"); if (!fp) return 0; if (fseek(fp, (long)off, SEEK_SET) != 0) { fclose(fp); return 0; }
    if (fread(buf, 1, n, fp) != n) { fclose(fp); return 0; } fclose(fp); return 1;
}

static void validate_zip_archive(const char *archive, AssetList *list) {
    FILE *fp; long size, search_start, i; unsigned char tail[66000]; size_t tail_len; long eocd = -1; unsigned long cd_off, cd_size, pos; unsigned short entries, n;
    fp = fopen(archive, "rb"); if (!fp) { report_issue("error", archive, strerror(errno)); return; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); search_start = size > 66000 ? size - 66000 : 0; fseek(fp, search_start, SEEK_SET); tail_len = fread(tail, 1, (size_t)(size - search_start), fp);
    for (i = (long)tail_len - 22; i >= 0; i--) if (rd32(tail + i) == ZIP_EOCD_SIG) { eocd = search_start + i; break; }
    if (eocd < 0) { report_issue("error", archive, "not a supported ZIP archive (missing end of central directory)"); fclose(fp); return; }
    (void)eocd; entries = rd16(tail + (eocd - search_start) + 10); cd_size = rd32(tail + (eocd - search_start) + 12); cd_off = rd32(tail + (eocd - search_start) + 16);
    if (cd_off == 0xffffffffUL || cd_size == 0xffffffffUL) report_issue("error", archive, "ZIP64 archives are not supported");
    pos = cd_off;
    for (n = 0; n < entries; n++) {
        unsigned char hdr[46]; unsigned short flags, method, name_len, extra_len, comment_len; unsigned long comp, uncomp, lhoff; char name[MAX_PATH_BUF], norm[MAX_PATH_BUF]; size_t got; AssetEntry e; size_t j;
        if (fseek(fp, (long)pos, SEEK_SET) != 0 || fread(hdr, 1, 46, fp) != 46 || rd32(hdr) != ZIP_CDH_SIG) { report_issue("error", archive, "invalid ZIP central directory"); break; }
        flags = rd16(hdr + 8); method = rd16(hdr + 10); comp = rd32(hdr + 20); uncomp = rd32(hdr + 24); name_len = rd16(hdr + 28); extra_len = rd16(hdr + 30); comment_len = rd16(hdr + 32); lhoff = rd32(hdr + 42);
        if (name_len >= sizeof(name)) { report_issue("error", archive, "ZIP entry name is too long"); fseek(fp, name_len + extra_len + comment_len, SEEK_CUR); pos += 46UL + name_len + extra_len + comment_len; continue; }
        got = fread(name, 1, name_len, fp); name[got] = '\0';
        for (j = 0; j < got; j++) if (name[j] == '\0') name[j] = '?';
        normalize_slashes(name);
        if (name[got ? got - 1 : 0] != '/') {
            check_path_recommendations(name, 1);
            if (!normalized_asset_path(name, norm, sizeof(norm))) { report_issue("error", name, "archive entry path is unsafe"); }
            else {
                size_t k; for (k = 0; k < list->count; k++) if (list->items[k].is_archive && strcmp(list->items[k].source, archive) == 0 && strcmp(list->items[k].path, norm) == 0) report_issue("error", norm, "duplicate entry within archive");
                if (flags & 1) report_issue("error", norm, "encrypted ZIP entries are unsupported");
                if (flags & 8) report_issue("warning", norm, "ZIP data descriptors are unsupported by simple runtime loaders");
                if (method != 0 && method != 8) report_issue("error", norm, "unsupported ZIP compression method");
                if (uncomp == 0xffffffffUL || comp == 0xffffffffUL) report_issue("error", norm, "ZIP64 entry sizes are unsupported");
                if (uncomp > MAX_ASSET_BYTES) report_issue("error", norm, "archive entry is oversized before decompression");
                memset(&e, 0, sizeof(e)); snprintf(e.path, sizeof(e.path), "%s", norm); snprintf(e.source, sizeof(e.source), "%s", archive); e.size = uncomp; e.local_header = lhoff; e.method = method; e.flags = flags; e.is_archive = 1; e.readable_text = method == 0; assets_add(list, &e);
            }
        }
        pos += 46UL + name_len + extra_len + comment_len;
    }
    fclose(fp);
}

static int archive_read_stored_text(const AssetEntry *e, char *out, size_t out_size) {
    unsigned char hdr[30]; unsigned short name_len, extra_len; FILE *fp; size_t n;
    if (!e->is_archive || e->method != 0 || e->size + 1 > out_size) return 0;
    if (!read_file_bytes_at(e->source, e->local_header, hdr, 30) || rd32(hdr) != ZIP_LFH_SIG) return 0;
    name_len = rd16(hdr + 26); extra_len = rd16(hdr + 28);
    fp = fopen(e->source, "rb"); if (!fp) return 0;
    if (fseek(fp, (long)(e->local_header + 30UL + name_len + extra_len), SEEK_SET) != 0) { fclose(fp); return 0; }
    n = fread(out, 1, e->size, fp); fclose(fp); if (n != e->size) return 0; out[n] = '\0'; return 1;
}

static int read_asset_text(const AssetList *list, const char *path, char *out, size_t out_size) {
    const AssetEntry *e = assets_find_effective(list, path); FILE *fp; size_t n;
    if (!e || e->size + 1 > out_size) return 0;
    if (e->is_archive) return archive_read_stored_text(e, out, out_size);
    fp = fopen(e->source, "rb"); if (!fp) return 0; n = fread(out, 1, out_size - 1, fp); fclose(fp); out[n] = '\0'; return 1;
}

static int ini_value_from_text(char *text, const char *section, const char *key, char *out, size_t out_size) {
    char current[128] = ""; char *line = strtok(text, "\n");
    while (line) {
        char *p = line; char *end; char *eq;
        while (isspace((unsigned char)*p)) p++;
        end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) *--end = '\0';
        if (*p && *p != ';' && *p != '#') {
            if (*p == '[' && end > p + 2 && end[-1] == ']') { end[-1] = '\0'; snprintf(current, sizeof(current), "%s", p + 1); }
            else if (!strcmp(current, section) && (eq = strchr(p, '=')) != NULL) { *eq = '\0'; end = p + strlen(p); while (end > p && isspace((unsigned char)end[-1])) *--end = '\0'; if (!strcmp(p, key)) { eq++; while (isspace((unsigned char)*eq)) eq++; snprintf(out, out_size, "%s", eq); return 1; } }
        }
        line = strtok(NULL, "\n");
    }
    return 0;
}

static int read_ini_value_asset(const AssetList *list, const char *path, const char *section, const char *key, char *out, size_t out_size) {
    char text[65536]; if (!read_asset_text(list, path, text, sizeof(text))) return 0; return ini_value_from_text(text, section, key, out, out_size);
}

static void asset_dirname(char *out, size_t out_size, const char *path) { const char *s = strrchr(path, '/'); if (!s) out[0] = '\0'; else { size_t n = (size_t)(s - path); if (n >= out_size) n = out_size - 1; memcpy(out, path, n); out[n] = '\0'; } }
static void asset_join_ref(char *out, size_t out_size, const char *base_file, const char *ref) {
    char base[MAX_PATH_BUF], tmp[MAX_PATH_BUF], *parts[128]; int count = 0, i; char *tok;
    if (ref[0] == '/' || ref[0] == '\\' || (isalpha((unsigned char)ref[0]) && ref[1] == ':')) { snprintf(out, out_size, "%s", ref); normalize_slashes(out); return; }
    asset_dirname(base, sizeof(base), base_file);
    if (base[0]) {
        if (strlen(base) + strlen(ref) + 2 > sizeof(tmp)) { out[0] = '\0'; return; }
        strcpy(tmp, base);
        strcat(tmp, "/");
        strcat(tmp, ref);
    } else {
        snprintf(tmp, sizeof(tmp), "%s", ref);
    }
    normalize_slashes(tmp);
    tok = strtok(tmp, "/"); out[0] = '\0';
    while (tok && count < 128) { if (!strcmp(tok, "..")) { if (count > 0) count--; else { snprintf(out, out_size, "../%s", ref); return; } } else if (strcmp(tok, ".") != 0) parts[count++] = tok; tok = strtok(NULL, "/"); }
    for (i = 0; i < count; i++) { if (i) strncat(out, "/", out_size - strlen(out) - 1); strncat(out, parts[i], out_size - strlen(out) - 1); }
}

static void validate_room_reference_effective(const AssetList *list, const char *room_ini, const char *section, const char *key) {
    char ref[MAX_PATH_BUF], full[MAX_PATH_BUF];
    if (!read_ini_value_asset(list, room_ini, section, key, ref, sizeof(ref))) { report_issue("warning", room_ini, "recommended room metadata reference is missing or unreadable"); return; }
    normalize_slashes(ref);
    asset_join_ref(full, sizeof(full), room_ini, ref);
    check_path_recommendations(full, 1);
    if (!normalized_asset_path(full, full, sizeof(full)) || !assets_find_effective(list, full)) { char msg[MAX_PATH_BUF + 128]; snprintf(msg, sizeof(msg), "metadata [%s] %s references missing file '%s'", section, key, ref); report_issue("error", room_ini, msg); }
}

static void validate_effective_view(const AssetList *list) {
    char first_room[MAX_PATH_BUF], rooms_path[MAX_PATH_BUF] = "rooms", room_ini[MAX_PATH_BUF];
    if (!assets_find_effective(list, "game.ini")) { report_issue("error", "game.ini", "required file is missing from final effective asset view"); return; }
    if (!read_ini_value_asset(list, "game.ini", "game", "first_room", first_room, sizeof(first_room))) { report_issue("error", "game.ini", "game.first_room is missing or unreadable"); return; }
    read_ini_value_asset(list, "game.ini", "paths", "rooms", rooms_path, sizeof(rooms_path));
    check_path_recommendations(first_room, 0); check_path_recommendations(rooms_path, 0);
    if (strlen(rooms_path) + strlen(first_room) + 11 > sizeof(room_ini)) {
        report_issue("error", first_room, "first room path is too long");
        return;
    }
    strcpy(room_ini, rooms_path);
    strcat(room_ini, "/");
    strcat(room_ini, first_room);
    strcat(room_ini, "/room.ini");
    normalize_slashes(room_ini);
    if (!assets_find_effective(list, room_ini)) { report_issue("error", room_ini, "first room metadata file is missing"); return; }
    validate_room_reference_effective(list, room_ini, "display", "background");
    validate_room_reference_effective(list, room_ini, "display", "palette");
    validate_room_reference_effective(list, room_ini, "audio", "music");
    validate_room_reference_effective(list, room_ini, "script", "file");
    {
        size_t i;
        char prefix[MAX_PATH_BUF];
        strcpy(prefix, rooms_path);
        strcat(prefix, "/");
        for (i = 0; i < list->count; i++) {
            const char *p = list->items[i].path;
            size_t len = strlen(p);
            if (!strncmp(p, prefix, strlen(prefix)) && len > 9 && !strcmp(p + len - 9, "/room.ini") && strcmp(p, room_ini) != 0) {
                validate_room_reference_effective(list, p, "display", "background");
                validate_room_reference_effective(list, p, "display", "palette");
                validate_room_reference_effective(list, p, "audio", "music");
                validate_room_reference_effective(list, p, "script", "file");
            }
        }
    }
}

static int line_has_unquoted_label(const char *line) {
    int in_string = 0;
    while (*line) {
        if (*line == '"') in_string = !in_string;
        else if (!in_string && line[0] == '#' && line[1] == ' ' && line[2] == '@') return 1;
        else if (!in_string && line[0] == '#' && line[1] == '@') return 1;
        line++;
    }
    return 0;
}

static int line_has_script_text_command(const char *line) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    return !strncmp(p, "say ", 4) || !strncmp(p, "title_card ", 11) || !strncmp(p, "cutscene_text ", 14);
}

static void validate_script_string_labels(const AssetEntry *entry) {
    FILE *fp;
    char line[MAX_PATH_BUF];
    unsigned long line_no = 0;
    fp = fopen(entry->source, "rb");
    if (!fp) return;
    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        if (line_has_script_text_command(line) && !line_has_unquoted_label(line)) {
            char path[MAX_PATH_BUF + 80];
            snprintf(path, sizeof(path), "%s:%lu", entry->path, line_no);
            report_issue("warning", path, "script text is missing a # @string.id label");
        }
    }
    fclose(fp);
}

static void validate_all_script_string_labels(const AssetList *list) {
    size_t i;
    for (i = 0; i < list->count; ++i) {
        size_t len = strlen(list->items[i].path);
        if (!list->items[i].is_archive && len > 5 && !strcmp(list->items[i].path + len - 5, ".yaat")) {
            validate_script_string_labels(&list->items[i]);
        }
    }
}

static int cmp_names(const void *a, const void *b) { const char * const *sa = (const char * const *)a; const char * const *sb = (const char * const *)b; return strcmp(*sa, *sb); }

static void discover_archives(const char *base, AssetList *list) {
    char packed[MAX_PATH_BUF], game_dat[MAX_PATH_BUF]; DIR *dir; struct dirent *entry; char **patches = NULL; size_t count = 0, cap = 0, i;
    join_path(packed, sizeof(packed), base, "packed"); join_path(game_dat, sizeof(game_dat), packed, "game.dat");
    if (is_file(game_dat)) validate_zip_archive(game_dat, list); else { join_path(game_dat, sizeof(game_dat), base, "game.dat"); if (is_file(game_dat)) validate_zip_archive(game_dat, list); }
    dir = opendir(packed); if (!dir) dir = opendir(base); if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        if (!strncmp(entry->d_name, "patch", 5) && strlen(entry->d_name) == 13 && !strcmp(entry->d_name + 9, ".dat") && isdigit((unsigned char)entry->d_name[5]) && isdigit((unsigned char)entry->d_name[6]) && isdigit((unsigned char)entry->d_name[7]) && isdigit((unsigned char)entry->d_name[8])) {
            char p[MAX_PATH_BUF]; join_path(p, sizeof(p), packed, entry->d_name); if (!is_file(p)) { join_path(p, sizeof(p), base, entry->d_name); if (!is_file(p)) continue; }
            if (count == cap) { size_t nc = cap ? cap * 2 : 8; char **np = (char **)realloc(patches, nc * sizeof(*np)); if (!np) break; patches = np; cap = nc; }
            patches[count] = (char *)malloc(strlen(p) + 1); if (patches[count]) strcpy(patches[count++], p);
        }
    }
    closedir(dir); qsort(patches, count, sizeof(*patches), cmp_names);
    for (i = 0; i < count; i++) { validate_zip_archive(patches[i], list); free(patches[i]); }
    free(patches);
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "game"; AssetList list; memset(&list, 0, sizeof(list));
    printf("YAAT offline asset validator\nvalidating: %s\n", root);
    if (is_dir(root)) { scan_loose(root, "", &list); discover_archives(root, &list); }
    else if (is_file(root)) { validate_zip_archive(root, &list); }
    else { report_issue("error", root, "game asset root folder or archive is missing"); }
    validate_effective_view(&list);
    validate_all_script_string_labels(&list);
    printf("summary: %d error(s), %d warning(s)\n", errors, warnings);
    free(list.items); return errors == 0 ? 0 : 1;
}
