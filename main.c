#include "utils.h"

global int       screen;
global Display*  display;
global bool      quit;
global root_t    root;
global bool      windowtest = false;
global monitor_t monitor;

internal void      start(void);
internal void      clean(void);
internal void      loop(void);
internal void      die(char* msg, int errorcode);
internal void      checkotherwm(void);
internal void      spawn(arg_t args);
internal void      testwindow(void);
internal client_t* wintoclient(Window window);
internal void      rearrange(void);
internal void      grabkeys(void);
internal void      resizeclient(client_t* client, int x, int y, int w, int h);
internal int       getclientpos(Window window);

internal int xerror(Display* display, XErrorEvent* error);
internal int xiniterror(Display* display, XErrorEvent* error);

internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);
internal void keypress(XEvent* event);
internal void (*handler[LASTEvent])(XEvent*) = {
    [ConfigureRequest] = configurerequest,
    [MapRequest]       = maprequest,
    [UnmapNotify]      = unmapnotify,
    [KeyPress]         = keypress,
};

#include "config.h"

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define WINDOWMASKS                                                                                \
        (SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | KeyReleaseMask |       \
         PointerMotionMask)
#define MODMASK(mask)                                                                              \
        (mask & (ShiftMask | LockMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |  \
                 Mod5Mask))

int
main(void)
{
        start();
        loop();
        clean();
        return (EXIT_SUCCESS);
}

internal void
start(void)
{
        printf("test");
        display = XOpenDisplay(0);
        screen  = XDefaultScreen(display);

        printf("test");
        root.window = XDefaultRootWindow(display);
        root.x      = 0;
        root.y      = 0;
        root.width  = XDisplayWidth(display, screen);
        root.height = XDisplayHeight(display, screen);

        printf("test");
        checkotherwm();

        printf("test");
        monitor.root    = &root;
        monitor.clients = NULL;

        printf("test");
        XSelectInput(display, root.window, WINDOWMASKS);
        grabkeys();
        printf("test");
}

internal void
loop(void)
{
        XEvent e;
        while (!quit && !XNextEvent(display, &e)) {
                if (handler[e.type])
                        handler[e.type](&e);
        }
}

internal void
clean(void)
{
        XCloseDisplay(display);
        arrfree(monitor.clients);
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent*) event;
        XWindowAttributes wa;

        if (!XGetWindowAttributes(display, ev->window, &wa))
                return;
        if (wa.override_redirect)
                return;

        if (!wintoclient(ev->window)) {
                client_t c = {
                    .window = ev->window,
                    .h      = wa.height,
                    .w      = wa.width,
                    .x      = wa.x,
                    .y      = wa.y,
                    .next   = NULL,
                };

                arrput(monitor.clients, c);
                rearrange();
                XMapWindow(display, c.window);
        }
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent* ev = &event->xunmap;
        XUnmapWindow(display, ev->window);

        int pos = getclientpos(ev->window);
        if (pos != -1) {
                arrdel(monitor.clients, pos);
                rearrange();
        }
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
        XSetErrorHandler(xerror);
        XSync(display, False);
}

internal int
xerror(Display* display, XErrorEvent* error)
{
        die("", error->error_code);
        return (EXIT_SUCCESS);
}

internal void
keypress(XEvent* event)
{
        XKeyPressedEvent* ev     = (XKeyPressedEvent*) event;
        KeySym            keysym = XLookupKeysym(ev, 0);

        for (uint i = 0; i < ARRLEN(keys); i++) {
                if (keys[i].keysym == keysym && MODMASK(keys[i].modifiers) == MODMASK(ev->state)) {
                        keys[i].func(keys[i].arguments);
                }
        }
}

internal void
spawn(arg_t args)
{
        if (fork() == 0) {
                setsid();
                execvp(((const char**) args.v)[0], (char**) args.v);
                perror("execvp");
                exit(EXIT_FAILURE);
        }
}

internal void
testwindow(void)
{
        windowtest = true;
        display    = XOpenDisplay(NULL);
        screen     = XDefaultScreen(display);

        root.window = XDefaultRootWindow(display);
        root.x      = 0;
        root.y      = 0;
        root.width  = XDisplayWidth(display, screen);
        root.height = XDisplayHeight(display, screen);

        XWindowAttributes attrs;
        XGetWindowAttributes(display, root.window, &attrs);

        const Window frame = XCreateSimpleWindow(display,
                                                 root.window,
                                                 attrs.x,
                                                 attrs.y,
                                                 attrs.width,
                                                 attrs.height,
                                                 30,
                                                 0xff0000,
                                                 0xff0000);

        XSelectInput(display, frame, KeyPressMask);
        XMapWindow(display, frame);
        XSync(display, frame);

        loop();

        XUnmapWindow(display, frame);
        XDestroyWindow(display, frame);
        clean();
}

internal client_t*
wintoclient(Window window)
{
        for (uint i = 0; i < arrlenu(monitor.clients); i++) {
                if (monitor.clients[i].window == window)
                        return (&monitor.clients[i]);
        }
        return (NULL);
}

internal void
rearrange(void)
{
        XWindowAttributes wa;
        XGetWindowAttributes(display, monitor.root->window, &wa);

        uint count = arrlenu(monitor.clients);
        fprintf(stderr, "REARRANGE ->\nCLIENT COUNT: %d\n-----\n", count);

        if (count == 0) {
                return;
        } else if (count == 1) {
                resizeclient(&monitor.clients[0], 0, 0, wa.width, wa.height);
        } else {
                uint ws = wa.width / 2;
                uint hs = wa.height / (count - 1);
                resizeclient(&monitor.clients[0], 0, 0, ws, wa.height);
                for (uint i = 0; i < count - 1; i++) {
                        resizeclient(&monitor.clients[i + 1], ws, hs * i, ws, hs);
                }
        }

        // for (uint i = 0; i < count; i++) {
        // }

        XSync(display, False);
}

internal void
configurerequest(XEvent* event)
{
        XConfigureRequestEvent* ev = &event->xconfigurerequest;
        XWindowChanges          wc;
        wc.x          = ev->x;
        wc.y          = ev->y;
        wc.width      = ev->width;
        wc.height     = ev->height;
        wc.sibling    = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(display, ev->window, ev->value_mask, &wc);
}

internal void
grabkeys(void)
{
        KeyCode code;
        XUngrabKey(display, AnyKey, AnyModifier, root.window);
        for (uint i = 0; i < ARRLEN(keys); i++) {
                if ((code = XKeysymToKeycode(display, keys[i].keysym))) {
                        XGrabKey(display,
                                 code,
                                 keys[i].modifiers,
                                 root.window,
                                 True,
                                 GrabModeAsync,
                                 GrabModeAsync);
                }
        }
}

internal void
resizeclient(client_t* client, int x, int y, int w, int h)
{
        XWindowChanges wc;
        XTextProperty  t;

        client->x = wc.x = x;
        client->y = wc.y = y;
        client->w = wc.width = w;
        client->h = wc.height = h;

        XConfigureWindow(display, client->window, CWX | CWY | CWHeight | CWWidth, &wc);
        XGetWMName(display, client->window, &t);
        XMapWindow(display, client->window);
        XSync(display, False);

        fprintf(stderr,
                "RESIZE IN CLIENT %s:\nx: %d, y: %d, w: %d, h: %d\n=====\n",
                t.value,
                client->x,
                client->y,
                client->w,
                client->h);
}

internal int
getclientpos(Window window)
{
        for (uint i = 0; i < arrlenu(monitor.clients); i++) {
                if (monitor.clients[i].window == window)
                        return (i);
        }
        return (-1);
}
