#include "utils.h"

global root_window root;
global int         screen;
global Display*    display;
global bool        wm_detected;
global Layout      layout;
global bool        quit;

internal void start(void);
internal void clean(void);
internal void loop(void);
internal void reparent_all_windows(void);
internal int  xerror(Display* display, XErrorEvent* error);
internal int  on_wm_detected(Display* display, XErrorEvent* error);
internal void frame(Window window, bool created_before_wm);
internal void unframe(Window window);

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
        start();

        if (wm_detected)
                return (EXIT_FAILURE);

        clean();
        loop();
        return (EXIT_SUCCESS);
}

internal void
start(void)
{
        display        = XOpenDisplay(0);
        screen         = XDefaultScreen(display);
        layout.clients = NULL;

        wm_detected = false;
        XSetErrorHandler(on_wm_detected);
        XSelectInput(display, root.window, SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(display, false);

        root.window = XDefaultRootWindow(display);
        root.x      = 0;
        root.y      = 0;
        root.width  = XDisplayWidth(display, screen);
        root.height = XDisplayHeight(display, screen);

        XSetErrorHandler(xerror);

        reparent_all_windows();
}

internal void
clean(void)
{
        XCloseDisplay(display);
        arrfree(layout.clients);
}

internal void
loop(void)
{
        XEvent event;

        while (!quit && !XNextEvent(display, &event)) {
                if (handler[event.type])
                        handler[event.type](&event);
        }
}

internal void
reparent_all_windows(void)
{
        XGrabServer(display);
        Window  return_root, return_parent;
        Window* windows;
        uint    window_count;
        XQueryTree(display, root.window, &return_root, &return_parent, &windows, &window_count);

        for (uint i = 0; i < window_count; i++) {
                XWindowAttributes window_attrs;

                XGetWindowAttributes(display, windows[i], &window_attrs);

                if (window_attrs.override_redirect || window_attrs.map_state != IsViewable)
                        return;
                else {

                        XAddToSaveSet(display, windows[i]);

                        // TODO(fonsi): reframe function to change size and position of windows when
                        // creating/reparenting
                        XReparentWindow(display, root.window, windows[i], 0, 0);
                        XMapWindow(display, windows[i]);
                        arrput(layout.clients, windows[i]);
                }
        }

        XFree(windows);
        XUngrabServer(display);
}

internal int
xerror(Display* display, XErrorEvent* error)
{
        char msg[512];
        XGetErrorText(display, error->error_code, msg, 512);

        fprintf(stderr, "[yawm] ERROR error code: %d\nMessage: %s\n===\n", error->error_code, msg);

        return (error->error_code);
}

internal int
on_wm_detected(Display* display, XErrorEvent* error)
{
        if (error->error_code == BadAccess) {
                wm_detected = true;
        }

        return (0);
}

internal void
configurerequest(XEvent* event)
{
        XConfigureRequestEvent* ev = (XConfigureRequestEvent*) event;

        XWindowChanges changes;

        changes.x            = ev->x;
        changes.y            = ev->y;
        changes.width        = ev->width;
        changes.height       = ev->height;
        changes.border_width = ev->border_width;
        changes.sibling      = ev->above;
        changes.stack_mode   = ev->detail;

        client_window* val = hmgetp_null(clients, ev->window);
        if (val) {
                XConfigureWindow(display, val->value, ev->value_mask, &changes);
        }
}

internal void
maprequest(XEvent* event)
{
        XMapRequestEvent* ev = (XMapRequestEvent*) event;

        frame(ev->window, false);
        XMapWindow(display, ev->window);
}

internal void
frame(Window window, bool created_before_wm)
{
        XWindowAttributes window_attrs;
        XGetWindowAttributes(display, window, &window_attrs);

        if (created_before_wm) {
                if (window_attrs.override_redirect || window_attrs.map_state != IsViewable)
                        return;
        }

        const Window window_frame = XCreateSimpleWindow(display,
                                                        root.window,
                                                        window_attrs.x,
                                                        window_attrs.y,
                                                        window_attrs.width,
                                                        window_attrs.height,
                                                        0,
                                                        0,
                                                        0);

        XSelectInput(display, window_frame, SubstructureRedirectMask | SubstructureNotifyMask);
        XAddToSaveSet(display, window);
        XReparentWindow(display, window, window_frame, 0, 0);
        XMapWindow(display, window_frame);

        hmput(clients, window, window_frame);
}

internal void
unmapnotify(XEvent* event)
{
        XUnmapEvent*   ev     = (XUnmapEvent*) event;
        client_window* client = hmgetp_null(clients, ev->window);

        if (!client || ev->event == root.window)
                return;

        unframe(ev->window);
}

internal void
unframe(Window window)
{
        client_window* client = hmgetp_null(clients, window);
        XUnmapWindow(display, client->value);
        XReparentWindow(display, window, root.window, 0, 0);
        XRemoveFromSaveSet(display, window);
        XDestroyWindow(display, client->value);
        hmdel(clients, client->key);
}
