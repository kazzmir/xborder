/* xborder: shows a border around an arbitrary X window.
 * Jon Rafkind jon@rafkind.com
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <execinfo.h>
#include <stdint.h>

#define xdebug(str, value) printf(str, value)

/* copied from libXmu/src/ClientWin.c */
static Window TryChildren(Display *dpy, Window win, Atom WM_STATE){
    Window root, parent;
    Window *children;
    unsigned int nchildren;
    unsigned int i;
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data;
    Window inf = 0;

    if (!XQueryTree(dpy, win, &root, &parent, &children, &nchildren))
        return 0;
    for (i = 0; !inf && (i < nchildren); i++) {
        data = NULL;
        XGetWindowProperty(dpy, children[i], WM_STATE, 0, 0, False,
                AnyPropertyType, &type, &format, &nitems,
                &after, &data);
        if (data)
            XFree(data);
        if (type)
            inf = children[i];
    }
    for (i = 0; !inf && (i < nchildren); i++)
        inf = TryChildren(dpy, children[i], WM_STATE);
    if (children)
        XFree(children);
    return inf;
}

static Window find_window_by_atom(Display *dpy, Window win, Atom atom){
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    Window inf;

    // std::cout << "Find window with atom " << XGetAtomName(dpy, atom) << std::endl;
    XGetWindowProperty(dpy, win, atom, 0, 0, False, AnyPropertyType,
            &type, &format, &nitems, &after, &data);
    if (data)
        XFree(data);
    if (type)
        return win;
    inf = TryChildren(dpy, win, atom);
    if (!inf)
        inf = win;
    return inf;
}

static Window _XmuClientWindow(Display *dpy, Window win){
    Atom WM_STATE;

    WM_STATE = XInternAtom(dpy, "WM_STATE", True);
    if (!WM_STATE)
        return win;
    return find_window_by_atom(dpy, win, WM_STATE);
}

Window choose_window(Display * display){
    Window root = RootWindow(display, DefaultScreen(display));
    Window root_out;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask;
    Window child = 0;
    Cursor cursor;

    cursor = XCreateFontCursor(display, XC_crosshair);

    XGrabPointer(display, root, False, ButtonPressMask | ButtonReleaseMask, GrabModeSync, GrabModeAsync, root, cursor, CurrentTime);

    // std::cout << "Grabbing window" << std::endl;
    while (1){
        XAllowEvents(display, SyncPointer, CurrentTime);
        XEvent event;
        XWindowEvent(display, root, ButtonPressMask | ButtonReleaseMask, &event);
        if (event.type == ButtonPress){
            child = event.xbutton.subwindow;
            break;
        }
    }

    // XQueryPointer(display, root, &root_out, &child, &root_x, &root_y, &win_x, &win_y, &mask);
    // std::cout << "Grabbed window " << child << std::endl;
    XUngrabPointer(display, CurrentTime);
    std::cout << "Grabbed window " << child << std::endl;
    if (child == root){
        return 0;
    }

    return child;
}

Window find_xborder_window(Display * display){
    Window chosen = choose_window(display);
    if (chosen == 0){
        std::cout << "Error: Window could not be selected, or root window chosen" << std::endl;
        return 0;
    }
    Atom xborder = XInternAtom(display, "xborder", false);
    Window xborder_window = find_window_by_atom(display, chosen, xborder);
    if (xborder_window == 0){
        std::cout << "Error: Selected window is not an xborder: " << chosen << std::endl;
        return 0;
    }
    return xborder_window;
}

Window find_terminal(Display * display){
    Window child = choose_window(display);
    if (child == 0){
        return 0;
    }
    return _XmuClientWindow(display, child);
}

void get_window_dimensions(Display * display, Window window, int * width, int * height){
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);
    *width = attributes.width;
    *height = attributes.height;
}

enum XBorderMessage{
    XBorderConfigure
};

XColor create_color(Display * display, int red, int green, int blue){
    XColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    int screen = DefaultScreen(display);
    Colormap screen_colormap = DefaultColormap(display, screen);
    XAllocColor(display, screen_colormap, &color);
    return color;
}

typedef struct {
    double r;       // ∈ [0, 1]
    double g;       // ∈ [0, 1]
    double b;       // ∈ [0, 1]
} rgb;

XColor create_color(Display * display, rgb rgb){
    return create_color(display, rgb.r * 65535, rgb.g * 65535, rgb.b * 65535);
}

typedef struct {
    double h;       // ∈ [0, 360]
    double s;       // ∈ [0, 1]
    double v;       // ∈ [0, 1]
} hsv;

/* https://stackoverflow.com/a/36209005/234139 */
rgb hsv2rgb(hsv HSV){
    rgb RGB;
    double H = HSV.h, S = HSV.s, V = HSV.v,
           P, Q, T,
           fract;

    (H == 360.)?(H = 0.):(H /= 60.);
    fract = H - floor(H);

    P = V*(1. - S);
    Q = V*(1. - S*fract);
    T = V*(1. - S*(1. - fract));

    if      (0. <= H && H < 1.)
        RGB = (rgb){.r = V, .g = T, .b = P};
    else if (1. <= H && H < 2.)
        RGB = (rgb){.r = Q, .g = V, .b = P};
    else if (2. <= H && H < 3.)
        RGB = (rgb){.r = P, .g = V, .b = T};
    else if (3. <= H && H < 4.)
        RGB = (rgb){.r = P, .g = Q, .b = V};
    else if (4. <= H && H < 5.)
        RGB = (rgb){.r = T, .g = P, .b = V};
    else if (5. <= H && H < 6.)
        RGB = (rgb){.r = V, .g = P, .b = Q};
    else
        RGB = (rgb){.r = 0., .g = 0., .b = 0.};

    return RGB;
}

float interpolate(float value, float in_range_min, float in_range_max, float out_range_min, float out_range_max){
    float b = (out_range_max - out_range_min);
    float a = (in_range_max - in_range_min);
    return (value - in_range_min) * b / a + out_range_min;
}

const int palette_size_block = 15;
const int palette_x = 20;
const int palette_y = 6;

rgb get_rgb(int h, float s, float v){
    hsv hsv;
    hsv.h = h;
    hsv.s = s;
    hsv.v = v;
    return hsv2rgb(hsv);
}

rgb get_rgb(int x, int y){
    if (x < 0 || x > palette_x || y < 0 || y > palette_y){
        rgb out;
        out.r = 0;
        out.g = 0;
        out.b = 0;
        return out;
    }
    hsv hsv;
    hsv.h = (int) interpolate(x, 0, palette_x, 0, 360);
    hsv.s = 1.0;
    hsv.v = 1.2 - interpolate(y, 0, palette_y, 0.2, 1.0);
    return hsv2rgb(hsv);
}

/* time in milliseconds */
uint64_t time_now(){
    struct timeval time;
    if (gettimeofday(&time, NULL) == 0){
        return time.tv_sec * 1000 + time.tv_usec / 1000;
    }

    return 0;
}

void draw_palette(Display * display, Window window, int start_y){
    
    GC local = XCreateGC(display, window, 0, NULL);
    
    int x = 0;
    int y = start_y;
    int size = palette_size_block;

    for (int cy = 0; cy <= palette_y; cy++){
        for (int cx = 0; cx <= palette_x; cx++){
            rgb rgb = get_rgb(cx, cy);
            XColor color = create_color(display, rgb);
            XGCValues values;
            values.foreground = color.pixel;
            XChangeGC(display, local, GCForeground, &values);
            XFillRectangle(display, window, local, x, y, size, size);

            x += size;
        }
        x = 0;
        y += size;
    } 

    XFreeGC(display, local);
}

void change_background_color(Display * display, Window window, rgb rgb){
    XSetWindowAttributes attributes;
    attributes.background_pixel = create_color(display, rgb).pixel;
    XChangeWindowAttributes(display, window, CWBackPixel, &attributes);
    XClearWindow(display, window);
}

volatile bool quit_now = 0;
void handle_signal(int signal){
    quit_now = 1;
}

/* choose a random starting color */
XColor start_color(Display * display){
    rgb rgb = get_rgb(rand() % (palette_x + 1), 0);
    return create_color(display, rgb);
}

/* has to be a global so that the x_error can reach it */
static Window window;

static void show_backtrace(int fd){
    void *array[100];
    size_t size = backtrace(array, 100);
    backtrace_symbols_fd(array, size, fd);
}

int x_error(Display * display, XErrorEvent * event){
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "/tmp/xborder-error.%lu", window);
    FILE * log = fopen(buffer, "a");
    fprintf(log, "xborder pid %d window id %lu crashed\n", getpid(), window);
    fprintf(log, "xerror:\n");
    fprintf(log, "  serial: %lu\n", event->serial);
    char error_string[256];
    XGetErrorText(display, event->error_code, error_string, sizeof(error_string));
    fprintf(log, "  error code: %s\n", error_string);
    fprintf(log, "  request code: %d\n", event->request_code);
    fprintf(log, "  minor code: %d\n", event->minor_code);
    fprintf(log, "  resource id: %lu\n", event->resourceid);
    fprintf(log, "\n");
    show_backtrace(fileno(log));
    fclose(log);
    // exit(1);
    return 0;
}

std::string get_window_title(Display * display, Window window){
    char* buffer = NULL;
    if (XFetchName(display, window, &buffer) != 0){
        if (buffer != NULL){
            std::string out = buffer;
            XFree(buffer);
            return out;
        }
    }

    return "XBorder";
}

void delete_properties(Display * display, Window window){
    int count = 0;
    Atom * atoms = XListProperties(display, window, &count);
    for (int i = 0; i < count; i++){
        const Atom & atom = atoms[i];
        char * name = XGetAtomName(display, atom);
        std::string sname = name;


        bool destroy = true;
        if (sname == "XKLAVIER_STATE" ||
            sname == "_NET_WM_STATE" ||
            sname == "_NET_FRAME_EXTENTS" ||
            sname == "_NET_WM_DESKTOP" ||
            sname == "WM_NAME"){
            destroy = false;
        }

        if (destroy){
            std::cout << "Delete property '" << name << "'" << std::endl;
        }
        XFree(name);

        if (destroy){
            XDeleteProperty(display, window, atom);
        }
    }
    XFree(atoms);
}

Window get_root_window(Display * display, Window window){
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);
    return attributes.root;
}

void set_override_redirect(Display * display, Window window, bool value){
    XSetWindowAttributes attributes;
    attributes.override_redirect = value;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attributes);
}

Window get_parent_window(Display * display, Window window){
    Window root = 0;
    Window parent = 0;
    Window* children = NULL;
    unsigned int count = 0;
    XQueryTree(display, window, &root, &parent, &children, &count);
    if (children != NULL){
        XFree(children);
    }
    return parent;
}

void set_xborder_window(Display * display, Window window){
    Atom xborder = XInternAtom(display, "xborder", false);
    unsigned char * c = (unsigned char*) "a";
    XChangeProperty(display, window, xborder, XA_STRING, 8, PropModeReplace, (unsigned char*) c, 1);
}

static const int glow_max_speed = 100;

class OptionWindow {
public:
    const Window window;
    const Window xborder_window;
    GC const graphics;
    Display * const display;
    bool destroy;
    std::string window_title;
    static const int palette_start_y = 40;
    bool left_button_pressed;
    static const int glow_line_step = 2;

    int glow_line_y;
    int glow_line_height;
    int glow_line_x;
    int glow_line_width;
    int glow_value;

    OptionWindow(Display * display, Window window, GC graphics, Window xborder_window):
    window(window),
    xborder_window(xborder_window),
    graphics(graphics),
    display(display),
    destroy(false),
    glow_line_y(0),
    glow_line_height(0),
    glow_line_x(0),
    glow_line_width(0),
    glow_value(0),
    left_button_pressed(false){
        window_title = get_window_title(display, xborder_window);
    }

    void raise(){
        XRaiseWindow(display, window);
    }

    bool isDead(){
        return this->destroy;
    }

    void handleEvent(XEvent * event, int * glow){
        if (event->xany.window == this->window){
            bool redraw = false;
            switch (event->type){
                case ClientMessage: {
                    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
                    Atom wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", true);

                    if (event->xclient.message_type == wm_protocols_atom &&
                        (Atom) event->xclient.data.l[0] == wm_delete_window){

                        destroy = true;
                        /*
                        XFreeGC(display, graphics);
                        graphics = NULL;
                        XUnmapWindow(display, option_window);
                        XDestroyWindow(display, option_window);
                        option_window = 0;
                        */
                    }
                    break;
                }
                case Expose: {
                    redraw = true;
                    break;
                }
                case MotionNotify: {
                    if (left_button_pressed){
                        int x = event->xmotion.x;
                        int use = (x - glow_line_x) / glow_line_step;
                        if (use < 0){
                            use = 0;
                        }
                        if (use > glow_max_speed){
                            use = glow_max_speed;
                        }
                        *glow = use;
                        glow_value = *glow;
                        redraw = true;
                    }
                    break;
                }
                case ButtonPress: {
                    int x = event->xbutton.x;
                    int y = event->xbutton.y;
                    if (y >= palette_start_y && y < palette_start_y + (palette_y + 1) * palette_size_block &&
                        x >= 0 && x < (palette_x + 1) * palette_size_block){

                        change_background_color(display, xborder_window, get_rgb(x / palette_size_block, (y - palette_start_y) / palette_size_block));
                    } else if (y >= glow_line_y - glow_line_height / 2 &&
                               y <= glow_line_y + glow_line_height / 2 &&
                               x >= glow_line_x &&
                               x <= glow_line_x + glow_line_width){
                        *glow = (x - glow_line_x) / glow_line_step;
                        if (*glow >= glow_max_speed){
                            *glow = glow_max_speed;
                        }
                        glow_value = *glow;
                        left_button_pressed = true;
                        redraw = true;
                    }
                    break;
                }
                case ButtonRelease: {
                    /* FIXME: check for left button */
                    left_button_pressed = false;
                    break;
                }
                case KeyPress: {
                    KeySym sym = XLookupKeysym(&event->xkey, 0);
                    if (sym == XK_BackSpace){
                        redraw = true;
                        if (window_title.length() > 0){
                            window_title = window_title.substr(0, window_title.size() - 1);
                        }
                    } else {
                        char * ascii = XKeysymToString(sym);
                        /* FIXME: handle shift, ctrl-w, ctrl-u */
                        if (ascii != NULL){
                            std::string add;
                            if (std::string(ascii) == "space"){
                                add = " ";
                            } else if (strlen(ascii) == 1){
                                add = ascii;
                            }

                            if (add != ""){
                                window_title += add;
                                redraw = true;
                            }
                        }
                    }
                    XStoreName(display, xborder_window, window_title.c_str());
                    break;
                }
            }

            if (redraw){
                draw(glow);
            }
        }
    }

    void draw(int * glow){
        std::string total;
        total += "Title: ";
        total += window_title;
        XGCValues values;
        int screen = DefaultScreen(display);
        values.foreground = WhitePixel(display, screen);
        GC local = XCreateGC(display, window, GCForeground, &values);
        int width, height;
        get_window_dimensions(display, window, &width, &height);
        // XFillRectangle(display, option_window, local, 0, 0, width, height);
        XClearWindow(display, window);
        XFreeGC(display, local);

        int y = 10;
        int font_size = 10;
        int glyph_width = 1;

        GContext context = XGContextFromGC(graphics);
        XFontStruct * font = XQueryFont(display, context);
        if (font != NULL){
            glyph_width = font->max_bounds.width;
            font_size = font->max_bounds.ascent - font->max_bounds.descent;
            // std::cout << "Got font info" << std::endl;
            XFreeFontInfo(0, font, 1);
        }

        XDrawString(display, window, graphics, 1, y, total.c_str(), total.size());
        y += font_size * 3 / 2;
        char glow_info[32];
        snprintf(glow_info, sizeof(glow_info), "Glow: %d", *glow);
        XDrawString(display, window, graphics, 1, y, glow_info, strlen(glow_info));

        int line_x = (glyph_width + 1) * strlen("glow: 123");
        int line_y = y - font_size / 2;
        int glow_height = 12;

        if (glow_line_y == 0){
            glow_line_y = line_y;
            glow_line_x = line_x;
            glow_line_width = glow_max_speed * glow_line_step;
            glow_line_height = glow_height;
        }

        int glow_x = *glow * glow_line_step;

        // XDrawLine(display, window, graphics, line_x, line_y, glow_line_width, line_y);
        XDrawLine(display, window, graphics, line_x, line_y, line_x + glow_line_width, line_y);

        XFillRectangle(display, window, graphics, line_x + glow_x, line_y - glow_height / 2, 5, glow_height);

        draw_palette(display, window, palette_start_y);
    }

    static OptionWindow* create(Display * display, Window xborder_window){
        Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
        int screen = DefaultScreen(display);
        int width = (palette_x + 1) * palette_size_block + 5;
        int height = palette_start_y + (palette_y + 1) * palette_size_block;
        Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                            1, 1,
                                            width, height,
                                            1, BlackPixel(display, screen), WhitePixel(display, screen));
        XFontStruct * fontInfo = XLoadQueryFont(display, "6x10");
        XGCValues values;
        // values.font = fontInfo->fid;
        values.foreground = BlackPixel(display, screen);
        GC graphics = XCreateGC(display, window, GCForeground, &values);
        XSelectInput(display, window,
                     ExposureMask |
                     KeyPress |
                     KeyRelease |
                     ButtonPressMask |
                     ButtonReleaseMask |
                     Button1MotionMask |
                     StructureNotifyMask);
        XMapWindow(display, window);
        XStoreName(display, window, "XBorder options");
        XSetWMProtocols(display, window, &wm_delete_window, 1);

        return new OptionWindow(display, window, graphics, xborder_window);
    }

    virtual ~OptionWindow(){
        XFreeGC(display, graphics);
        XUnmapWindow(display, window);
        XDestroyWindow(display, window);
    }
};

void run_xborder(bool glow){
    Display * display;

    display = XOpenDisplay(NULL);
    if (display == NULL){
        std::cerr << "Cannot open display" << std::endl;
        return;
    }

    srand(time(NULL));

    XSetErrorHandler(x_error);

    /*
    std::cout << "Once a window has been selected you can press the following keys in the selected window" << std::endl;
    std::cout << "Press 'right alt + right shift' to bring up the options window" << std::endl;
    std::cout << "Press 'left shift + right shift' to close the border" << std::endl;
    */
    std::cout << "Select an existing window to wrap in xborder" << std::endl;
    std::cout << "kill the xborder program with ctrl-c, or press the X window button will also stop the xborder program" << std::endl;

    struct sigaction action;
    action.sa_handler = &handle_signal;
    action.sa_flags = SA_RESTART;
    sigfillset(&action.sa_mask);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int screen = DefaultScreen(display);

    Window child_window = find_terminal(display);
    if (child_window == RootWindow(display, screen) || child_window == 0){
        std::cout << "Invalid window chosen" << std::endl;
        return;
    }
    std::cout << "Child window " << child_window << std::endl;

    int child_x, child_y;
    XWindowAttributes child_attributes;
    XGetWindowAttributes(display, child_window, &child_attributes);
    Window child;
    XTranslateCoordinates(display, child_window, child_attributes.root, 0, 0, &child_x, &child_y, &child);
    // std::cout << "Window at " << child_x << ", " << child_y << std::endl;

    int border_size = 3;

    int use_width = child_attributes.width + border_size * 2;
    int use_height = child_attributes.height + border_size * 2;
    window = XCreateSimpleWindow(display, RootWindow(display, screen), child_x, child_y, use_width, use_height, 1, BlackPixel(display, screen), start_color(display).pixel);

    set_xborder_window(display, window);

    XSelectInput(display, window,
                 ExposureMask |
                 FocusChangeMask |
                 SubstructureNotifyMask |
                 StructureNotifyMask);
    std::cout << "Xborder window " << window << std::endl;
    XMapWindow(display, window);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    // delete_properties(display, child_window);
    set_override_redirect(display, child_window, true);

    XReparentWindow(display, child_window, window, border_size, border_size);
    /*
    std::cout << "Child root window " << get_root_window(display, child_window) << std::endl;
    std::cout << "Child parent window " << get_parent_window(display, child_window) << std::endl;
    // xdebug("XReparentWindow: %d\n", XReparentWindow(display, child_window, window, border_size, border_size));
    std::cout << "Child root window after reparent " << get_root_window(display, child_window) << std::endl;
    std::cout << "Child parent window after reparent " << get_parent_window(display, child_window) << std::endl;
    */
    // XSelectInput(display, child_window, KeyPressMask | KeyReleaseMask);
    XMoveWindow(display, window, child_x, child_y);

    Atom wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", true);

    bool shift_pressed = false;
    bool alt_pressed = false;

    OptionWindow* optionWindow = NULL;
    // Window option_window = 0;
    // int palette_start = 20;
    std::string window_title = get_window_title(display, child_window);

    GC graphics = NULL;
    XStoreName(display, window, window_title.c_str());

    uint64_t glow_start = time_now();
    int glow_color = rand() % 360;
    int glow_speed = 0;
    if (glow){
        glow_speed = 50;
    }
    
    Atom xborder_atom = XInternAtom(display, "xborder", false);

    while (1){
        if (quit_now){
            std::cout << "Bye!" << std::endl;
            break;
        }

        /* TODO: move all the glow code into an object */
        if (glow_speed > 0){
            uint64_t check = time_now();
            int use_speed = glow_max_speed - glow_speed;
            if (use_speed < 1){
                use_speed = 1;
            }
            int move = (check - glow_start) / use_speed / 2;
            if (move > 0){
                glow_color = (glow_color + move) % 360;
                rgb next_color = get_rgb(glow_color, 1.0, 1.0);
                change_background_color(display, window, next_color);
                glow_start += move * use_speed;
            }
        }

        if (get_parent_window(display, child_window) != window){
            XReparentWindow(display, child_window, window, border_size, border_size);
        }

        if (XPending(display) == 0){
            usleep(1000);
            continue;
        }

        /*
        Window focus;
        int state;
        XGetInputFocus(display, &focus, &state);
        std::cout << "Focus state " << focus << std::endl;
        */

        XEvent event;
        XNextEvent(display, &event);

        if (optionWindow != NULL){
            optionWindow->handleEvent(&event, &glow_speed);
            if (optionWindow->isDead()){
                delete optionWindow;
                optionWindow = NULL;
            }
        }

        if (event.xany.window == window){
            if (event.type == Expose){
                /* anything to do here? the child already seems to get expose events */
            } else if (event.type == FocusIn){
                if (child_window != 0){
                    /* FIXME: somehow the child_window can be destroyed before we get here. check that the child is a valid window? */
                    XSetInputFocus(display, child_window, RevertToPointerRoot, CurrentTime);
                }
            } else if (event.type == ClientMessage){
                XClientMessageEvent * client = &event.xclient;

                // std::cout << "Got client message: " << XGetAtomName(display, client->message_type) << std::endl;

                if (client->message_type == xborder_atom &&
                    client->data.b[0] == XBorderConfigure){
                    std::cout << "Reconfigure xborder" << std::endl;

                    if (optionWindow != NULL){
                        optionWindow->raise();
                    } else {
                        optionWindow = OptionWindow::create(display, window);
                    }
                } else if (client->message_type == wm_protocols_atom &&
                           (Atom) client->data.l[0] == wm_delete_window){
                    break;
                }
            } else if (event.type == DestroyNotify){
                // std::cout << "Child window " << child_window << " event.event " << event.xdestroywindow.event << " event.window " << event.xdestroywindow.window << std::endl;
                /* there is no more child to deal with */
                if (event.xdestroywindow.window == child_window){
                    child_window = 0;
                    break;
                }
            } else if (event.type == ConfigureNotify){
                XWindowAttributes self;
                XGetWindowAttributes(display, window, &self);
                XResizeWindow(display, child_window, self.width - border_size * 2, self.height - border_size * 2);
            }
        }
    }

    if (optionWindow != NULL){
        delete optionWindow;
        optionWindow = NULL;
    }

    if (child_window != 0){
        set_override_redirect(display, child_window, false);
        Window child_root = child_attributes.root;
        /* Place the window wherever the container is now */
        XGetWindowAttributes(display, window, &child_attributes);
        XReparentWindow(display, child_window, child_root, child_attributes.x, child_attributes.y);
        XTranslateCoordinates(display, window, child_attributes.root, 0, 0, &child_x, &child_y, &child);
        XMoveWindow(display, child_window, child_x, child_y - border_size * 2);
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

void do_configure_xborder(){
    Display * display;

    display = XOpenDisplay(NULL);
    if (display == NULL){
        std::cerr << "Cannot open display" << std::endl;
        return;
    }

    Window chosen = find_xborder_window(display);
    if (chosen == 0){
        XCloseDisplay(display);
        return;
    }

    // std::cout << "Send event to window " << chosen << std::endl;

    Atom xborder_configure = XInternAtom(display, "xborder", false);

    XEvent event;
    XClientMessageEvent * client = &event.xclient;
    event.type = ClientMessage;
    client->message_type = xborder_configure;
    client->format = 8;
    client->window = chosen;
    client->data.b[0] = XBorderConfigure;

    if (XSendEvent(display, chosen, 0, 0, &event) != 0){
        std::cout << "Reconfigured xborder window " << chosen << std::endl;
    } else {
        std::cout << "Could not send an event" << std::endl;
    }
    XCloseDisplay(display);
}

int main(int argc, char ** argv){
    bool glow = false;

    for (int i = 1; i < argc; i++){
        std::string arg(argv[i]);
        if (arg == "glow" ||
            arg == "-glow" ||
            arg == "--glow"){
            glow = true;
        } else if (arg == "configure"){
            do_configure_xborder();
            return 0;
        }
    }

    run_xborder(glow);
}
