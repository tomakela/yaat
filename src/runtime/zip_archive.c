#include "runtime/zip_archive.h"

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "third_party/miniz/miniz.h"

#include <stdlib.h>
#include <string.h>

#define YAAT_ZIP_METHOD_STORE 0
#define YAAT_ZIP_METHOD_DEFLATE 8
#define YAAT_ZIP_MAX_ENTRIES 4096U

static void yaat_zip_set_error(YaatZipArchive *archive, const char *message)
{
    unsigned int i;

    if (archive == 0) return;
    if (message == 0) message = "ZIP archive error";
    for (i = 0; i + 1U < sizeof(archive->error) && message[i] != '\0'; ++i) {
        archive->error[i] = message[i];
    }
    archive->error[i] = '\0';
}

static int yaat_zip_is_slash(char ch)
{
    return ch == '/' || ch == '\\';
}

static int yaat_zip_normalize_path(const char *path, char *out, unsigned int out_size)
{
    unsigned int in_i;
    unsigned int out_i;
    unsigned int segment_len;

    if (path == 0 || out == 0 || out_size == 0U || path[0] == '\0') return 0;
    if (yaat_zip_is_slash(path[0])) return 0;
    if (path[0] != '\0' && path[1] == ':') return 0;

    in_i = 0U;
    out_i = 0U;
    segment_len = 0U;
    while (path[in_i] != '\0') {
        unsigned char ch;

        ch = (unsigned char)path[in_i++];
        if (ch < 32U || ch > 126U) return 0;
        if (yaat_zip_is_slash((char)ch)) {
            if (segment_len == 0U) return 0;
            if (segment_len == 1U && out[out_i - 1U] == '.') return 0;
            if (segment_len == 2U && out[out_i - 2U] == '.' && out[out_i - 1U] == '.') return 0;
            if (out_i + 1U >= out_size) return 0;
            out[out_i++] = '/';
            segment_len = 0U;
        } else {
            if (out_i + 1U >= out_size) return 0;
            out[out_i++] = (char)ch;
            ++segment_len;
        }
    }
    if (segment_len == 0U) return 0;
    if (segment_len == 1U && out[out_i - 1U] == '.') return 0;
    if (segment_len == 2U && out[out_i - 2U] == '.' && out[out_i - 1U] == '.') return 0;
    out[out_i] = '\0';
    return 1;
}

static int yaat_zip_entry_is_symlink(const mz_zip_archive_file_stat *stat)
{
    unsigned int mode;

    mode = (unsigned int)((stat->m_external_attr >> 16) & 0170000U);
    return mode == 0120000U;
}

YaatZipResult yaat_zip_open(YaatZipArchive *archive, const char *path)
{
    mz_zip_archive *zip;

    if (archive == 0 || path == 0) return YAAT_ZIP_ERROR_IO;
    memset(archive, 0, sizeof(*archive));
    zip = (mz_zip_archive *)calloc(1U, sizeof(*zip));
    if (zip == 0) {
        yaat_zip_set_error(archive, "Out of memory opening ZIP archive");
        return YAAT_ZIP_ERROR_MEMORY;
    }
    if (!mz_zip_reader_init_file(zip, path, 0)) {
        free(zip);
        yaat_zip_set_error(archive, "Could not open ZIP archive");
        return YAAT_ZIP_ERROR_IO;
    }
    archive->zip = zip;
    return YAAT_ZIP_OK;
}

YaatZipResult yaat_zip_read_toc(YaatZipArchive *archive)
{
    mz_zip_archive *zip;
    mz_uint count;
    mz_uint i;

    if (archive == 0 || archive->zip == 0) return YAAT_ZIP_ERROR_IO;
    zip = (mz_zip_archive *)archive->zip;
    count = mz_zip_reader_get_num_files(zip);
    if (count > YAAT_ZIP_MAX_ENTRIES) {
        yaat_zip_set_error(archive, "ZIP archive has too many entries");
        return YAAT_ZIP_ERROR_LIMIT;
    }
    free(archive->entries);
    archive->entries = 0;
    archive->entry_count = 0U;
    archive->entry_capacity = (unsigned int)count;
    if (count > 0U) {
        archive->entries = (YaatZipEntry *)calloc((size_t)count, sizeof(YaatZipEntry));
        if (archive->entries == 0) {
            yaat_zip_set_error(archive, "Out of memory reading ZIP table of contents");
            return YAAT_ZIP_ERROR_MEMORY;
        }
    }

    for (i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        YaatZipEntry *entry;
        char normalized[YAAT_ZIP_MAX_PATH];

        if (!mz_zip_reader_file_stat(zip, i, &stat)) {
            yaat_zip_set_error(archive, "Could not read ZIP central directory entry");
            return YAAT_ZIP_ERROR_FORMAT;
        }
        if (stat.m_is_directory) continue;
        if (!yaat_zip_normalize_path(stat.m_filename, normalized, sizeof(normalized))) {
            yaat_zip_set_error(archive, "ZIP entry path is not a relative ASCII path");
            return YAAT_ZIP_ERROR_UNSUPPORTED;
        }
        if (stat.m_is_encrypted || !stat.m_is_supported || yaat_zip_entry_is_symlink(&stat) ||
            (stat.m_method != YAAT_ZIP_METHOD_STORE && stat.m_method != YAAT_ZIP_METHOD_DEFLATE) ||
            stat.m_comp_size > 0xffffffffUL || stat.m_uncomp_size > 0xffffffffUL) {
            yaat_zip_set_error(archive, "ZIP entry uses an unsupported feature");
            return YAAT_ZIP_ERROR_UNSUPPORTED;
        }
        entry = &archive->entries[archive->entry_count++];
        strcpy(entry->path, normalized);
        entry->compressed_size = (unsigned long)stat.m_comp_size;
        entry->uncompressed_size = (unsigned long)stat.m_uncomp_size;
        entry->method = stat.m_method;
        entry->file_index = i;
    }
    return YAAT_ZIP_OK;
}

const YaatZipEntry *yaat_zip_find(YaatZipArchive *archive, const char *path)
{
    char normalized[YAAT_ZIP_MAX_PATH];
    unsigned int i;

    if (archive == 0 || !yaat_zip_normalize_path(path, normalized, sizeof(normalized))) return 0;
    for (i = 0U; i < archive->entry_count; ++i) {
        if (strcmp(archive->entries[i].path, normalized) == 0) return &archive->entries[i];
    }
    return 0;
}

YaatZipResult yaat_zip_read_entry(YaatZipArchive *archive, const YaatZipEntry *entry,
                                  unsigned long max_size, unsigned char **data,
                                  unsigned long *data_size)
{
    size_t heap_size;
    void *heap_data;

    if (data != 0) *data = 0;
    if (data_size != 0) *data_size = 0U;
    if (archive == 0 || archive->zip == 0 || entry == 0 || data == 0) return YAAT_ZIP_ERROR_IO;
    if (max_size == 0UL) max_size = YAAT_ZIP_DEFAULT_MAX_ENTRY_SIZE;
    if (entry->uncompressed_size > max_size) {
        yaat_zip_set_error(archive, "ZIP entry exceeds configured size limit");
        return YAAT_ZIP_ERROR_LIMIT;
    }
    heap_size = 0U;
    heap_data = mz_zip_reader_extract_to_heap((mz_zip_archive *)archive->zip,
                                              (mz_uint)entry->file_index,
                                              &heap_size, 0);
    if (heap_data == 0 && entry->uncompressed_size != 0UL) {
        yaat_zip_set_error(archive, "Could not extract ZIP entry");
        return YAAT_ZIP_ERROR_FORMAT;
    }
    if (heap_size > max_size) {
        free(heap_data);
        yaat_zip_set_error(archive, "ZIP entry exceeded size limit while extracting");
        return YAAT_ZIP_ERROR_LIMIT;
    }
    *data = (unsigned char *)heap_data;
    if (data_size != 0) *data_size = (unsigned long)heap_size;
    return YAAT_ZIP_OK;
}

void yaat_zip_close(YaatZipArchive *archive)
{
    if (archive == 0) return;
    if (archive->zip != 0) {
        mz_zip_reader_end((mz_zip_archive *)archive->zip);
        free(archive->zip);
    }
    free(archive->entries);
    memset(archive, 0, sizeof(*archive));
}

const char *yaat_zip_error(const YaatZipArchive *archive)
{
    if (archive == 0 || archive->error[0] == '\0') return "No ZIP archive error";
    return archive->error;
}

const char *yaat_zip_result_string(YaatZipResult result)
{
    switch (result) {
    case YAAT_ZIP_OK: return "ok";
    case YAAT_ZIP_ERROR_IO: return "I/O error";
    case YAAT_ZIP_ERROR_FORMAT: return "invalid ZIP format";
    case YAAT_ZIP_ERROR_UNSUPPORTED: return "unsupported ZIP feature";
    case YAAT_ZIP_ERROR_NOT_FOUND: return "entry not found";
    case YAAT_ZIP_ERROR_LIMIT: return "ZIP size limit exceeded";
    case YAAT_ZIP_ERROR_MEMORY: return "out of memory";
    }
    return "unknown ZIP error";
}
