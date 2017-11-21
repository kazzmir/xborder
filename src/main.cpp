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
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 100, 100, 1, BlackPixel(display, screen), WhitePixel(display, screen));
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    while (1){
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose){
            XFillRectangle(display, window, DefaultGC(display, screen), 20, 20, 10, 10);
            XDrawString(display, window, DefaultGC(display, screen), 10, 50, "hi", strlen("hi"));
        } else if (event.type == KeyPress){
            break;
        }
    }

    XCloseDisplay(display);
}
