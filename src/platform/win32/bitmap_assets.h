#ifndef YAAT_PLATFORM_WIN32_BITMAP_ASSETS_H
#define YAAT_PLATFORM_WIN32_BITMAP_ASSETS_H

#include "runtime/asset_loader.h"

typedef struct YaatBitmap { unsigned long *pixels; int width; int height; int has_alpha; char path[YAAT_ASSET_MAX_PATH * 2]; } YaatBitmap;

#endif
