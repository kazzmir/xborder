#include <X11/Xlib.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <iostream>
#include <stdlib.h>
#include <string.h>

int main(){
    Display * display;
    Window window;

    display = XOpenDisplay(NULL);
    if (display == NULL){
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);

    int use_terminal = 0x38d34c0;

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

    XReparentWindow(display, use_terminal, window, border_size, border_size);
    XMoveWindow(display, window, child_x, child_y);

    XStoreName(display, window, "XBorder");

    bool shift_pressed = false;
    bool alt_pressed = false;
    while (1){
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose){
            /* anything to do here? the child already seems to get expose events */
        } else if (event.type == ConfigureNotify){
            XWindowAttributes self;
            XGetWindowAttributes(display, window, &self);
            XResizeWindow(display, use_terminal, self.width - border_size * 2, self.height - border_size * 2);
        } else if (event.type == KeyPress){
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
                std::cout << "magic" << std::endl;
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
            }
            if (sym == XK_Alt_L){
                alt_pressed = false;
            }
        }
    }

    Window child_root = child_attributes.root;
    /* Place the window wherever the container is now */
    XGetWindowAttributes(display, window, &child_attributes);
    XReparentWindow(display, use_terminal, child_root, child_attributes.x, child_attributes.y);
    XTranslateCoordinates(display, window, child_attributes.root, 0, 0, &child_x, &child_y, &child);
    XMoveWindow(display, use_terminal, child_x, child_y - border_size * 2);
    XCloseDisplay(display);
}
