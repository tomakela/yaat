#include "script_parser.h"
#include "script_bytecode.h"
#include <stdio.h>

static int validate_package(const YaatScriptPackage *p)
{
    int i;
    if (p->room_count < 1) { fprintf(stderr, "yaatc: no rooms found\n"); return 0; }
    for (i = 0; i < p->command_count; ++i) {
        if (p->commands[i].kind < YAAT_CMD_SAY || p->commands[i].kind > YAAT_CMD_DROP) {
            fprintf(stderr, "yaatc: invalid command kind %d\n", (int)p->commands[i].kind); return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    YaatScriptPackage package;
    if (argc != 3) { fprintf(stderr, "usage: yaatc input.yaat output.yaatbc\n"); return 2; }
    yaat_script_package_init(&package);
    if (!yaat_parse_script_file_into(&package, argv[1])) { fprintf(stderr, "yaatc: parse failed: %s\n", argv[1]); return 1; }
    if (!validate_package(&package)) return 1;
    if (!yaat_bytecode_write_file(argv[2], &package)) { fprintf(stderr, "yaatc: write failed: %s\n", argv[2]); return 1; }
    return 0;
}
