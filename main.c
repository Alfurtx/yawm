#include "utils.h"

/* global varibles */
global int       screen;
global Display*  display;
global bool      quit;
global Window    root;
global monitor_t monitor;
global Window    checkwin;
global wm_t      manager;

/* helper functions */
internal void      start(void);
internal void      clean(void);
internal void      loop(void);
internal void      die(char* msg, int errorcode);
internal void      checkotherwm(void);
internal void      rearrange(void);
internal void      grabkeys(void);
internal void      focus(Window window);
internal void      grabbuttons(Window window);
internal void      setupatoms(void);
internal void      setupnetcheck(void);
internal int       sendevent(client_t* c, Atom atom);
internal monitor_t createmon(XineramaScreenInfo info);

/* multimonitor geometry helper functions */
internal void setupgeometry(void);
internal int  wintomonpos(Window window);

/* window & client helpers functions */
internal client_t* wintoclient(Window window);
internal int       wintoclientpos(Window window);
internal void      resizeclient(client_t* client, int x, int y, int w, int h, uint bw);
internal void      manageclient(Window window);
internal void      unmanageclient(Window window);
internal void      swapclients(uint srcpos, uint destpos);

/* command functions */
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
    [ButtonPress] = buttonpress,
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
     ButtonPressMask | ButtonReleaseMask)
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
        arrfree(manager.monitors[i].clients);
    }
    arrfree(manager.monitors);
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
    if(!wintoclient(ev->window))
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
    for(uint i = 0; i < arrlenu(monitor.clients); i++) {
        if(monitor.clients[i].window == window)
            return (&monitor.clients[i]);
    }
    return (NULL);
}

internal void
rearrange(void)
{
    XWindowAttributes wa;
    XGetWindowAttributes(display, root, &wa);

    uint count = arrlenu(monitor.clients);

    if(count == 0) {
        return;
    } else if(count == 1) {
        resizeclient(&monitor.clients[0], 0, 0, wa.width - 2 * border_width,
                     wa.height - 2 * border_width, border_width);
    } else {

        client_t* c  = monitor.clients;
        uint      bw = c->bw;
        uint      ws = wa.width / 2;
        uint      hs = wa.height / (count - 1);
        uint      x  = 0;
        uint      y  = 0;
        resizeclient(c, x, y, ws - 2 * bw, wa.height - 2 * bw, bw);

        // NOTE(fonsi): an excess value is computed to take into account
        // the decimal loss in (height / number of screens) and shared
        // after in between all windows
        uint excess = hs - 2 * bw;
        for(uint i = 1; i < count - 1; i++) {
            excess += (hs - bw) + bw;
        }
        excess += 2 * bw;
        excess = wa.height - excess;

        for(uint i = 0; i < count - 1; i++) {
            c  = &monitor.clients[i + 1];
            bw = c->bw;
            x  = ws - bw;
            y  = hs * i;

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
    arrput(monitor.clients, c);
    rearrange();
    XMapWindow(display, c.window);
    grabbuttons(window);
    focus(window);
}

internal void
unmanageclient(Window window)
{
    int pos = wintoclientpos(window);
    if(pos != -1) {
        XGrabServer(display);
        XSetErrorHandler(xerrordummy);
        arrdel(monitor.clients, pos);
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

    for(uint i = 0; i < arrlenu(monitor.clients); i++) {
        if(monitor.clients[i].window != window) {
            XSetWindowBorder(display, monitor.clients[i].window, border_color_inactive);
        } else {
            monitor.focus_pos = i;
        }
    }

    XChangeProperty(display, window, netatoms[NET_WM_ACTIVE_WINDOW], XA_WINDOW, PropModeReplace, 32,
                    (unsigned char*) &window, 1);
}

internal void
changefocus(arg_t args)
{
    if(arrlenu(monitor.clients) == 0)
        return;

    int aux = monitor.focus_pos + args.i;
    if(aux < 0)
        aux = arrlenu(monitor.clients) - 1;
    if(aux > (int) arrlenu(monitor.clients) - 1)
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
    XGrabButton(display, AnyButton, AnyModifier, window, False, BUTTONMASK, GrabModeAsync,
                GrabModeSync, None, None);
}

internal void
deleteclient(arg_t args)
{
    if(monitor.focus_pos == -1)
        return;
    if(!sendevent(&monitor.clients[monitor.focus_pos], wmatoms[WM_DELETE])) {
        fprintf(stderr, "no atom");
        XGrabServer(display);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(display, DestroyAll);
        XKillClient(display, monitor.clients[monitor.focus_pos].window);
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
    int count = arrlenu(monitor.clients);

    if(count <= 1)
        return;

    int pos = monitor.focus_pos + args.i;
    if(pos < 0) {
        pos = count - 1;
    }
    if(pos > count - 1) {
        pos = 0;
    }

    swapclients(monitor.focus_pos, pos);
    changefocus(args);
    rearrange();
}

internal void
swapclients(uint srcpos, uint destpos)
{
    client_t tmp             = monitor.clients[destpos];
    monitor.clients[destpos] = monitor.clients[srcpos];
    monitor.clients[srcpos]  = tmp;
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
    for(uint i = 0; i < arrlenu(monitor.clients); i++)
        if(monitor.clients[i].window == window)
            return (i);
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
        .clients   = NULL,
        .focus_pos = -1,
        .x_orig    = info.x_org,
        .y_orig    = info.y_org,
        .height    = info.height,
        .width     = info.width,
    };
    return (mon);
}

internal void
setupgeometry(void)
{
    manager.monitors = NULL;

#if defined(XINERAMA)
    int                 num;
    XineramaScreenInfo* info = XineramaQueryScreens(display, &num);
    for(uint i = 0; i < num; i++) {
        arrput(manager.monitors, createmon(info[i]));
    }
    XFree(info);
#else
    XWindowAttributes wa;
    XGetWindowAttributes(display, root, &wa);

    monitor.clients   = NULL;
    monitor.focus_pos = -1;
    monitor.x_orig    = 0;
    monitor.y_orig    = 0;
    monitor.height    = wa.height;
    monitor.width     = wa.width;
#endif
}

internal int
wintomonpos(Window window)
{
    uint moncount = arrlenu(manager.monitors);

    for(uint i = 0; i < moncount; i++) {

        uint clientcount = arrlenu(manager.monitors[i].clients);

        for(uint j = 0; j < clientcount; j++) {

            if(manager.monitors[i].clients->window == window)
                return (i);
        }
    }
    return (-1);
}
