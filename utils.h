#ifndef UTILS_H
#define UTILS_H

#include "stb_ds.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define internal static
#define global   static

enum LayoutType {
        TILING = 0
};

typedef unsigned int uint;

typedef struct Root {
        Window window;
        int    x, y;
        uint   width, height;
        uint   layout;
} root_window;

typedef struct Layout {
        enum LayoutType type;
        Window* clients;
} Layout;

#endif
