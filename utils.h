#ifndef UTILS_H
#define UTILS_H

// clang-format off
#include <X11/Xlib.h>
// clang-format on
#include "stb_ds.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// TODO(fonsi): remove in release build
#define XINERAMA

#if defined(XINERAMA)
#include <X11/extensions/Xinerama.h>
#endif

#define internal static
#define global   static

/* icccm atoms */
enum { WM_PROTOCOLS, WM_STATE, WM_TAKE_FOCUS, WM_DELETE, WM_LAST };
/* ewmh atoms */
enum {
    NET_WM_FULLSCREEN,
    NET_WM_NAME,
    NET_WM_STATE,
    NET_WM_CHECK,
    NET_WM_SUPPORTED,
    NET_WM_ACTIVE_WINDOW,
    NET_WM_WINDOW_TYPE,
    NET_WM_CLIENT_LIST,
    NET_WM_LAST
};

typedef unsigned int  uint;
typedef unsigned long ulong;

typedef struct Root {
    Window window;
    int    x, y;
    uint   width, height;
    uint   layout;
} root_t;

typedef union Arguments {
    const void* v;
    int         i;
} arg_t;

typedef struct Key {
    uint   modifiers;
    KeySym keysym;
    void (*func)(arg_t args);
    arg_t arguments;
} keybind_t;

typedef struct Client client_t;
struct Client {
    Window    window;
    int       x, y, w, h;
    client_t* next;
    uint      bw;
};

typedef struct Monitor {
    client_t* clients;
    int       focus_pos; // TODO(fonsi): remove focuspos from monitor and move it to wm_t
    int       x_orig;
    int       y_orig;
    uint      width;
    uint      height;
} monitor_t;

typedef struct WindowManager {
    monitor_t* monitors;
    Window     active;
} wm_t;

#endif
