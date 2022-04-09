#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#ifdef __APPLE__
const char* term[] = {"open", "-a", "iterm", NULL};
#else
const char* term[] = {"alacritty", NULL};
#endif

const char* editor[] = {"neovide", NULL};

#define SHIFT ShiftMask
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SUPER Mod2Mask

static keybind_t keys[] = {
    {ALT, XK_Return, spawn,   {.v = term}},
    {ALT,      XK_n, spawn, {.v = editor}}
};

#endif
