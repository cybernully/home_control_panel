#pragma once
#include <stddef.h>
#include "module.h"
void module_registry_begin();
size_t module_registry_count();
PanelModule *module_registry_at(size_t index);
PanelModule *module_registry_find(const char *id);
