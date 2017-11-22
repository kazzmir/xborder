xborder shows a colored border around an X window. It does this by creating a new X window with the color as a background, and then reparenting the chosen window to be a child of the border window. A child window is chosen by clicking on the window when xborder starts up, similar to how xwininfo works.

If the new combined window has input focus then the following key combinations will work:

left shift + left alt: bring up an options window that lets you change the window title and background color
left shift + right shift: end xborder, putting the child window back onto the root window

![alt "xborder example"](https://raw.githubusercontent.com/kazzmir/xborder/master/screen1.png)
