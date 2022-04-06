#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#ifdef __APPLE__
const char* term[] = {"open", "-a", "iterm", NULL};
#else
const char* term[] = {"alacritty", NULL};
#endif

#define SHIFT ShiftMask
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SUPER Mod2Mask

static keybind_t keys[] = {
    {SUPER, XK_Return, spawn, {.v = term}}
};

#endif
