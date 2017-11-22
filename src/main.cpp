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

    std::cout << "Grabbing window" << std::endl;
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

int main(){
    Display * display;
    Window window;

    display = XOpenDisplay(NULL);
    if (display == NULL){
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);

    Window use_terminal = find_terminal(display);

    XColor yellow;
    memset(&yellow, 0, sizeof(XColor));
    yellow.flags = DoRed | DoGreen | DoBlue;
    yellow.red = 65535;
    yellow.green = 65535;
    yellow.blue = 0;
    Colormap screen_colormap = DefaultColormap(display, screen);
    int yellow_color = XAllocColor(display, screen_colormap, &yellow);
    // std::cout << "yellow color " << yellow_color << std::endl;

    int child_x, child_y;
    XWindowAttributes child_attributes;
    XGetWindowAttributes(display, use_terminal, &child_attributes);
    Window child;
    XTranslateCoordinates(display, use_terminal, child_attributes.root, 0, 0, &child_x, &child_y, &child);
    std::cout << "Window at " << child_x << ", " << child_y << std::endl;

    int border_size = 3;

    window = XCreateSimpleWindow(display, RootWindow(display, screen), child_x, child_y, child_attributes.width + border_size * 2, child_attributes.height + border_size * 2, 1, BlackPixel(display, screen), yellow.pixel);
    XSelectInput(display, window,
                 ExposureMask |
                 StructureNotifyMask);
    XSelectInput(display, use_terminal, KeyPressMask | KeyReleaseMask);
    XMapWindow(display, window);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    XReparentWindow(display, use_terminal, window, border_size, border_size);
    XMoveWindow(display, window, child_x, child_y);

    bool shift_pressed = false;
    bool alt_pressed = false;

    Window option_window = 0;
    std::string window_title = "XBorder";

    GC graphics;
    XStoreName(display, window, window_title.c_str());

    while (1){
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
            } else if (event.type == KeyPress){
                KeySym sym = XLookupKeysym(&event.xkey, 0);
                if (sym == XK_BackSpace){
                    redraw = true;
                    if (window_title.length() > 0){
                        window_title = window_title.substr(0, window_title.size() - 1);
                    }
                } else {
                    char * ascii = XKeysymToString(sym);
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
                char total[512];
                sprintf(total, "Title: %s", window_title.c_str());
                XGCValues values;
                values.foreground = WhitePixel(display, screen);
                GC local = XCreateGC(display, option_window, GCForeground, &values);
                int width, height;
                get_window_dimensions(display, option_window, &width, &height);
                XFillRectangle(display, option_window, local, 0, 0, width, height);
                XDrawString(display, option_window, graphics, 1, 10, total, strlen(total));
                XFreeGC(display, local);
            }
        } else if (event.xany.window == window){
            if (event.type == Expose){
                /* anything to do here? the child already seems to get expose events */
            } else if (event.type == ClientMessage){
                if ((Atom) event.xclient.data.l[0] == wm_delete_window){
                    break;
                }
            } else if (event.type == ConfigureNotify){
                XWindowAttributes self;
                XGetWindowAttributes(display, window, &self);
                XResizeWindow(display, use_terminal, self.width - border_size * 2, self.height - border_size * 2);
            }
        } else if (event.xany.window == use_terminal){
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

                if (shift_pressed && sym == XK_Tab){
                    break;
                }

                if (shift_pressed && alt_pressed){
                    if (option_window != 0){
                        XRaiseWindow(display, option_window);
                        XSetInputFocus(display, window, RevertToParent, CurrentTime);
                    } else {
                        std::cout << "magic" << std::endl;
                        option_window = XCreateSimpleWindow(display, RootWindow(display, screen), 1, 1, 100, 100, 1, BlackPixel(display, screen), WhitePixel(display, screen));
                        XFontStruct * fontInfo = XLoadQueryFont(display, "6x10");
                        XGCValues values;
                        // values.font = fontInfo->fid;
                        values.foreground = BlackPixel(display, screen);
                        graphics = XCreateGC(display, option_window, GCForeground, &values);
                        XSelectInput(display, option_window,
                                     ExposureMask |
                                     KeyPress |
                                     KeyRelease |
                                     StructureNotifyMask);
                        XMapWindow(display, option_window);
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

    Window child_root = child_attributes.root;
    /* Place the window wherever the container is now */
    XGetWindowAttributes(display, window, &child_attributes);
    XReparentWindow(display, use_terminal, child_root, child_attributes.x, child_attributes.y);
    XTranslateCoordinates(display, window, child_attributes.root, 0, 0, &child_x, &child_y, &child);
    XDestroyWindow(display, window);
    XMoveWindow(display, use_terminal, child_x, child_y - border_size * 2);
    XCloseDisplay(display);
}
