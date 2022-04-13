#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#ifdef __APPLE__
const char* term[] = {"open", "-a", "iterm", NULL};
#else
const char* term[] = {"alacritty", NULL};
#endif

const char* editor[]   = {"neovide", NULL};
const char* launcher[] = {"dmenu_run", NULL};

#define SHIFT ShiftMask
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SUPER Mod2Mask

// global keybindings
// { modifiers, key, function, arguments }
static keybind_t keys[] = {
    {ALT, XK_Return,   spawn,     {.v = term}},
    {ALT,      XK_n,   spawn,   {.v = editor}},
    {ALT,  XK_space,   spawn, {.v = launcher}},
    {ALT, XK_Escape, setquit,     {.v = NULL}},
};

// border width of each window, in pixels
static const uint border_width = 2;
// border color of inactive window (format: 0xRRGGBB)
static const ulong border_color = 0x00FF00;

#endif
