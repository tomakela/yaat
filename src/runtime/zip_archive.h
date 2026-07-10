#ifndef YAAT_RUNTIME_ZIP_ARCHIVE_H
#define YAAT_RUNTIME_ZIP_ARCHIVE_H

#include <stddef.h>

#define YAAT_ZIP_MAX_PATH 128
#define YAAT_ZIP_DEFAULT_MAX_ENTRY_SIZE (16UL * 1024UL * 1024UL)

typedef enum YaatZipResult {
    YAAT_ZIP_OK = 0,
    YAAT_ZIP_ERROR_IO,
    YAAT_ZIP_ERROR_FORMAT,
    YAAT_ZIP_ERROR_UNSUPPORTED,
    YAAT_ZIP_ERROR_NOT_FOUND,
    YAAT_ZIP_ERROR_LIMIT,
    YAAT_ZIP_ERROR_MEMORY
} YaatZipResult;

typedef struct YaatZipEntry {
    char path[YAAT_ZIP_MAX_PATH];
    unsigned long compressed_size;
    unsigned long uncompressed_size;
    unsigned short method;
    unsigned int file_index;
} YaatZipEntry;

typedef struct YaatZipArchive {
    void *zip;
    YaatZipEntry *entries;
    unsigned int entry_count;
    unsigned int entry_capacity;
    char error[128];
} YaatZipArchive;

YaatZipResult yaat_zip_open(YaatZipArchive *archive, const char *path);
YaatZipResult yaat_zip_read_toc(YaatZipArchive *archive);
const YaatZipEntry *yaat_zip_find(YaatZipArchive *archive, const char *path);
YaatZipResult yaat_zip_read_entry(YaatZipArchive *archive,
                                  const YaatZipEntry *entry,
                                  unsigned long max_size,
                                  unsigned char **data,
                                  unsigned long *data_size);
void yaat_zip_close(YaatZipArchive *archive);
const char *yaat_zip_error(const YaatZipArchive *archive);
const char *yaat_zip_result_string(YaatZipResult result);

#endif
