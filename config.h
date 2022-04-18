#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

/* appearance */
static const uint  border_width          = 2;
static const ulong border_color_inactive = 0x00FF00;
static const ulong border_color_active   = 0xFF0000;

/* application shell cmd */
const char* term[]     = {"alacritty", NULL};
const char* editor[]   = {"emacsclient -nc -a 'emacs'", NULL};
const char* launcher[] = {"dmenu_run", NULL};
const char* search[]   = {"google-chrome-stable", NULL};

/* key modifiers mask definitions */
#define SHIFT ShiftMask
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SUPER Mod2Mask

/* global keybindings */
// { modifiers, key, function, arguments }
static keybind_t keys[] = {
    {        ALT, XK_Return,        spawn,     {.v = term}},
    {        ALT,      XK_n,        spawn,   {.v = editor}},
    {        ALT,      XK_d,        spawn, {.v = launcher}},
    {        ALT,      XK_g,        spawn,   {.v = search}},
    {        ALT, XK_Escape,      setquit,     {.v = NULL}},
    {        ALT,      XK_j,  changefocus,       {.i = -1}},
    {        ALT,      XK_k,  changefocus,        {.i = 1}},
    {        ALT,      XK_w, deleteclient,     {.v = NULL}},
    {ALT | SHIFT,      XK_j,  cycleclient,       {.i = -1}},
    {ALT | SHIFT,      XK_k,  cycleclient,        {.i = 1}},
};

#endif
