# Yet Another Window Manager
---
This is meant to be a short toy project to learn how to create a window manager. 
The code is heavily inspired by [dwm](https://dwm.suckless.org/)'s source code, and uses Xlib as the x11 development library.

### How to build
---
The dependencies to build this project are:
- make
- clang
- xlib (In Debian/Ubuntu 'libx11-dev' and in Archlinux 'libx11')

Afterwards, you would only need to do:
    make

### How to test it
---
To test it I used Xephyr. (That's what's in the makefile as well). And you would only need to run:
    make test
    make run
And a window should pop up, with yawm running inside of it. The final 'make run' just launches yawm, so you would need to abort it to close it.
However, Xephyr would still be running, so you have to kill it as well with 'kill' or 'killall'. Although, for convenience, theres is also a command:
    make freetest
that kills all isntances of Xephyr.

### Resources I used to learn about X11 Window Managers
---
[How X Window Managers work and how to write one](https://jichu4n.com/posts/how-x-window-managers-work-and-how-to-write-one-part-i/)
[XLib Programming Manual](https://tronche.com/gui/x/xlib/)
