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

/* copied from libXmu/src/ClientWin.c */
static Window TryChildren(Display *dpy, Window win, Atom WM_STATE);
static Window _XmuClientWindow(Display *dpy, Window win){
    Atom WM_STATE;
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    Window inf;

    WM_STATE = XInternAtom(dpy, "WM_STATE", True);
    if (!WM_STATE)
	return win;
    XGetWindowProperty(dpy, win, WM_STATE, 0, 0, False, AnyPropertyType,
		       &type, &format, &nitems, &after, &data);
    if (data)
	XFree(data);
    if (type)
	return win;
    inf = TryChildren(dpy, win, WM_STATE);
    if (!inf)
	inf = win;
    return inf;
}

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

Window find_terminal(Display * display){
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
    return _XmuClientWindow(display, child);
}

void get_window_dimensions(Display * display, Window window, int * width, int * height){
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);
    *width = attributes.width;
    *height = attributes.height;
}

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

const int palette_size_block = 20;
const int palette_x = 16;
const int palette_y = 5;

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

void draw_palette(Display * display, Window window, int start_y){
    
    GC local = XCreateGC(display, window, 0, NULL);
    
    int x = 0;
    int y = start_y;
    int size = palette_size_block;

    for (int cy = 0; cy <= palette_y; cy++){
        for (int cx = 0; cx <= palette_x; cx++){
            rgb rgb = get_rgb(cx, cy);
            XColor color = create_color(display, rgb.r * 65535, rgb.g * 65535, rgb.b * 65535);
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
    attributes.background_pixel = create_color(display, rgb.r * 65535, rgb.g * 65535, rgb.b * 65535).pixel;
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
    return create_color(display, rgb.r * 65535, rgb.g * 65535, rgb.b * 65535);
}

int main(){
    Display * display;
    Window window;

    srand(time(NULL));

    display = XOpenDisplay(NULL);
    if (display == NULL){
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }

    std::cout << "Once a window has been selected you can press the following keys in the selected window" << std::endl;
    std::cout << "Press 'left alt + left shift' to bring up the options window" << std::endl;
    std::cout << "Press 'left shift + right shift' to close the border" << std::endl;
    std::cout << "ctrl-c on xborder, or pressing the X window button will also stop the xborder program" << std::endl;

    struct sigaction action;
    action.sa_handler = &handle_signal;
    action.sa_flags = SA_RESTART;
    sigfillset(&action.sa_mask);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int screen = DefaultScreen(display);

    Window child_window = find_terminal(display);
    std::cout << "Child window " << child_window << std::endl;

    int child_x, child_y;
    XWindowAttributes child_attributes;
    XGetWindowAttributes(display, child_window, &child_attributes);
    Window child;
    XTranslateCoordinates(display, child_window, child_attributes.root, 0, 0, &child_x, &child_y, &child);
    // std::cout << "Window at " << child_x << ", " << child_y << std::endl;

    int border_size = 3;

    window = XCreateSimpleWindow(display, RootWindow(display, screen), child_x, child_y, child_attributes.width + border_size * 2, child_attributes.height + border_size * 2, 1, BlackPixel(display, screen), start_color(display).pixel);
    XSelectInput(display, window,
                 ExposureMask |
                 FocusChangeMask |
                 SubstructureNotifyMask |
                 StructureNotifyMask);
    XMapWindow(display, window);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    XReparentWindow(display, child_window, window, border_size, border_size);
    XMoveWindow(display, window, child_x, child_y);
    XSelectInput(display, child_window, KeyPressMask | KeyReleaseMask);

    bool shift_pressed = false;
    bool alt_pressed = false;

    Window option_window = 0;
    int palette_start = 20;
    std::string window_title = "XBorder";

    GC graphics;
    XStoreName(display, window, window_title.c_str());

    while (1){
        if (quit_now){
            std::cout << "Bye!" << std::endl;
            break;
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

        if (event.xany.window == option_window){
            bool redraw = false;
            if (event.type == ClientMessage){
                if ((Atom) event.xclient.data.l[0] == wm_delete_window){
                    XUnmapWindow(display, option_window);
                    XDestroyWindow(display, option_window);
                    option_window = 0;
                }
            } else if (event.type == Expose){
                redraw = true;
            } else if (event.type == ButtonPress){
                int x = event.xbutton.x;
                int y = event.xbutton.y;
                if (y >= palette_start && y < palette_start + (palette_y + 1) * palette_size_block &&
                    x >= 0 && x < (palette_x + 1) * palette_size_block){

                    change_background_color(display, window, get_rgb(x / palette_size_block, (y - palette_start) / palette_size_block));
                }
            } else if (event.type == KeyPress){
                KeySym sym = XLookupKeysym(&event.xkey, 0);
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
                XStoreName(display, window, window_title.c_str());
            }

            if (redraw){
                std::string total;
                total += "Title: ";
                total += window_title;
                XGCValues values;
                values.foreground = WhitePixel(display, screen);
                GC local = XCreateGC(display, option_window, GCForeground, &values);
                int width, height;
                get_window_dimensions(display, option_window, &width, &height);
                // XFillRectangle(display, option_window, local, 0, 0, width, height);
                XClearWindow(display, option_window);
                XDrawString(display, option_window, graphics, 1, 10, total.c_str(), total.size());
                XFreeGC(display, local);

                draw_palette(display, option_window, palette_start);
            }
        } else if (event.xany.window == window){
            if (event.type == Expose){
                /* anything to do here? the child already seems to get expose events */
            } else if (event.type == FocusIn){
                if (child_window != 0){
                    XSetInputFocus(display, child_window, RevertToPointerRoot, CurrentTime);
                }
            } else if (event.type == ClientMessage){
                if ((Atom) event.xclient.data.l[0] == wm_delete_window){
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
        } else if (event.xany.window == child_window){
            // std::cout << "key event in child window" << std::endl;
            if (event.type == KeyPress){
                KeySym sym = XLookupKeysym(&event.xkey, 0);

                if (sym == XK_Shift_L){
                    // std::cout << "Pressed left shift" << std::endl;
                    shift_pressed = true;
                }

                if (sym == XK_Alt_L){
                    // std::cout << "Pressed left alt" << std::endl;
                    alt_pressed = true;
                }

                if (shift_pressed && sym == XK_Shift_R){
                    break;
                }

                if (shift_pressed && alt_pressed){
                    if (option_window != 0){
                        XRaiseWindow(display, option_window);
                        XSetInputFocus(display, window, RevertToParent, CurrentTime);
                    } else {
                        // std::cout << "magic" << std::endl;
                        option_window = XCreateSimpleWindow(display, RootWindow(display, screen), 1, 1, (palette_x + 1) * palette_size_block + 5, 20 + palette_start + (palette_y + 1) * palette_size_block, 1, BlackPixel(display, screen), WhitePixel(display, screen));
                        XFontStruct * fontInfo = XLoadQueryFont(display, "6x10");
                        XGCValues values;
                        // values.font = fontInfo->fid;
                        values.foreground = BlackPixel(display, screen);
                        graphics = XCreateGC(display, option_window, GCForeground, &values);
                        XSelectInput(display, option_window,
                                     ExposureMask |
                                     KeyPress |
                                     KeyRelease |
                                     ButtonPressMask |
                                     StructureNotifyMask);
                        XMapWindow(display, option_window);
                        XStoreName(display, option_window, "XBorder options");
                        XSetWMProtocols(display, option_window, &wm_delete_window, 1);
                        XSetInputFocus(display, window, RevertToParent, CurrentTime);
                    }
                }

                /*
                std::cout << "Pressed key " << sym << std::endl;
                if (event.xkey.keycode == 0x09){
                    break;
                }


                */
            } else if (event.type == KeyRelease){
                // std::cout << "Release key" << std::endl;
                KeySym sym = XLookupKeysym(&event.xkey, 0);
                if (sym == XK_Shift_L){
                    shift_pressed = false;
                    // std::cout << "shift " << shift_pressed << std::endl;
                }
                if (sym == XK_Alt_L){
                    alt_pressed = false;
                    // std::cout << "alt " << alt_pressed << std::endl;
                }
            }
        }
    }

    if (child_window != 0){
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
