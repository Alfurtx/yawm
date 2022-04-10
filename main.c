#include "utils.h"

global int       screen;
global Display*  display;
global bool      quit;
global root_t    root;
global bool      windowtest = false;
global monitor_t monitor;

// TODO(fonsi): figure out if i need this
// global client_t clients;

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

internal int xerror(Display* display, XErrorEvent* error);
internal int xiniterror(Display* display, XErrorEvent* error);

internal void destroynotify(XEvent* event);
internal void createnotify(XEvent* event);
internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);
internal void keypress(XEvent* event);
internal void (*handler[LASTEvent])(XEvent*) = {
    [DestroyNotify]    = destroynotify,
    [CreateNotify]     = createnotify,
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

        grabkeys();
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

        for (client_t* c = monitor.clients; c;) {
                client_t* tmp = c;
                c             = c->next;
                if (tmp) {
                        free(tmp);
                        tmp = NULL;
                }
        }
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent*) event;
        XMapWindow(display, ev->window);
        rearrange();
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent* ev = &event->xunmap;
        XUnmapWindow(display, ev->window);
        rearrange();
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
        client_t* c = monitor.clients;
        while (c && c->window != window)
                c = c->next;

        return (c);
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

        c = monitor.clients;

        if (count == 0) {
                return;
        } else if (count == 1) {
                resizeclient(c, 0, 0, wa.width, wa.height);
        } else {
                resizeclient(c, 0, 0, wa.width / 2, wa.height);
                uint i = 0;
                for (c = monitor.clients->next; c; c = c->next) {
                        resizeclient(c,
                                     wa.width / 2,
                                     (wa.height / count) * i,
                                     wa.width / 2,
                                     wa.height / count);
                        i++;
                }
        }

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
        rearrange();
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
        client->x = wc.x = x;
        client->y = wc.y = y;
        client->w = wc.width = w;
        client->h = wc.height = h;
        XConfigureWindow(display, client->window, CWX | CWY | CWHeight | CWWidth, &wc);
        XSync(display, False);
}

internal void
createnotify(XEvent* event)
{
        XCreateWindowEvent* ev = &event->xcreatewindow;

        client_t* c = calloc(1, sizeof(*c));
        c->window   = ev->window;
        c->h        = ev->height;
        c->w        = ev->width;
        c->x        = ev->x;
        c->y        = ev->y;
        c->next     = NULL;

        client_t* aux = monitor.clients;

        if (!aux) {
                monitor.clients = c;
                return;
        }

        while (aux->next)
                aux = aux->next;

        aux->next = c;
}

internal void
destroynotify(XEvent* event)
{
        XDestroyWindowEvent* ev = &event->xdestroywindow;

        client_t* c    = monitor.clients;
        client_t* prev = NULL;

        assert(c);

        if (c->window == ev->window) {
                monitor.clients = c->next;
                free(c);
                return;
        }

        while (c && c->window != ev->window) {
                prev = c;
                c    = c->next;
        }

        assert(c);
        prev->next = c->next;
        free(c);
}
