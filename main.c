#include "utils.h"

global int       screen;
global Display*  display;
global bool      quit;
global root_t    root;
global bool      windowtest = false;
global monitor_t monitor;

// TODO(fonsi): figure out if i need this
// global client_t clients;

// TODO(fonsi): remove clientdel if not called apart from unmapnotify

internal void      start(void);
internal void      clean(void);
internal void      loop(void);
internal void      die(char* msg, int errorcode);
internal void      checkotherwm(void);
internal void      spawn(arg_t args);
internal void      testwindow(void);
internal client_t* wintoclient(Window window);
internal void      rearrange(void);
internal void      delclient(client_t* client);
internal void      addclient(client_t* client);

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
        rearrange();
        loop();
        clean();
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

        monitor.root = &root;

        XSelectInput(display, root.window, WINDOWMASKS);
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
        client_t* tmp;
        for (client_t* c = monitor.clients; c;) {
                tmp = c;
                c   = c->next;
                free(tmp);
        }
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent*) event;
        XMapWindow(display, ev->window);

        XWindowAttributes wa;
        XGetWindowAttributes(display, ev->window, &wa);

        client_t* c = calloc(1, sizeof(*c));
        c->window   = ev->window;
        c->next     = NULL;
        c->x        = wa.x;
        c->y        = wa.y;
        c->w        = wa.width;
        c->h        = wa.height;

        addclient(c);
        rearrange();
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent* ev = &event->xunmap;
        XUnmapWindow(display, ev->window);

        client_t* tmp = monitor.clients;

        if (tmp->window == ev->window) {
                monitor.clients = tmp->next;
                free(tmp);
                tmp = NULL;
                return;
        }

        for (client_t* c = tmp->next; c; c = c->next) {
                if (c->window == ev->window) {
                        tmp->next = c->next;
                        free(c);
                        c = NULL;
                        return;
                }
                tmp = c;
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
        if (!monitor.clients)
                return (NULL);

        for (client_t* c = monitor.clients; c != NULL; c = c->next) {
                if (c->window == window)
                        return (c);
        }

        return (NULL);
}

internal void
delclient(client_t* client)
{
        client_t* tmp = monitor.clients;

        if (tmp->window == client->window) {
                monitor.clients = tmp->next;
                free(tmp);
                tmp = NULL;
                return;
        }

        for (client_t* c = tmp->next; c; c = c->next) {
                if (c->window == client->window) {
                        tmp->next = c->next;
                        free(c);
                        c = NULL;
                        return;
                }
                tmp = c;
        }
}

internal void
rearrange(void)
{
        client_t* c;
        uint      count = 0;

        XWindowAttributes wa;
        XGetWindowAttributes(display, monitor.root->window, &wa);

        for (c = monitor.clients; c; c = c->next) {
                count++;
        }

        if (count == 0) {
                return;
        } else if (count == 1) {
                XMoveResizeWindow(display, monitor.clients->window, 0, 0, wa.width, wa.height);
        } else {
                uint i = 0;
                for (c = monitor.clients; c; c = c->next) {
                        if (i == 0) {
                                XMoveResizeWindow(
                                    display, c->window, 0, 0, wa.width / 2, wa.height);
                        } else {
                                XMoveResizeWindow(display,
                                                  c->window,
                                                  wa.width / 2,
                                                  (wa.height / count) * i,
                                                  wa.width / 2,
                                                  wa.height / count);
                        }
                        i++;
                }
        }
}

internal void
configurerequest(XEvent* event)
{
        XConfigureRequestEvent* ev = &event->xconfigurerequest;

        XWindowChanges changes;
        changes.x            = ev->x;
        changes.y            = ev->y;
        changes.width        = ev->width;
        changes.height       = ev->height;
        changes.border_width = ev->border_width;
        changes.sibling      = ev->above;
        changes.stack_mode   = ev->detail;

        XConfigureWindow(display, ev->window, ev->value_mask, &changes);
        rearrange();
}

internal void
addclient(client_t* client)
{
        client_t* c = monitor.clients;

        while (c) {
                c = c->next;
        }
        c = client;
}
