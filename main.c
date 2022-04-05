#include "utils.h"

global int         screen;
global Display*    display;
global bool        quit;
global root_t root;

global client_t clients; // TODO(fonsi): figure out if i need this

internal void start(void);
internal void clean(void);
internal void loop(void);
internal int  xerror(Display* display, XErrorEvent* error);

internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);

internal void (*handler[LASTEvent])(XEvent*) = {
    [ConfigureRequest] = configurerequest,
    [MapRequest]       = maprequest,
    [UnmapNotify]      = unmapnotify,
};

int
main(void)
{
        return (EXIT_SUCCESS);
}

internal void
start(void)
{
        display = XOpenDisplay(0);
        screen = XDefaultScreen(display);

        root.window = XDefaultRootWindow(display);
        root.x = 0;
        root.y = 0;
        root.width = XDisplayWidth(display, screen);
        root.height = XDisplayHeight(display, screen);

        XSelectInput(display, root.window, SubstructureRedirectMask | SubstructureNotifyMask);
}

internal void
loop(void)
{
        XEvent e;
        while(!XNextEvent(display, &e) && !quit)
        {
                if(handler[e.type])
                        handler[e.type](&e);
        }
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent *) event;
        XMapWindow(display, ev->window);
}

internal void
configurerequest(XEvent* event)
{
}

internal void
unmapnotify(XEvent* event)
{
}
