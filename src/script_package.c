#include "script_package.h"
#include <string.h>

void yaat_script_package_init(YaatScriptPackage *package) { if (package) memset(package, 0, sizeof(*package)); }

int yaat_script_package_set_var(YaatScriptPackage *package, const char *name, int bool_value)
{
    int i;
    if (!package || !name) return 0;
    for (i = 0; i < package->var_count; ++i) {
        if (strcmp(package->vars[i].name, name) == 0) { package->vars[i].bool_value = bool_value; return 1; }
    }
    if (package->var_count >= YAAT_MAX_VARS) return 0;
    strncpy(package->vars[package->var_count].name, name, sizeof(package->vars[package->var_count].name)-1);
    package->vars[package->var_count].bool_value = bool_value;
    package->var_count++;
    return 1;
}
