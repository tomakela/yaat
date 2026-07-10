#include "script_bytecode.h"
#include <stdio.h>
#include <string.h>

static void w16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void w32(FILE *f, unsigned long v) { w16(f, (unsigned)v); w16(f, (unsigned)(v >> 16)); }
static int r16(FILE *f, unsigned *v) { int a = fgetc(f), b = fgetc(f); if (a < 0 || b < 0) return 0; *v = (unsigned)(a | (b << 8)); return 1; }
static int r32(FILE *f, unsigned long *v) { unsigned a, b; if (!r16(f, &a) || !r16(f, &b)) return 0; *v = (unsigned long)a | ((unsigned long)b << 16); return 1; }

static void ws(FILE *f, const char *s, int n)
{
    int i;
    for (i = 0; i < n && s[i]; ++i) fputc((unsigned char)s[i], f);
    for (; i < n; ++i) fputc(0, f);
}

static int rs(FILE *f, char *s, int n)
{
    if (fread(s, 1, (size_t)n, f) != (size_t)n) return 0;
    s[n - 1] = 0;
    return 1;
}

static void we(FILE *f, const YaatEvent *e)
{
    ws(f, e->name, 32);
    ws(f, e->item, 32);
    w16(f, (unsigned)e->first_command);
    w16(f, (unsigned)e->command_count);
}

static int re(FILE *f, YaatEvent *e)
{
    unsigned v;
    if (!rs(f, e->name, 32) || !rs(f, e->item, 32) || !r16(f, &v)) return 0;
    e->first_command = (int)v;
    if (!r16(f, &v)) return 0;
    e->command_count = (int)v;
    return 1;
}

static int valid_event(const YaatEvent *e, const YaatScriptPackage *p)
{
    if (e->command_count < 0 || e->first_command < 0) return 0;
    if (e->command_count == 0) return e->first_command <= p->command_count;
    if (e->first_command >= p->command_count) return 0;
    return e->command_count <= p->command_count - e->first_command;
}

static void wc(FILE *f, const YaatCommand *c)
{
    w16(f, (unsigned)c->kind);
    ws(f, c->a, 96);
    ws(f, c->b, 96);
    w16(f, (unsigned)c->bool_value);
    w16(f, (unsigned)c->int_value);
    w16(f, (unsigned)c->first_child);
    w16(f, (unsigned)c->child_count);
    w16(f, (unsigned)c->first_else_child);
    w16(f, (unsigned)c->else_child_count);
}

static int rc(FILE *f, YaatCommand *c)
{
    unsigned v;
    if (!r16(f, &v)) return 0;
    c->kind = (YaatCommandKind)v;
    if (!rs(f, c->a, 96) || !rs(f, c->b, 96) || !r16(f, &v)) return 0;
    c->bool_value = (int)v;
    if (!r16(f, &v)) return 0;
    c->int_value = (int)v;
    if (!r16(f, &v)) return 0;
    c->first_child = (int)v;
    if (!r16(f, &v)) return 0;
    c->child_count = (int)v;
    if (!r16(f, &v)) return 0;
    c->first_else_child = (int)v;
    if (!r16(f, &v)) return 0;
    c->else_child_count = (int)v;
    return 1;
}

static int valid_command(const YaatCommand *c, const YaatScriptPackage *p)
{
    if (c->kind < YAAT_CMD_SAY || c->kind > YAAT_CMD_SHAKE) return 0;
    if (c->child_count < 0 || c->first_child < 0 ||
        c->else_child_count < 0 || c->first_else_child < 0) return 0;
    if (c->child_count > 0 &&
        (c->first_child >= p->command_count || c->child_count > p->command_count - c->first_child)) return 0;
    if (c->else_child_count > 0 &&
        (c->first_else_child >= p->command_count || c->else_child_count > p->command_count - c->first_else_child)) return 0;
    return 1;
}

int yaat_bytecode_write_file(const char *path, const YaatScriptPackage *p)
{
    FILE *f;
    int i, j, k;
    if (!path || !p) return 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    fwrite("YAATBC\0\0", 1, 8, f);
    w16(f, YAAT_BYTECODE_VERSION);
    w16(f, 0);
    w16(f, (unsigned)p->var_count);
    w16(f, (unsigned)p->room_count);
    w16(f, (unsigned)p->command_count);
    w16(f, 0);
    for (i = 0; i < p->var_count; ++i) { ws(f, p->vars[i].name, 32); w16(f, (unsigned)p->vars[i].bool_value); }
    for (i = 0; i < p->command_count; ++i) wc(f, &p->commands[i]);
    for (i = 0; i < p->room_count; ++i) {
        const YaatRoom *r = &p->rooms[i];
        ws(f, r->id, 32); ws(f, r->label, 64); w32(f, r->color);
        w16(f, (unsigned)r->event_count); w16(f, (unsigned)r->entity_count);
        for (j = 0; j < r->event_count; ++j) we(f, &r->events[j]);
        for (j = 0; j < r->entity_count; ++j) {
            const YaatEntity *e = &r->entities[j];
            w16(f, (unsigned)e->kind); ws(f, e->id, 32); ws(f, e->name, 64);
            w16(f, (unsigned)e->x); w16(f, (unsigned)e->y); w16(f, (unsigned)e->w); w16(f, (unsigned)e->h);
            w16(f, (unsigned)e->visible); w16(f, (unsigned)e->event_count);
            for (k = 0; k < e->event_count; ++k) we(f, &e->events[k]);
        }
    }
    fclose(f);
    return 1;
}

int yaat_bytecode_read_file(const char *path, YaatScriptPackage *p)
{
    FILE *f;
    char m[8];
    unsigned ver, flags, vc, rcount, cc, res, v;
    int i, j, k;
    unsigned long ul;
    if (!path || !p) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    yaat_script_package_init(p);
    if (fread(m, 1, 8, f) != 8 || memcmp(m, "YAATBC\0\0", 8) ||
        !r16(f, &ver) || ver != YAAT_BYTECODE_VERSION || !r16(f, &flags) ||
        !r16(f, &vc) || !r16(f, &rcount) || !r16(f, &cc) || !r16(f, &res)) { fclose(f); return 0; }
    if (vc > YAAT_MAX_VARS || rcount > YAAT_MAX_ROOMS || cc > YAAT_MAX_COMMANDS) { fclose(f); return 0; }
    p->var_count = (int)vc;
    p->room_count = (int)rcount;
    p->command_count = (int)cc;
    for (i = 0; i < p->var_count; ++i) {
        if (!rs(f, p->vars[i].name, 32) || !r16(f, &v)) { fclose(f); return 0; }
        p->vars[i].bool_value = (int)v;
    }
    for (i = 0; i < p->command_count; ++i) {
        if (!rc(f, &p->commands[i]) || !valid_command(&p->commands[i], p)) { fclose(f); return 0; }
    }
    for (i = 0; i < p->room_count; ++i) {
        YaatRoom *r = &p->rooms[i];
        if (!rs(f, r->id, 32) || !rs(f, r->label, 64) || !r32(f, &ul) || !r16(f, &v)) { fclose(f); return 0; }
        r->color = ul;
        if (v > YAAT_MAX_EVENTS) { fclose(f); return 0; }
        r->event_count = (int)v;
        if (!r16(f, &v) || v > YAAT_MAX_ENTITIES) { fclose(f); return 0; }
        r->entity_count = (int)v;
        for (j = 0; j < r->event_count; ++j) {
            if (!re(f, &r->events[j]) || !valid_event(&r->events[j], p)) { fclose(f); return 0; }
        }
        for (j = 0; j < r->entity_count; ++j) {
            YaatEntity *e = &r->entities[j];
            if (!r16(f, &v) || v > YAAT_ENTITY_OBJECT) { fclose(f); return 0; }
            e->kind = (YaatEntityKind)v;
            if (!rs(f, e->id, 32) || !rs(f, e->name, 64) || !r16(f, &v)) { fclose(f); return 0; }
            e->x = (int)v;
            if (!r16(f, &v)) { fclose(f); return 0; }
            e->y = (int)v;
            if (!r16(f, &v)) { fclose(f); return 0; }
            e->w = (int)v;
            if (!r16(f, &v)) { fclose(f); return 0; }
            e->h = (int)v;
            if (!r16(f, &v)) { fclose(f); return 0; }
            e->visible = (int)v;
            if (!r16(f, &v) || v > YAAT_MAX_EVENTS) { fclose(f); return 0; }
            e->event_count = (int)v;
            for (k = 0; k < e->event_count; ++k) {
                if (!re(f, &e->events[k]) || !valid_event(&e->events[k], p)) { fclose(f); return 0; }
            }
        }
    }
    fclose(f);
    return 1;
}
