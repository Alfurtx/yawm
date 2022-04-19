#include "utils.h"

/* global varibles */
global int      screen;
global Display* display;
global bool     quit;
global Window   root;
global wm_t     manager;

/* helper functions */
internal void start(void);
internal void clean(void);
internal void loop(void);
internal void die(char* msg, int errorcode);
internal void checkotherwm(void);
internal void rearrange(void);
internal void grabkeys(void);
internal void focus(Window window);
internal void grabbuttons(Window window, bool focused);
internal void setupatoms(void);
internal void setupnetcheck(void);
internal int  sendevent(client_t* c, Atom atom);
internal void setupmanager(void);
internal void printtest(void);

/* multimonitor geometry helper functions */
internal monitor_t  createmon(XineramaScreenInfo info);
internal void       setupgeometry(void);
internal uint       wintomonpos(Window window);
internal monitor_t* wintomon(Window window);
internal monitor_t* getactivemonitor(void);
internal uint       coordstomonitorpos(uint x, uint y);
internal void       focusmonitor(uint pos);

/* window & client helpers functions */
internal client_t*  wintoclient(Window window);
internal int        wintoclientpos(Window window);
internal monitor_t* clienttomon(client_t* client);
internal void       resizeclient(client_t* client, int x, int y, int w, int h, uint bw);
internal void       manageclient(Window window);
internal void       unmanageclient(Window window);
internal void       swapclients(uint srcpos, uint destpos);
internal Window     getactivewindow(void);

/* command functions */
internal void changemonitorfocus(arg_t args);
internal void deleteclient(arg_t args);
internal void cycleclient(arg_t args);
internal void changefocus(arg_t args);
internal void spawn(arg_t args);
internal void setquit(arg_t args);

/* error handler functions */
internal int xerrordummy(Display* display, XErrorEvent* error);
internal int xerror(Display* display, XErrorEvent* error);
internal int xiniterror(Display* display, XErrorEvent* error);

/* event handler functions */
internal void motionnotify(XEvent* event);
internal void createnotify(XEvent* event);
internal void clientmessage(XEvent* event);
internal void buttonpress(XEvent* event);
internal void configurerequest(XEvent* event);
internal void maprequest(XEvent* event);
internal void unmapnotify(XEvent* event);
internal void keypress(XEvent* event);
internal void (*handler[LASTEvent])(XEvent*) = {
    [CreateNotify] = createnotify,         [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest, [MapRequest] = maprequest,
    [UnmapNotify] = unmapnotify,           [KeyPress] = keypress,
    [ButtonPress] = buttonpress,           [MotionNotify] = motionnotify,
};

#include "config.h"

global Atom wmatoms[WM_LAST];
global Atom netatoms[NET_WM_LAST];

/* definitions */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define BUTTONMASK  (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)
#define CONFMASK    (CWX | CWY | CWWidth | CWHeight | CWBorderWidth)
#define WINDOWMASKS                                                                                \
    (SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | KeyReleaseMask |           \
     ButtonPressMask | ButtonReleaseMask | PointerMotionMask)
#define MODMASK(mask)                                                                              \
    (mask &                                                                                        \
     (ShiftMask | LockMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))

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
    root    = XDefaultRootWindow(display);

    setupmanager();
    setupgeometry();
    checkotherwm();
    XSelectInput(display, root, WINDOWMASKS);
    grabkeys();
    setupatoms();
    setupnetcheck();
}

internal void
loop(void)
{
    XEvent e;
    while(!quit && !XNextEvent(display, &e)) {
        if(handler[e.type])
            handler[e.type](&e);
    }
}

internal void
clean(void)
{
    XCloseDisplay(display);
    for(uint i = 0; i < arrlenu(manager.monitors); i++) {
        if(manager.monitors[i].clients)
            arrfree(manager.monitors[i].clients);
    }
    if(arrlenu(manager.monitors) > 0) {
        arrfree(manager.monitors);
    }
}

internal void
maprequest(XEvent* event)
{
    XMapRequestEvent* ev = (XMapRequestEvent*) event;
    XWindowAttributes wa;

    if(!XGetWindowAttributes(display, ev->window, &wa))
        return;
    if(wa.override_redirect)
        return;
    if(!wintoclient(ev->window)) {
        manageclient(ev->window);
    }
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
    if(error->error_code == BadAccess) {
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
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
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

    for(uint i = 0; i < ARRLEN(keys); i++) {
        if(keys[i].keysym == keysym && MODMASK(keys[i].modifiers) == MODMASK(ev->state)) {
            keys[i].func(keys[i].arguments);
        }
    }
}

internal void
spawn(arg_t args)
{
    if(fork() == 0) {
        setsid();
        execvp(((const char**) args.v)[0], (char**) args.v);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

internal client_t*
wintoclient(Window window)
{
    monitor_t* monitor = wintomon(window);
    if(monitor && monitor->clients) {
        for(uint i = 0; i < arrlenu(monitor->clients); i++) {
            if(monitor->clients[i].window == window) {
                return (&monitor->clients[i]);
            }
        }
    }
    return (NULL);
}

internal void
rearrange(void)
{

    for(uint i = 0; i < arrlenu(manager.monitors); i++) {
        monitor_t* monitor = &manager.monitors[i];
        uint       count   = arrlenu(monitor->clients);

        if(!count)
            return;
        else if(count == 1)
            resizeclient(&monitor->clients[0], monitor->x_orig, monitor->y_orig,
                         monitor->width - 2 * border_width, monitor->height - 2 * border_width,
                         border_width);
        else {
            client_t* c  = monitor->clients;
            uint      bw = c->bw;
            uint      ws = monitor->width / 2;
            uint      hs = monitor->height / (count - 1);
            uint      x  = monitor->x_orig;
            uint      y  = monitor->y_orig;
            resizeclient(c, x, y, ws - 2 * bw, monitor->height - 2 * bw, bw);

            // NOTE(fonsi): excess value accounted for decimal loss in height / numerb of screens,
            // given to last client window
            uint excess = hs - 2 * bw;
            for(uint i = 1; i < count - 1; i++) {
                excess += (hs - bw) + bw;
            }
            excess += 2 * bw;
            excess = monitor->height - excess;

            for(uint i = 0; i < count - 1; i++) {
                c  = &monitor->clients[i + 1];
                bw = c->bw;
                x  = monitor->x_orig + ws - bw;
                y  = monitor->y_orig + hs * i;

                // NOTE(fonsi): first vertical window should have all
                // borders, then next ones can move to overlap them
                if(i == 0) {
                    resizeclient(c, x, y, ws - 1 * bw, hs - 2 * bw, bw);
                } else if(i == count - 2) {
                    resizeclient(c, x, y - bw, ws - 1 * bw, hs - bw + excess, bw);
                } else {
                    resizeclient(c, x, y - bw, ws - 1 * bw, hs - 1 * bw, bw);
                }
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
    XUngrabKey(display, AnyKey, AnyModifier, root);
    for(uint i = 0; i < ARRLEN(keys); i++) {
        if((code = XKeysymToKeycode(display, keys[i].keysym))) {
            XGrabKey(display, code, keys[i].modifiers, root, True, GrabModeAsync, GrabModeAsync);
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

internal void
manageclient(Window window)
{
    XWindowAttributes wa;
    XGetWindowAttributes(display, window, &wa);

    client_t c = {
        .window = window,
        .x      = wa.x,
        .y      = wa.y,
        .w      = wa.width,
        .h      = wa.height,
        .bw     = border_width,
    };

    XSetWindowBorder(display, window, border_color_inactive);
    XSelectInput(display, window,
                 EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    XChangeProperty(display, window, netatoms[NET_WM_CLIENT_LIST], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char*) &window, 1);
    arrput(manager.monitors[manager.active_monitor_pos].clients, c);
    rearrange();
    XMapWindow(display, c.window);
    grabbuttons(window, true);
    focus(window);
}

internal void
unmanageclient(Window window)
{
    monitor_t* monitor = wintomon(window);
    int        pos     = wintoclientpos(window);
    if(pos != -1) {
        XGrabServer(display);
        XSetErrorHandler(xerrordummy);
        arrdel(monitor->clients, pos);
        XSetErrorHandler(xerror);
        XUngrabServer(display);
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
    grabbuttons(window, true);
    monitor_t* monitor = wintomon(window);

    for(uint i = 0; i < arrlenu(monitor->clients); i++) {
        if(monitor->clients[i].window != window) {
            XSetWindowBorder(display, monitor->clients[i].window, border_color_inactive);
            grabbuttons(monitor->clients[i].window, false);
        } else {
            manager.active_window_pos = i;
        }
    }

    XChangeProperty(display, window, netatoms[NET_WM_ACTIVE_WINDOW], XA_WINDOW, PropModeReplace, 32,
                    (unsigned char*) &window, 1);
}

internal void
changefocus(arg_t args)
{
    monitor_t* monitor = &manager.monitors[manager.active_monitor_pos];
    int        count   = arrlenu(monitor->clients);

    if(count == 0)
        return;
    else {
        int aux = manager.active_window_pos + args.i;
        if(aux < 0)
            aux = count - 1;
        if(aux >= count)
            aux = 0;
        focus(monitor->clients[aux].window);
    }
}

internal void
buttonpress(XEvent* event)
{
    XButtonPressedEvent* ev = &event->xbutton;
    focus(ev->window);
}

internal void
grabbuttons(Window window, bool focused)
{
    XUngrabButton(display, AnyButton, AnyModifier, window);

    if(!focused)
        XGrabButton(display, AnyButton, AnyModifier, window, False, BUTTONMASK, GrabModeAsync,
                    GrabModeSync, None, None);
}

internal void
deleteclient(arg_t args)
{
    if(manager.active_window_pos == -1)
        return;

    client_t* c = wintoclient(getactivewindow());
    if(!sendevent(c, wmatoms[WM_DELETE])) {
        XGrabServer(display);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(display, DestroyAll);
        XKillClient(display, getactivewindow());
        XSync(display, False);
        XSetErrorHandler(xerror);
        XUngrabServer(display);
    }

    changefocus((arg_t){.i = -1});
}

internal int
xerrordummy(Display* display, XErrorEvent* error)
{
    return (0);
}

internal void
cycleclient(arg_t args)
{
    monitor_t* monitor = getactivemonitor();
    int        count   = arrlenu(monitor->clients);

    if(count <= 1)
        return;

    int pos = manager.active_window_pos + args.i;
    if(pos < 0) {
        pos = count - 1;
    }
    if(pos > count - 1) {
        pos = 0;
    }

    swapclients(manager.active_window_pos, pos);
    changefocus(args);
    rearrange();
}

internal void
swapclients(uint srcpos, uint destpos)
{
    monitor_t* monitor        = getactivemonitor();
    client_t   tmp            = monitor->clients[destpos];
    monitor->clients[destpos] = monitor->clients[srcpos];
    monitor->clients[srcpos]  = tmp;
}

internal void
setupatoms(void)
{
    wmatoms[WM_PROTOCOLS]          = XInternAtom(display, "WM_PROTOCOLS", False);
    wmatoms[WM_STATE]              = XInternAtom(display, "WM_STATE", False);
    wmatoms[WM_TAKE_FOCUS]         = XInternAtom(display, "WM_TAKE_FOCUS", False);
    wmatoms[WM_DELETE]             = XInternAtom(display, "WM_DELETE_WINDOW", False);
    netatoms[NET_WM_FULLSCREEN]    = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    netatoms[NET_WM_NAME]          = XInternAtom(display, "_NET_WM_NAME", False);
    netatoms[NET_WM_STATE]         = XInternAtom(display, "_NET_WM_STATE", False);
    netatoms[NET_WM_CHECK]         = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    netatoms[NET_WM_SUPPORTED]     = XInternAtom(display, "_NET_SUPPORTED", False);
    netatoms[NET_WM_ACTIVE_WINDOW] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    netatoms[NET_WM_WINDOW_TYPE]   = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    netatoms[NET_WM_CLIENT_LIST]   = XInternAtom(display, "_NET_CLIENT_LIST", False);
}

internal void
clientmessage(XEvent* event)
{
    return;
}

internal int
sendevent(client_t* c, Atom atom)
{
    if(!c)
        return (0);

    Atom* protocols;
    int   n;
    bool  exist = false;
    if(XGetWMProtocols(display, c->window, &protocols, &n)) {
        while(!exist && n--)
            exist = protocols[n] == atom;
        XFree(protocols);
    }

    if(exist) {
        XEvent event;
        event.type                 = ClientMessage;
        event.xclient.message_type = wmatoms[WM_PROTOCOLS];
        event.xclient.window       = c->window;
        event.xclient.message_type = atom;
        event.xclient.format       = 32;
        event.xclient.data.l[0]    = atom;
        event.xclient.data.l[1]    = CurrentTime;
        XSendEvent(display, c->window, False, NoEventMask, &event);
    }

    return (exist);
}

internal void
setupnetcheck(void)
{
    // NOTE(fonsi): setup list of supported protocols
    XChangeProperty(display, root, netatoms[NET_WM_SUPPORTED], XA_ATOM, PropModeReplace, 32,
                    (unsigned char*) netatoms, NET_WM_LAST);

    // TODO(fonsi): set _NET_NUMBER_OF_DESKTOPS for virtual desktops
}

internal int
wintoclientpos(Window window)
{
    monitor_t* mon = wintomon(window);
    for(uint i = 0; i < arrlenu(mon->clients); i++) {
        if(mon->clients[i].window == window)
            return (i);
    }
    return (-1);
}

internal void
createnotify(XEvent* event)
{
    return;
}

internal monitor_t
createmon(XineramaScreenInfo info)
{
    monitor_t mon = {
        .clients = NULL,
        .x_orig  = info.x_org,
        .y_orig  = info.y_org,
        .height  = info.height,
        .width   = info.width,
    };
    return (mon);
}

internal void
setupgeometry(void)
{
#if defined(XINERAMA)
    uint                num;
    XineramaScreenInfo* info = XineramaQueryScreens(display, (int*) &num);
    for(uint i = 0; i < num; i++) {
        monitor_t mon = createmon(info[i]);
        arrput(manager.monitors, mon);
    }
    XFree(info);
#else
    XWindowAttributes wa;
    XGetWindowAttributes(display, root, &wa);

    manager.monitors[0].clients = NULL;
    manager.monitors[0].x_orig  = 0;
    manager.monitors[0].y_orig  = 0;
    manager.monitors[0].height  = wa.height;
    manager.monitors[0].width   = wa.width;
#endif
}

internal uint
wintomonpos(Window window)
{
    uint moncount = arrlenu(manager.monitors);
    for(uint i = 0; i < moncount; i++) {
        uint clientcount = arrlenu(manager.monitors[i].clients);
        if(clientcount > 0) {
            for(uint j = 0; j < clientcount; j++) {
                if(manager.monitors[i].clients[j].window == window)
                    return (i);
            }
        }
    }
    return (0);
}

internal monitor_t*
wintomon(Window window)
{
    uint moncount = arrlenu(manager.monitors);
    for(uint i = 0; i < moncount; i++) {
        uint clientcount = arrlenu(manager.monitors[i].clients);
        if(clientcount > 0) {
            for(uint j = 0; j < clientcount; j++) {
                if(manager.monitors[i].clients[j].window == window)
                    return (&manager.monitors[i]);
            }
        }
    }
    return (&manager.monitors[0]);
}

internal void
setupmanager(void)
{
    manager.monitors           = NULL;
    manager.active_monitor_pos = 0;
    manager.active_window_pos  = -1;
}

internal Window
getactivewindow(void)
{
    monitor_t* mon = getactivemonitor();
    return (mon->clients[manager.active_window_pos].window);
}

internal monitor_t*
clienttomon(client_t* client)
{
    for(uint i = 0; i < arrlenu(manager.monitors); i++) {
        for(uint j = 0; j < arrlenu(manager.monitors[i].clients); j++) {
            if(manager.monitors[i].clients[j].window == client->window)
                return (&manager.monitors[i]);
        }
    }
    return (NULL);
}

internal monitor_t*
getactivemonitor(void)
{
    return (&manager.monitors[manager.active_monitor_pos]);
}

internal void
printtest(void)
{
    fprintf(stderr, "test\n");
}

internal void
motionnotify(XEvent* event)
{
    XMotionEvent* ev           = &event->xmotion;
    manager.active_monitor_pos = coordstomonitorpos(ev->x_root, ev->y_root);
}

internal uint
coordstomonitorpos(uint x, uint y)
{
    for(uint i = 0; i < arrlenu(manager.monitors); i++) {
        monitor_t* mon = &manager.monitors[i];
        if(x > mon->x_orig && x < mon->x_orig + mon->width && y > mon->y_orig &&
           y < mon->y_orig + mon->height)
            return (i);
    }
    return (0);
}

internal void
changemonitorfocus(arg_t args)
{
    uint count = arrlenu(manager.monitors);

    if(count <= 1)
        return;

    int pos = manager.active_monitor_pos + args.i;
    if(pos < 0)
        pos = count - 1;
    if(pos > (int) count - 1)
        pos = 0;
    focusmonitor(pos);
}

internal void
focusmonitor(uint pos)
{
    manager.active_monitor_pos = pos;
    focus(manager.monitors[pos].clients[0].window);
}
