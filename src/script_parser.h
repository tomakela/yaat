#ifndef YAAT_SCRIPT_PARSER_H
#define YAAT_SCRIPT_PARSER_H
#include "script_package.h"
int yaat_parse_script_text_into(YaatScriptPackage *package, const char *source);
int yaat_parse_script_file_into(YaatScriptPackage *package, const char *path);
#endif
