#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#ifdef __APPLE__

const char* term[] = {"open", "-a", "iterm", NULL};

#endif

// TODO(fonsi) find SUPER or whatever it's called key and define it
#define SHIFT ShiftMask
#define ALT Mod1Mask
#define CTRL ControlMask


static keybind_t keys[] = {
        {ShiftMask, XK_Return, spawn, {.v = term}}
};

#endif
