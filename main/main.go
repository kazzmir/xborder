package main

import (
    "github.com/BurntSushi/xgb/xproto"
    "github.com/BurntSushi/xgbutil"
    "github.com/BurntSushi/xgbutil/xwindow"
    "github.com/BurntSushi/xgbutil/xevent"
    "github.com/BurntSushi/xgbutil/xcursor"
    "github.com/BurntSushi/xgbutil/mousebind"
    "github.com/BurntSushi/xgbutil/icccm"
    "log"
    "time"
    "fmt"
)

func findClientWindow(X *xgbutil.XUtil, window xproto.Window) xproto.Window {
    _, err := icccm.WmStateGet(X, window)
    if err == nil {
        return window
    }

    tree, err := xproto.QueryTree(X.Conn(), window).Reply()
    if err != nil {
        return 0
    }

    for _, try := range tree.Children {
        use := findClientWindow(X, try)
        if use != 0 {
            return use
        }
    }

    return 0
}

func chooseWindow(X *xgbutil.XUtil, root *xwindow.Window) xproto.Window {
    log.Printf("Click on a window")

    cursor, err := xcursor.CreateCursor(X, xcursor.Crosshair)
    if err != nil {
        log.Printf("Could not create cursor", err)
        return 0
    }

    pressed := make(chan xproto.Window, 1)
    err = mousebind.ButtonPressFun(
        func (X *xgbutil.XUtil, event xevent.ButtonPressEvent){
            log.Printf("Button pressed on window root=0x%x event=0x%x child=0x%x", event.Root, event.Event, event.Child)
            /*
            err := xproto.AllowEventsChecked(X.Conn(), xproto.AllowReplayPointer, xproto.TimeCurrentTime).Check()
            if err != nil {
                log.Printf("Could not allow events: %v", err)
            }
            */

            pressed <- event.Child
    }).Connect(X, root.Id, "1", false, false)

    if err != nil {
        log.Printf("Could not grab: %v", err)
        return 0
    }

    defer mousebind.DetachPress(X, root.Id)

    grabbed, err := mousebind.GrabPointer(X, root.Id, root.Id, cursor)
    if err != nil {
        log.Printf("Could not grab pointer: %v", err)
        return 0
    }

    log.Printf("Grabbed %v", grabbed)

    defer mousebind.UngrabPointer(X)

    pingBefore, pingAfter, pingQuit := xevent.MainPing(X)

    timeout := time.After(5 * time.Second)
    for {
        select {
            case <-pingBefore:
            case <-pingAfter:
            case <-pingQuit:
                X.Quit = false
            case window := <-pressed:
                log.Printf("Pressed a mouse button")

                xevent.Quit(X)
                for {
                    select {
                        case <-pingBefore:
                        case <-pingAfter:
                        case <-pingQuit:
                            X.Quit = false
                            return findClientWindow(X, window)
                    }
                }
            case <-timeout:
                log.Printf("Timed out")
        }
    }

    return 0
}

func rgb(red uint32, green uint32, blue uint32) uint32 {
    return (red << 16) | (green << 8) | blue
}

func xborder(X *xgbutil.XUtil, root *xwindow.Window, child *xwindow.Window) error {
    geometry, err := child.Geometry()
    if err != nil {
        return err
    }
    border, err := xwindow.Generate(X)
    if err != nil {
        return fmt.Errorf("Could not generate xborder window: %v", err)
    }
    defer border.Destroy()

    borderSize := 3

    log.Printf("Xborder window 0x%x", border.Id)

    childX := geometry.X()
    childY := geometry.Y()

    border.Create(root.Id, geometry.X(), geometry.Y(), geometry.Width() + borderSize * 2, geometry.Height() + borderSize * 2, xproto.CwBackPixel, rgb(255, 0, 0))

    border.Map()

    border.Move(childX, childY)

    /* get the original parent */
    childParent, err := child.Parent()
    if err != nil {
        log.Printf("Could not get parent of child: %v", err)
        childParent = nil
    } else {
        log.Printf("Original child parent: 0x%x", childParent.Id)
    }

    child.Change(xproto.CwOverrideRedirect, 1)
    defer child.Change(xproto.CwOverrideRedirect, 0)

    err = xproto.ReparentWindowChecked(X.Conn(), child.Id, border.Id, int16(borderSize), int16(borderSize)).Check()
    if err != nil {
        log.Printf("Unable to reparent child window into xborder window: %v", err)
    }

    done := make(chan bool, 1)

    border.Listen(xproto.EventMaskExposure,
                  xproto.EventMaskFocusChange,
                  xproto.EventMaskSubstructureNotify,
                  xproto.EventMaskStructureNotify)

    // Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);
    // XSetWMProtocols(display, window, &wm_delete_window, 1);
    // icccm.WmProtocolsSet(X, border.Id, []string{"WM_DELETE_WINDOW"})
    border.WMGracefulClose(func (self *xwindow.Window){
        // log.Printf("Client mesage: delete? %v", icccm.IsDeleteProtocol(X, event))
        done <- true
    })

    /*
    xevent.ClientMessageFun(func (X* xgbutil.XUtil, event xevent.ClientMessageEvent){
        log.Printf("Client mesage: delete? %v", icccm.IsDeleteProtocol(X, event))
        done <- true
    }).Connect(X, border.Id)
    */

    xevent.ConfigureNotifyFun(func (X* xgbutil.XUtil, event xevent.ConfigureNotifyEvent){
        // log.Printf("Notify on xborder")
        /*
        parent, err := child.Parent()
        if err == nil {
            log.Printf("Child 0x%x parent=0x%x", child.Id, parent.Id)
            if parent.Id != border.Id {
                err = xproto.ReparentWindowChecked(X.Conn(), child.Id, border.Id, int16(borderSize), int16(borderSize)).Check()
            }
        } else {
            log.Printf("Could not get parent: %v", err)
        }
        */


        geometry, err := border.Geometry()
        if err == nil {
            child.Resize(geometry.Width() - borderSize * 2, geometry.Height() - borderSize * 2)
        } else {
            log.Printf("Could not get xborder geometry: %v", err)
        }
        // XResizeWindow(display, child_window, self.width - border_size * 2, self.height - border_size * 2);

    }).Connect(X, border.Id)

    /* process X events */
    pingBefore, pingAfter, pingQuit := xevent.MainPing(X)
    quit := false
    for !quit{
        select {
            case <-done:
                xevent.Quit(X)

                err = xproto.ReparentWindowChecked(X.Conn(), child.Id, root.Id, 0, 0).Check()
                if err != nil {
                    log.Printf("Unable to reparent child window back to root: %v", err)
                }

                geometry, err := border.Geometry()
                if err == nil {
                    log.Printf("Geometry: %v", geometry)
                    x := geometry.X()
                    y := geometry.Y()
                    log.Printf("Move child back to %v, %v", x, y)
                    child.Move(x, y)
                }
            case <-pingBefore:
                parent, err := child.Parent()
                if err == nil {
                    log.Printf("Child 0x%x parent=0x%x", child.Id, parent.Id)
                    if parent.Id != border.Id {
                        err = xproto.ReparentWindowChecked(X.Conn(), child.Id, border.Id, int16(borderSize), int16(borderSize)).Check()
                    }
                } else {
                    log.Printf("Could not get parent: %v", err)
                }
            case <-pingAfter:
            case <-pingQuit:
                quit = true
        }
    }

    /*
    if childParent != nil {
        log.Printf("Set child back to parent 0x%x", childParent.Id)
        err = xproto.ReparentWindowChecked(X.Conn(), child.Id, childParent.Id, 0, 0).Check()
        if err != nil {
            log.Printf("Unable to reparent child window back to root: %v", err)
        }
    }
    */

    // go xevent.Main(X)

    return nil
}

func main(){
    log.SetFlags(log.Ldate | log.Lshortfile | log.Lmicroseconds)
    X, err := xgbutil.NewConn()
    if err != nil {
        log.Printf("Could not connect to X server: %v", err)
        return
    }
    defer X.Conn().Close()
    log.Printf("Connected to X\n")

    mousebind.Initialize(X)

    root := xwindow.New(X, X.RootWin())
    tree, err := xproto.QueryTree(X.Conn(), root.Id).Reply()
    if err != nil {
        log.Printf("Could not query tree: %v", err)
        return
    }


    for _, window := range tree.Children {
        log.Printf("Window: 0x%x", window)
    }

    window := chooseWindow(X, root)
    log.Printf("Chose window 0x%x", window)

    if window != 0 && window != root.Id {
        log.Printf("Xborder on 0x%x", window)
        err = xborder(X, root, xwindow.New(X, window))
        if err != nil {
            log.Printf("Error: %v", err)
        }
    }

    log.Printf("Shutting down")
}
