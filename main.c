#include "utils.h"

global int      screen;
global Display* display;
global bool     quit;
global root_t   root;

// TODO(fonsi): figure out if i need this
// global client_t clients;

internal void start(void);
internal void clean(void);
internal void loop(void);
internal void die(char* msg, int errorcode);
internal void checkotherwm(void);

internal int xerror(Display* display, XErrorEvent* error);
internal int xiniterror(Display* display, XErrorEvent* error);

internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);
internal void resizerequest(XEvent* event);
internal void keypress(XEvent* event);

internal void (*handler[LASTEvent])(XEvent*) = {
    // [ConfigureRequest] = configurerequest,
    [MapRequest]       = maprequest,
    [UnmapNotify]      = unmapnotify,
    // [ResizeRequest]    = resizerequest,
    // [KeyPress]         = keypress,
};

int
main(void)
{
        start();
        return (EXIT_SUCCESS);
}

internal void
start(void)
{
        display = XOpenDisplay(0);
        screen  = XDefaultScreen(display);

        root.window = XDefaultRootWindow(display);
        root.x      = 0;
        root.y      = 0;
        root.width  = XDisplayWidth(display, screen);
        root.height = XDisplayHeight(display, screen);

        checkotherwm();

        XSelectInput(display, root.window, SubstructureRedirectMask | SubstructureNotifyMask);
}

internal void
loop(void)
{
        XEvent e;
        while (!XNextEvent(display, &e) && !quit) {
                if (handler[e.type])
                        handler[e.type](&e);
        }
}

internal void
clean(void)
{
        XCloseDisplay(display);
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent*) event;
        XMapWindow(display, ev->window);
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent* ev = &event->xunmap;
        XUnmapWindow(display, ev->window);
}

internal int
xiniterror(Display* display, XErrorEvent* error)
{
        if (error->error_code == BadAccess) {
                die("Another Window Manager is already active", error->error_code);
                exit(EXIT_FAILURE);
        }

        return (EXIT_SUCCESS);
}

internal void
die(char* msg, int errorcode)
{
        char str[256];
        XGetErrorText(display, errorcode, str, 256);
        fprintf(stderr, "[yawm] ERROR %d\nMESSAGE: %s\nHINT: %s\n", errorcode, str, msg);
}

internal void
checkotherwm(void)
{
        XSetErrorHandler(xiniterror);
        XSelectInput(display, root.window, SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(display, False);
        // XSetErrorHandler(xerror);
        XSync(display, False);
}
