#ifndef YAAT_SCRIPT_BYTECODE_H
#define YAAT_SCRIPT_BYTECODE_H
#include "script_package.h"
#define YAAT_BYTECODE_VERSION 6
int yaat_bytecode_write_file(const char *path, const YaatScriptPackage *package);
int yaat_bytecode_read_file(const char *path, YaatScriptPackage *package);
#endif
