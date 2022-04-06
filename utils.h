#ifndef UTILS_H
#define UTILS_H

#include "stb_ds.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define internal static
#define global   static

typedef unsigned int uint;

typedef Window client_t;

typedef struct Root {
        Window window;
        int    x, y;
        uint   width, height;
        uint   layout;
} root_t;

typedef union Arguments {
        const void* v;
} arg_t;

typedef struct Key {
        uint   modifiers;
        KeySym keysym;
        void (*func)(arg_t args);
        arg_t arguments;
} keybind_t;

typedef struct Monitor {
        root_t*   root;
        client_t* clients;
} monitor_t;

#endif
