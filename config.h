#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#ifdef __APPLE__
const char* term[] = {"open", "-a", "iterm", NULL};
const char* editor[] = {"neovide", NULL};
#else
const char* term[] = {"alacritty", NULL};
const char* editor[] = {"neovide", NULL};
#endif

#define SHIFT ShiftMask
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SUPER Mod2Mask

static keybind_t keys[] = {
    {ALT, XK_Return, spawn, {.v = term}},
    {ALT, XK_e, spawn, {.v = editor}}
};

#endif
