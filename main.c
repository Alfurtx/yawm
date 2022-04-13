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
internal void      resizeclient(client_t* client, int x, int y, int w, int h, uint bw);
internal int       getclientpos(Window window);
internal void      manageclient(Window window);
internal void      unmanageclient(Window window);
internal void      setquit(arg_t args);
internal void      focus(Window window);
internal void      changefocus(arg_t args);
internal void      grabbuttons(Window window);

internal int xerror(Display* display, XErrorEvent* error);
internal int xiniterror(Display* display, XErrorEvent* error);

internal void buttonpress(XEvent* event);
internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);
internal void keypress(XEvent* event);
internal void (*handler[LASTEvent])(XEvent*) = {
    [ConfigureRequest] = configurerequest,
    [MapRequest]       = maprequest,
    [UnmapNotify]      = unmapnotify,
    [KeyPress]         = keypress,
    [ButtonPress]      = buttonpress,
};

#include "config.h"

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define BUTTONMASK  (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)
#define CONFMASK    (CWX | CWY | CWWidth | CWHeight | CWBorderWidth)
#define WINDOWMASKS                                                                                \
        (SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | KeyReleaseMask |       \
         ButtonPressMask | ButtonReleaseMask)
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

        monitor.root      = &root;
        monitor.clients   = NULL;
        monitor.focus_pos = -1;

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
        if (!wintoclient(ev->window))
                manageclient(ev->window);
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent* ev = &event->xunmap;
        XUnmapWindow(display, ev->window);
        unmanageclient(ev->window);
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
        uint              count = arrlenu(monitor.clients);

        XGetWindowAttributes(display, monitor.root->window, &wa);

        if (count == 0) {
                return;
        } else if (count == 1) {
                resizeclient(&monitor.clients[0],
                             0,
                             0,
                             wa.width - 2 * border_width,
                             wa.height - 2 * border_width,
                             border_width);
        } else {

                client_t* c  = monitor.clients;
                uint      bw = c->bw;
                uint      ws = wa.width / 2;
                uint      hs = wa.height / (count - 1);
                uint      x  = 0;
                uint      y  = 0;
                resizeclient(c, x, y, ws - 2 * bw, wa.height - 2 * bw, bw);

                // NOTE(fonsi): an excess value is computed to take into account the decimal loss in
                // (height / number of screens) and shared after in between all windows
                uint excess = hs - 2 * bw;
                for (uint i = 1; i < count - 1; i++) {
                        excess += (hs - bw) + bw;
                }
                excess += 2 * bw;
                excess = wa.height - excess;

                for (uint i = 0; i < count - 1; i++) {
                        c  = &monitor.clients[i + 1];
                        bw = c->bw;
                        x  = ws - bw;
                        y  = hs * i;

                        // NOTE(fonsi): first vertical window should have all borders, then next
                        // ones can move to overlap them
                        if (i == 0) {
                                resizeclient(c, x, y, ws - 1 * bw, hs - 2 * bw, bw);
                        } else if (i == count - 2) {
                                resizeclient(c, x, y - bw, ws - 1 * bw, hs - bw + excess, bw);
                        } else {
                                resizeclient(c, x, y - bw, ws - 1 * bw, hs - 1 * bw, bw);
                        }
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
resizeclient(client_t* client, int x, int y, int w, int h, uint bw)
{
        XWindowChanges wc;

        client->x = wc.x = x;
        client->y = wc.y = y;
        client->w = wc.width = w;
        client->h = wc.height = h;
        client->bw = wc.border_width = bw;

        XConfigureWindow(display, client->window, CONFMASK, &wc);
        XMapWindow(display, client->window);
        XSync(display, False);
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

internal void
manageclient(Window window)
{
        XWindowAttributes wa;
        XGetWindowAttributes(display, window, &wa);

        client_t c = {.window = window,
                      .x      = wa.x,
                      .y      = wa.y,
                      .w      = wa.width,
                      .h      = wa.height,
                      .bw     = border_width};

        XSetWindowBorder(display, window, border_color_inactive);
        arrput(monitor.clients, c);
        rearrange();
        XMapWindow(display, c.window);
        grabbuttons(window);
        focus(window);
}

internal void
unmanageclient(Window window)
{
        int pos = getclientpos(window);
        if (pos != -1) {
                arrdel(monitor.clients, pos);
                rearrange();
        }
}

internal void
setquit(arg_t args)
{
        quit = true;
}

internal void
focus(Window window)
{
        XSetInputFocus(display, window, RevertToNone, CurrentTime);
        XSetWindowBorder(display, window, border_color_active);
        for (uint i = 0; i < arrlenu(monitor.clients); i++) {
                if (monitor.clients[i].window != window) {
                        XSetWindowBorder(display, monitor.clients[i].window, border_color_inactive);
                } else {
                        monitor.focus_pos = i;
                }
        }
}

internal void
changefocus(arg_t args)
{
        if (arrlenu(monitor.clients) == 0)
                return;

        int aux = monitor.focus_pos + args.i;
        if (aux < 0)
                aux = arrlenu(monitor.clients) - 1;
        if (aux > (int) arrlenu(monitor.clients) - 1)
                aux = 0;

        focus(monitor.clients[aux].window);
}

internal void
buttonpress(XEvent* event)
{
        XButtonPressedEvent* ev = &event->xbutton;
        focus(ev->window);
}

internal void
grabbuttons(Window window)
{
        XUngrabButton(display, AnyButton, AnyModifier, window);
        XGrabButton(display,
                    AnyButton,
                    AnyModifier,
                    window,
                    False,
                    BUTTONMASK,
                    GrabModeAsync,
                    GrabModeSync,
                    None,
                    None);
}
