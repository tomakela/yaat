#ifndef YAAT_SCRIPT_PACKAGE_H
#define YAAT_SCRIPT_PACKAGE_H

#define YAAT_MAX_ROOMS 8
#define YAAT_MAX_ENTITIES 32
#define YAAT_MAX_EVENTS 8
#define YAAT_MAX_COMMANDS 64
#define YAAT_MAX_VARS 64
#define YAAT_MAX_INVENTORY 16
#define YAAT_TEXT_MAX 160
#define YAAT_MAX_RUNTIME_HOTSPOTS 32

typedef enum YaatEntityKind { YAAT_ENTITY_HOTSPOT, YAAT_ENTITY_OBJECT } YaatEntityKind;
typedef enum YaatCommandKind { YAAT_CMD_SAY, YAAT_CMD_SET, YAAT_CMD_GOTO, YAAT_CMD_PLAY_SOUND, YAAT_CMD_TAKE, YAAT_CMD_HIDE, YAAT_CMD_IF, YAAT_CMD_SHAKE } YaatCommandKind;

typedef struct YaatEvent { char name[32]; char item[32]; int first_command; int command_count; } YaatEvent;
typedef struct YaatEntity { YaatEntityKind kind; char id[32]; char name[64]; int x; int y; int w; int h; int visible; YaatEvent events[YAAT_MAX_EVENTS]; int event_count; } YaatEntity;
typedef struct YaatRoom { char id[32]; char label[64]; unsigned long color; YaatEntity entities[YAAT_MAX_ENTITIES]; int entity_count; YaatEvent events[YAAT_MAX_EVENTS]; int event_count; } YaatRoom;
typedef struct YaatCommand { YaatCommandKind kind; char a[96]; char b[96]; int bool_value; int int_value; int first_child; int child_count; int first_else_child; int else_child_count; } YaatCommand;
typedef struct YaatVar { char name[32]; int bool_value; } YaatVar;

typedef struct YaatScriptPackage {
    YaatRoom rooms[YAAT_MAX_ROOMS]; int room_count;
    YaatCommand commands[YAAT_MAX_COMMANDS]; int command_count;
    YaatVar vars[YAAT_MAX_VARS]; int var_count;
} YaatScriptPackage;

void yaat_script_package_init(YaatScriptPackage *package);
int yaat_script_package_set_var(YaatScriptPackage *package, const char *name, int bool_value);

#endif
