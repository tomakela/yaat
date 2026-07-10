#include "script_package.h"
#include <string.h>

void yaat_script_package_init(YaatScriptPackage *package) { if (package) memset(package, 0, sizeof(*package)); }

static void yaat_copy_value_string(char *dst, int dst_size, const char *src)
{
    if (dst_size <= 0) return;
    if (!src) src = "";
    strncpy(dst, src, (size_t)dst_size - 1);
    dst[dst_size - 1] = '\0';
}

YaatValue yaat_value_bool(int value)
{
    YaatValue result;
    memset(&result, 0, sizeof(result));
    result.kind = YAAT_VALUE_BOOL;
    result.bool_value = value ? 1 : 0;
    result.int_value = result.bool_value;
    return result;
}

YaatValue yaat_value_int(int value)
{
    YaatValue result;
    memset(&result, 0, sizeof(result));
    result.kind = YAAT_VALUE_INT;
    result.int_value = value;
    result.bool_value = value != 0;
    return result;
}

YaatValue yaat_value_string(const char *value)
{
    YaatValue result;
    memset(&result, 0, sizeof(result));
    result.kind = YAAT_VALUE_STRING;
    yaat_copy_value_string(result.string_value, sizeof(result.string_value), value);
    result.bool_value = result.string_value[0] != '\0';
    return result;
}

int yaat_script_package_set_var_value(YaatScriptPackage *package, const char *name, const YaatValue *value)
{
    int i;
    if (!package || !name || !value) return 0;
    for (i = 0; i < package->var_count; ++i) {
        if (strcmp(package->vars[i].name, name) == 0) { package->vars[i].value = *value; return 1; }
    }
    if (package->var_count >= YAAT_MAX_VARS) return 0;
    strncpy(package->vars[package->var_count].name, name, sizeof(package->vars[package->var_count].name)-1);
    package->vars[package->var_count].name[sizeof(package->vars[package->var_count].name)-1] = '\0';
    package->vars[package->var_count].value = *value;
    package->var_count++;
    return 1;
}

int yaat_script_package_set_var(YaatScriptPackage *package, const char *name, int bool_value)
{
    YaatValue value = yaat_value_bool(bool_value);
    return yaat_script_package_set_var_value(package, name, &value);
}
