#include <X11/Xlib.h>
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
    std::cout << "yellow color " << yellow_color << std::endl;

    XWindowAttributes child_attributes;
    XGetWindowAttributes(display, use_terminal, &child_attributes);

    int border_size = 3;

    window = XCreateSimpleWindow(display, RootWindow(display, screen), child_attributes.x, child_attributes.y, child_attributes.width + border_size * 2, child_attributes.height + border_size * 2, 1, BlackPixel(display, screen), yellow.pixel);
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    XReparentWindow(display, use_terminal, window, border_size, border_size);

    while (1){
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose){
            /*
            XFillRectangle(display, window, DefaultGC(display, screen), 20, 20, 10, 10);
            XDrawString(display, window, DefaultGC(display, screen), 10, 50, "hi", strlen("hi"));
            */
        } else if (event.type == KeyPress){
            break;
        }
    }

    XReparentWindow(display, use_terminal, RootWindow(display, screen), child_attributes.x, child_attributes.y);
    XCloseDisplay(display);
}
